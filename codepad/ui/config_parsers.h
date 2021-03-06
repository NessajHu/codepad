// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Parsers of JSON configuration files.

#include <vector>

#include "hotkey_registry.h"
#include "element_classes.h"
#include "element.h"

namespace codepad::ui {
	class manager;

	/// Parses visuals from JSON objects.
	template <typename ValueType> class arrangements_parser {
	public:
		using value_t = ValueType; ///< The type that holds JSON values.
		using object_t = typename ValueType::object_type; ///< The type that holds JSON objects.
		using array_t = typename ValueType::array_type; ///< The type that holds JSON arrays.

		/// Initializes the class with the given \ref manager.
		explicit arrangements_parser(manager &man) : _manager(man) {
		}

		/// Parses a dictionary of colors.
		void parse_color_scheme(const object_t &val, std::map<str_t, colord, std::less<>> &scheme) {
			for (auto it = val.member_begin(); it != val.member_end(); ++it) {
				if (auto color = it.value().template parse<colord>()) {
					scheme.emplace(it.name(), color.value());
				}
			}
		}

		/// Parses a \ref element_configuration from the given JSON object, and adds it to \p value. If one for the
		/// specified states already exists in \p value, it is kept if the inheritance is not overriden with
		/// \p inherit_from.
		void parse_configuration(const object_t &val, element_configuration &value) {
			parse_parameters(val, value.default_parameters);

			if (auto extraobj = val.template parse_optional_member<object_t>(u8"extras")) {
				for (auto it = extraobj->member_begin(); it != extraobj->member_end(); ++it) {
					value.additional_attributes.emplace(it.name(), json::store(it.value()));
				}
			}

			if (auto ani_from = val.template parse_optional_member<str_view_t>(u8"inherit_animations_from")) {
				if (auto *ancestor = get_manager().get_class_arrangements().get(ani_from.value())) {
					value.event_triggers = ancestor->configuration.event_triggers;
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid animation inheritance";
				}
			}
			if (auto animations = val.template parse_optional_member<object_t>(u8"animations")) {
				for (auto sbit = animations->member_begin(); sbit != animations->member_end(); ++sbit) {
					if (auto ani_obj = sbit.value().template cast<object_t>()) {
						element_configuration::event_trigger trigger;
						trigger.identifier = element_configuration::event_identifier::parse_from_string(sbit.name());
						for (auto aniit = ani_obj->member_begin(); aniit != ani_obj->member_end(); ++aniit) {
							animation_path::component_list components;
							auto res = animation_path::parser::parse(aniit.name(), components);
							if (res != animation_path::parser::result::completed) {
								aniit.value().template log<log_level::error>(CP_HERE) <<
									"failed to segment animation path, skipping";
								continue;
							}
							element_configuration::animation_parameters aniparams;
							aniparams.subject = std::move(components);
							if (auto ani = aniit.value().template parse<generic_keyframe_animation_definition>(
								managed_json_parser<generic_keyframe_animation_definition>(get_manager())
								)) {
								aniparams.definition = ani.value();
							} else {
								continue;
							}
							trigger.animations.emplace_back(std::move(aniparams));
						}
						value.event_triggers.emplace_back(std::move(trigger));
					}
				}
			}
		}
		/// Parses a \ref element_parameters from the given JSON object.
		void parse_parameters(const object_t &val, element_parameters &value) {
			if (auto layout_from = val.template parse_optional_member<str_view_t>(u8"inherit_layout_from")) {
				if (auto *ancestor = get_manager().get_class_arrangements().get(layout_from.value())) {
					value.layout_parameters = ancestor->configuration.default_parameters.layout_parameters;
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid layout inheritance";
				}
			}
			if (auto layout = val.template parse_optional_member<element_layout>(u8"layout")) {
				value.layout_parameters = layout.value();
			}

			if (auto visuals_from = val.template parse_optional_member<str_view_t>(u8"inherit_visuals_from")) {
				if (auto *ancestor = get_manager().get_class_arrangements().get(visuals_from.value())) {
					value.visual_parameters = ancestor->configuration.default_parameters.visual_parameters;
				} else {
					val.template log<log_level::error>(CP_HERE) << "invalid visual inheritance";
				}
			}
			if (auto vis = val.template parse_optional_member<visuals>(
				u8"visuals", managed_json_parser<visuals>(get_manager())
				)) {
				value.visual_parameters = std::move(vis.value());
			}

			value.element_visibility =
				val.template parse_optional_member<visibility>(u8"visibility").value_or(value.element_visibility);
			value.custom_cursor =
				val.template parse_optional_member<cursor>(u8"cursor").value_or(value.custom_cursor);
		}
		/// Parses additional attributes of a \ref class_arrangements::child from the given JSON object.
		void parse_additional_arrangement_attributes(
			const object_t &val, class_arrangements::child &child
		) {
			if (auto type = val.template parse_member<str_view_t>(u8"type")) {
				child.type = type.value();
				child.element_class = val.template parse_optional_member<str_view_t>(u8"class").value_or(child.type);
				child.name = val.template parse_optional_member<str_view_t>(u8"name").value_or(child.name);
			}
		}
		/// Parses the metrics and children arrangements of either a composite element or one of its children, given
		/// a JSON object.
		template <typename T> void parse_class_arrangements(const object_t &val, T &obj) {
			parse_configuration(val, obj.configuration);
			if (auto children = val.template parse_optional_member<array_t>(u8"children")) {
				for (auto &&elem : children.value()) {
					if (auto child = elem.template cast<object_t>()) {
						class_arrangements::child ch;
						parse_additional_arrangement_attributes(child.value(), ch);
						if (auto *cls = get_manager().get_class_arrangements().get(ch.element_class)) {
							// provide default values for its element configuration
							// TODO animations may be invalidated by additional parsing
							ch.configuration = cls->configuration;
						}
						parse_class_arrangements(child.value(), ch);
						obj.children.emplace_back(std::move(ch));
					}
				}
			}
		}
		/// Parses the whole set of arrangements for \ref _manager from the given JSON object. The original list is
		/// not emptied so configs can be appended to one another.
		void parse_arrangements_config(const object_t &val) {
			for (auto i = val.member_begin(); i != val.member_end(); ++i) {
				if (auto obj = i.value().template cast<object_t>()) {
					class_arrangements arr;
					if (auto name = obj->template parse_optional_member<str_view_t>(u8"name")) {
						arr.name = str_t(name.value());
					}
					parse_class_arrangements(obj.value(), arr);
					auto [it, inserted] = get_manager().get_class_arrangements().mapping.emplace(
						i.name(), std::move(arr)
					);
					if (!inserted) {
						logger::get().log_warning(CP_HERE) << "duplicate class arrangements";
					}
				}
			}
		}

		/// Returns the associated \ref manager.
		manager &get_manager() const {
			return _manager;
		}
	protected:
		manager &_manager; ///< The \ref manager associated with this parser.
	};

