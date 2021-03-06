// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Plugin manager.

#ifdef CP_ENABLE_PLUGINS

#	include <map>
#	include <memory>
#	include <filesystem>

#	include "misc.h"
#	include "assert.h"
#	include "logging.h"
#	include "../apigen_definitions.h"
#	include "../os/dynamic_library.h"

struct CP_API_STRUCT;

namespace codepad {
	class plugin_manager;

	/// Abstract interface for plugins.
	class plugin {
		friend plugin_manager;
	public:
		/// Default virtual destructor.
		virtual ~plugin() = default;

		/// Attaches this plugin to the given \ref plugin_manager and loads the plugin. During the lifetime of this
		/// plugin, this function will be called exactly once.
		virtual void initialize(plugin_manager&) = 0;

		/// Returns the name of this plugin. Call this only after this plugin has been initialized.
		virtual str_t get_name() const = 0;

		/// Enables this plugin.
		virtual void enable() {
			assert_true_logical(!_enabled, "calling enable() on a plugin that is already enabled");
			_enabled = true;
		}
		/// Disables this plugin.
		virtual void disable() {
			assert_true_logical(_enabled, "calling enable() on a plugin that is already disabled");
			_enabled = false;
		}
		/// Returns whether this plugin is currently enabled.
		bool is_enabled() const {
			return _enabled;
		}
	private:
		bool _enabled = false; ///< Whether this plugin is currently enabled.
	};
	class native_plugin;

	/// Plugin manager.
	class APIGEN_EXPORT_RECURSIVE plugin_manager {
		friend native_plugin;
	public:
		/// Initializes \ref _api_table.
		plugin_manager();
		/// Deletes \ref _api_table.
		~plugin_manager();
		plugin_manager(const plugin_manager&) = delete; // HACK apigen
		plugin_manager &operator=(const plugin_manager&) = delete; // HACK apigen

		/// Tries to find the plugin with the given name.
		plugin *find(str_view_t name) const {
			if (auto it = _plugins.find(name); it != _plugins.end()) {
				return it->second.get();
			}
			return nullptr;
		}
		/// Attaches the given plugin to this manager and initialize it. If a plugin with the given name already
		/// exists, the plugin in the argument will be destroyed, and the existing plugin will be returned.
		plugin &attach(std::unique_ptr<plugin> p) {
			p->initialize(*this);
			auto [it, inserted] = _plugins.try_emplace(p->get_name(), std::move(p));
			if (!inserted) {
				// TODO decide whether to replace the old plugin
				logger::get().log_warning(CP_HERE) <<
					"plugin " << it->first << " already exists, the newly created one is destroyed";
			}
			return *it->second;
		}

		/// Returns the global plugin manager.
		static plugin_manager &get();
	protected:
		std::map<str_t, std::unique_ptr<plugin>, std::less<>> _plugins; ///< All loaded plugins.
		/// The table of API function pointers. This is a raw pointer because we don't include apigen host header in
		/// this file, thus \p CP_API_STRUCT is an incomplete type.
		CP_API_STRUCT *_api_table;
	};

	/// A dynamic library plugin. This is the most basic type of plugin that supports all other types of plugins.
	class APIGEN_EXPORT_RECURSIVE native_plugin : public plugin {
	public:
		/// Function pointer type used to initialize the plugin.
		using initialize_func_t = void (*)(const CP_API_STRUCT*);
		using get_name_func_t = const char *(*)(); ///< Function pointer used to retrieve the name of a plugin.
		using enable_func_t = void (*)(); ///< Function pointer used to enable the plugin.
		using disable_func_t = void (*)(); ///< Function pointer used to disable the plugin.

		/// Loads the dynamic library. It's recommended to call \ref valid() afterwards to check if it has been
		/// successfully loaded.
		explicit native_plugin(const std::filesystem::path &path) : _lib(path) {
			if (_lib.valid()) {
				_init = _lib.find_symbol<initialize_func_t>("initialize");
				_get_name = _lib.find_symbol<get_name_func_t>("get_name");
				_enable = _lib.find_symbol<enable_func_t>("enable");
				_disable = _lib.find_symbol<disable_func_t>("disable");
			}
		}

		/// Returns whether the dynamic library has been successfully loaded and all required symbols have been
		/// found.
		bool valid() const {
			return
				_lib.valid() &&
				_init != nullptr && _get_name != nullptr &&
				_enable != nullptr && _disable != nullptr;
		}
		/// Logs the reason why this plugin is not valid.
		void diagnose() const {
			if (!_lib.valid()) {
				logger::get().log_warning(CP_HERE) << "failed to load dynamic library";
			} else {
				if (_init == nullptr) {
					logger::get().log_warning(CP_HERE) << "initialize() symbol not found in dynamic library";
				}
				if (_get_name == nullptr) {
					logger::get().log_warning(CP_HERE) << "get_name() symbol not found in dynamic library";
				}
				if (_enable == nullptr) {
					logger::get().log_warning(CP_HERE) << "enable() symbol not found in dynamic library";
				}
				if (_disable == nullptr) {
					logger::get().log_warning(CP_HERE) << "disable() symbol not found in dynamic library";
				}
			}
		}

		/// Invokes \ref _init.
		void initialize(plugin_manager&) override;
		/// Invokes \ref _get_name.
		str_t get_name() const override {
			return _get_name();
		}
		/// Invokes \ref _enable.
		void enable() override {
			plugin::enable();
			_enable();
		}
		/// Invokes \ref _disable.
		void disable() override {
			plugin::disable();
			_disable();
		}
	protected:
		os::dynamic_library _lib; ///< Reference to the dynamic library.
		initialize_func_t _init = nullptr; ///< The initialize function.
		get_name_func_t _get_name = nullptr; ///< Used to get the name of the plugin.
		enable_func_t _enable = nullptr; ///< Used to enable the plugin.
		disable_func_t _disable = nullptr; ///< Used to disable the plugin.
	};
}

#endif