	/// Parses hotkeys from JSON objects.
	template <typename ValueType> class hotkey_json_parser {
	public:
		constexpr static char key_delim = '+'; ///< The delimiter for keys.

		using value_t = ValueType; ///< The type for JSON values.
		using object_t = typename ValueType::object_type; ///< The type for JSON objects.
		using array_t = typename ValueType::array_type; ///< The type for JSON arrays.

		/// Parses a JSON object for a list of \ref key_gesture "key_gestures" and the corresponding command.
		inline static bool parse_hotkey_entry(
			std::vector<key_gesture> &gests, str_t &cmd, const object_t &obj
		) {
			if (auto command = obj.template parse_member<str_view_t>(u8"command")) {
				cmd = command.value();
			} else {
				return false;
			}
			if (auto gestures = obj.find_member(u8"gestures"); gestures != obj.member_end()) {
				if (auto gstr = gestures.value().template try_cast<str_view_t>()) {
					if (auto gestval = key_gesture::parse(gstr.value())) {
						gests.emplace_back(gestval.value());
					} else {
						return false;
					}
				} else if (auto garr = gestures.value().template try_cast<array_t>()) {
					for (auto &&g : garr.value()) {
						if (auto gpart = g.template try_cast<str_view_t>()) {
							if (auto gestval = key_gesture::parse(gpart.value())) {
								gests.emplace_back(gestval.value());
							}
						}
					}
				} else {
					return false;
				}
			} else {
				return false;
			}
			return true;
		}
		/// Parses an \ref class_hotkey_group from an JSON array.
		inline static bool parse_class_hotkey(class_hotkey_group &gp, const array_t &arr) {
			for (auto &&cls : arr) {
				if (auto obj = cls.template try_cast<object_t>()) {
					std::vector<key_gesture> gs;
					str_t name;
					if (parse_hotkey_entry(gs, name, obj.value())) {
						gp.register_hotkey(gs, std::move(name));
					} else {
						logger::get().log_warning(CP_HERE) << "invalid hotkey entry";
					}
				}
			}
			return true;
		}
		/// Parses a set of \ref class_hotkey_group "element_hotkey_groups" from a given JSON object.
		template <typename Map> inline static void parse_config(
			Map &mapping, const object_t &obj
		) {
			for (auto i = obj.member_begin(); i != obj.member_end(); ++i) {
				class_hotkey_group gp;
				if (auto arr = i.value().template try_cast<array_t>()) {
					if (parse_class_hotkey(gp, arr.value())) {
						mapping.emplace(i.name(), std::move(gp));
					}
				}
			}
		}
	};
}
