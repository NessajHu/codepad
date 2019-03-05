// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Abstraction of modes of interaction that applies to a variety of kinds of editors.

#include "../ui/element.h"
#include "../ui/manager.h"
#include "caret_set.h"
#include "editor.h"

namespace codepad::editors {
	template <typename> class interaction_manager;
	template <typename> class interaction_mode;

	/// A \ref contents_region_base that exposes caret information for \ref interaction_manager.
	template <typename CaretSet> class interactive_contents_region_base : public contents_region_base {
		friend interaction_mode<CaretSet>;
	public:
		/// Returns the set of carets.
		virtual const CaretSet &get_carets() const = 0;
		/// Removes the given caret.
		virtual void remove_caret(typename CaretSet::const_iterator) = 0;
		/// Returns the part of the \p CaretSet::entry that corresponds to a \ref caret-selection_position.
		virtual caret_selection_position extract_caret_selection_position(const typename CaretSet::entry&) const = 0;
	protected:
		/// Called when temporary carets have been changed.
		virtual void _on_temporary_carets_changed() = 0;
	};

	/// Virtual base class of different interaction modes. This class receive certain events, to which it must return
	/// a \p bool indicating whether this mode is still in effect.
	template <typename CaretSet> class interaction_mode {
	public:
		using manager_t = interaction_manager<CaretSet>; ///< The corresponding \ref interaction_manager type.

		/// Default virtual constructor.
		virtual ~interaction_mode() = default;

		/// Called when a mouse button has been pressed.
		virtual bool on_mouse_down(manager_t&, ui::mouse_button_info&) {
			return true;
		}
		/// Called when a mouse button has been released.
		virtual bool on_mouse_up(manager_t&, ui::mouse_button_info&) {
			return true;
		}
		/// Called when the mouse has been moved.
		virtual bool on_mouse_move(manager_t&, ui::mouse_move_info&) {
			return true;
		}
		/// Called when the mouse capture has been lost.
		virtual bool on_capture_lost(manager_t&) {
			return true;
		}
		/// Called when the element is being updated.
		virtual bool on_update(manager_t&) {
			return true;
		}

		/// Called when an edit operation is about to take place, where the carets will likely be used.
		virtual bool on_edit_operation(manager_t&) {
			return true;
		}
		/// Called when the viewport of the contents region has changed.
		virtual bool on_viewport_changed(manager_t&) {
			return true;
		}

		/// Returns the override cursor in this mode. If the cursor is not overriden, return
		/// \ref ui::cursor::not_specified.
		virtual ui::cursor get_override_cursor(const manager_t&) const {
			return ui::cursor::not_specified;
		}

		/// Returns a list of temporary carets that should be rendered but are not actually in effect.
		virtual std::vector<caret_selection_position> get_temporary_carets(manager_t&) const = 0;
	protected:
		/// Notify the contents region by calling
		/// \ref interactive_contents_region_base::_on_temporary_carets_changed().
		void _on_temporary_carets_changed(manager_t&);
	};
	/// Virtual base class that controls the activation of \ref interaction_mode "interaction_modes". This class
	/// receives certain events, to which it can either return \p nullptr, meaning that this mode is not activated,
	/// or a pointer to an newly-created object of the corresponding \ref interaction_mode. If an
	/// \ref interaction_mode is active, these will not receive any events. Derived classes should overload at least
	/// one operator.
	template <typename CaretSet> class interaction_mode_activator {
	public:
		using manager_t = interaction_manager<CaretSet>; ///< The corresponding \ref interaction_manager type.
		using mode_t = interaction_mode<CaretSet>; ///< The corresponding \ref interaction_mode type.

		/// Default virtual constructor.
		virtual ~interaction_mode_activator() = default;

		/// Called when a mouse button has been pressed. Returns \p nullptr by default.
		virtual std::unique_ptr<mode_t> on_mouse_down(manager_t&, ui::mouse_button_info&) {
			return nullptr;
		}
		/// Called when a mouse button has been released. Returns \p nullptr by default.
		virtual std::unique_ptr<mode_t> on_mouse_up(manager_t&, ui::mouse_button_info&) {
			return nullptr;
		}
		/// Called when the mouse has been moved. Returns \p nullptr by default.
		virtual std::unique_ptr<mode_t> on_mouse_move(manager_t&, ui::mouse_move_info&) {
			return nullptr;
		}
		/// Called when the mouse capture has been lost. Returns \p nullptr by default.
		virtual std::unique_ptr<mode_t> on_capture_lost(manager_t&) {
			return nullptr;
		}

		/// Returns the override cursor in this mode. If the cursor is not overriden, return
		/// \ref ui::cursor::not_specified. If a mode does override the cursor, all following modes will be ignored.
		virtual ui::cursor get_override_cursor(const manager_t&) const {
			return ui::cursor::not_specified;
		}
	};

	/// Manages a list of \ref interaction_mode "interaction_modes". At any time at most one mode can be active, to
	/// which all interaction events will be forwarded.
	template <typename CaretSet> class interaction_manager {
	public:
		using mode_t = interaction_mode<CaretSet>; ///< The corresponding \ref interaction_mode type.
		/// The corresponding \ref interaction_mode_activator type.
		using mode_activator_t = interaction_mode_activator<CaretSet>;

		/// Returns a reference to \ref _activators.
		std::vector<mode_activator_t*> &activators() {
			return _activators;
		}
		/// \overload
		const std::vector<mode_activator_t*> &activators() const {
			return _activators;
		}

		/// Returns \ref _cached_position.
		caret_position get_mouse_position() const {
			return _cached_position;
		}

		/// Sets the \ref contents_region_proxy for this \ref interaction_manager.
		void set_contents_region(interactive_contents_region_base<CaretSet> &rgn) {
			_contents_region = &rgn;
		}
		/// Returns \ref _contents_region.
		interactive_contents_region_base<CaretSet> &get_contents_region() const {
			return *_contents_region;
		}

		/// Returns a list of temporary carets.
		///
		/// \sa interaction_mode::get_temporary_carets()
		std::vector<caret_selection_position> get_temporary_carets() {
			if (_active) {
				return _active->get_temporary_carets(*this);
			}
			return {};
		}

		/// Called when a mouse button has been pressed.
		void on_mouse_down(ui::mouse_button_info &info) {
			_update_cached_positions(info.position - _contents_region->get_layout().xmin_ymin());
			_dispatch_event<&mode_activator_t::on_mouse_down, &mode_t::on_mouse_down>(info);
		}
		/// Called when a mouse button has been released.
		void on_mouse_up(ui::mouse_button_info &info) {
			_update_cached_positions(info.position - _contents_region->get_layout().xmin_ymin());
			_dispatch_event<&mode_activator_t::on_mouse_up, &mode_t::on_mouse_up>(info);
		}
		/// Called when the mouse has been moved.
		void on_mouse_move(ui::mouse_move_info &info) {
			_update_cached_positions(info.new_position - _contents_region->get_layout().xmin_ymin());
			_dispatch_event<&mode_activator_t::on_mouse_move, &mode_t::on_mouse_move>(info);
		}
		/// Called when the mouse capture has been lost.
		void on_capture_lost() {
			_dispatch_event<&mode_activator_t::on_capture_lost, &mode_t::on_capture_lost>();
		}
		/// Called when the element is being updated.
		void on_update() {
			_dispatch_event<nullptr, &mode_t::on_update>();
		}

		/// Called when an edit operation is about to take place, where the carets will likely be used.
		void on_edit_operation() {
			_dispatch_event<nullptr, &mode_t::on_edit_operation>();
		}
		/// Called when the viewport of the contents region has changed.
		void on_viewport_changed() {
			_cached_position = _contents_region->hit_test_for_caret(_cached_mouse_position);
			_dispatch_event<nullptr, &mode_t::on_viewport_changed>();
		}

		/// Returns the overriden cursor. Returns \p ui::cursor::not_specified when the cursor is not overriden. If
		/// there's an active \ref interaction_mode, then it decides the overriden cursor; otherwise the cursor is
		/// decided by the list of \ref interaction_mode_activator "interaction_mode_activators".
		ui::cursor get_override_cursor() const {
			if (_active) {
				return _active->get_override_cursor(*this);
			}
			for (const mode_activator_t *activator : _activators) {
				ui::cursor c = activator->get_override_cursor(*this);
				if (c != ui::cursor::not_specified) {
					return c;
				}
			}
			return ui::cursor::not_specified;
		}
	protected:
		std::vector<mode_activator_t*> _activators; ///< The list of activators.
		/// The cached mouse position relative to the corresponding \ref ui::element, used when none is supplied.
		vec2d _cached_mouse_position;
		caret_position _cached_position; ///< The cached mouse position.
		std::unique_ptr<mode_t> _active; ///< The currently active \ref interaction_mode.
		interactive_contents_region_base<CaretSet> *_contents_region = nullptr; ///< The \ref contents_region_base.

		/// Dispatches an event. If \ref _active is \p nullptr, then the \ref ActivatorPtr of each entry in
		/// \ref _activators will be called. Otherwise, \ref ModePtr of \ref _active will be called, and \ref _active
		/// will be disposed if necessary.
		template <auto ActivatorPtr, auto ModePtr, typename ...Args> void _dispatch_event(Args &&...args) {
			if (_active) {
				if (!(_active.get()->*ModePtr)(*this, std::forward<Args>(args)...)) { // deactivated
					_active.reset();
				}
			} else {
				if constexpr (static_cast<bool>(ActivatorPtr)) {
					for (mode_activator_t *act : _activators) {
						if (auto ptr = (act->*ActivatorPtr)(*this, std::forward<Args>(args)...)) { // activated
							_active = std::move(ptr);
							break;
						}
					}
				}
			}
		}
		/// Updates \ref _cached_mouse_position and \ref _cached_position with the given mouse position.
		void _update_cached_positions(vec2d pos) {
			_cached_mouse_position = pos;
			_cached_position = _contents_region->hit_test_for_caret(_cached_mouse_position);
		}
	};

	template <typename CaretSet> void interaction_mode<CaretSet>::_on_temporary_carets_changed(manager_t &man) {
		man.get_contents_region()._on_temporary_carets_changed();
	}


	/// Contains several built-in interaction modes.
	namespace interaction_modes {
		/// A mode where the user can scroll the viewport by moving the mouse near or out of boundaries. This is
		/// intended to be used as a base class for other interaction modes.
		template <typename CaretSet> class mouse_nagivation_mode : public interaction_mode<CaretSet> {
		public:
			using typename interaction_mode<CaretSet>::manager_t;

			constexpr static double default_padding_value = 50.0; ///< The default value of \ref _padding.

			/// Updates \ref _speed. This function always returns \p true.
			bool on_mouse_move(manager_t &man, ui::mouse_move_info &info) override {
				contents_region_base &elem = man.get_contents_region();
				rectd r = ui::thickness(_padding).shrink(elem.get_layout());
				r.make_valid_average();
				// find anchor point
				_scrolling = false;
				vec2d anchor = info.new_position;
				if (anchor.x < r.xmin) {
					anchor.x = r.xmin;
					_scrolling = true;
				} else if (anchor.x > r.xmax) {
					anchor.x = r.xmax;
					_scrolling = true;
				}
				if (anchor.y < r.ymin) {
					anchor.y = r.ymin;
					_scrolling = true;
				} else if (anchor.y > r.ymax) {
					anchor.y = r.ymax;
					_scrolling = true;
				}
				// calculate speed
				_speed = info.new_position - anchor; // TODO further manipulate _speed
				if (_scrolling) { // schedule update
					elem.get_manager().get_scheduler().schedule_element_update(elem);
				}
				return true;
			}
			/// Updates the position of the contents region. This function always returns \p true.
			bool on_update(manager_t &man) override {
				if (_scrolling) {
					contents_region_base &contents = man.get_contents_region();
					auto *edt = editor::get_encapsulating(contents);
					edt->set_position(
						edt->get_position() + _speed * contents.get_manager().get_scheduler().update_delta_time()
					);
					contents.get_manager().get_scheduler().schedule_element_update(contents);
				}
				return true;
			}
		protected:
			vec2d _speed; ///< The speed of scrolling.
			/// The inner padding. This allows the screen to start scrolling even if the mouse is still inside the
			/// \ref ui::element.
			double _padding = default_padding_value;
			bool _scrolling = false; ///< Whether the viewport is currently scrolling.
		};

		/// The mode that allows the user to edit a single selected region using the mouse.
		template <typename CaretSet> class mouse_single_selection_mode : public mouse_nagivation_mode<CaretSet> {
		public:
			using typename mouse_nagivation_mode<CaretSet>::manager_t;

			/// How existing carets will be handled.
			enum class mode {
				single, ///< Existing carets will be cleared.
				multiple, ///< Existing carets will be preserved.
				extend ///< One of the existing carets will be edited.
			};

			/// Acquires mouse capture and initializes the caret with the given value.
			mouse_single_selection_mode(
				manager_t &man, ui::mouse_button trig, caret_selection_position initial_value
			) : mouse_nagivation_mode<CaretSet>(), _selection(initial_value), _trigger_button(trig) {
				contents_region_base &elem = man.get_contents_region();
				elem.get_window()->set_mouse_capture(elem);
			}
			/// Delegate constructor that initializes the caret with the mouse position.
			mouse_single_selection_mode(manager_t &man, ui::mouse_button trig) :
				mouse_single_selection_mode(man, trig, man.get_mouse_position()) {
			}

			/// Updates the caret.
			bool on_mouse_move(manager_t &man, ui::mouse_move_info &info) override {
				mouse_nagivation_mode<CaretSet>::on_mouse_move(man, info);
				if (man.get_mouse_position() != _selection.get_caret_position()) {
					_selection.set_caret_position(man.get_mouse_position());
					interaction_mode<CaretSet>::_on_temporary_carets_changed(man);
				}
				return true;
			}
			/// Updates the caret.
			bool on_viewport_changed(manager_t &man) override {
				_selection.set_caret_position(man.get_mouse_position());
				return true;
			}
			/// Releases capture and exits this mode if \ref _trigger_button is released.
			bool on_mouse_up(manager_t &man, ui::mouse_button_info &info) override {
				if (info.button == _trigger_button) {
					_exit(man, true);
					return false;
				}
				return true;
			}
			/// Exits this mode.
			bool on_capture_lost(manager_t &man) override {
				_exit(man, false);
				return false;
			}
			/// Exits this mode.
			bool on_edit_operation(manager_t &man) override {
				_exit(man, true);
				return false;
			}

			/// Returns \ref _selection.
			std::vector<caret_selection_position> get_temporary_carets(manager_t&) const override {
				return {_selection};
			}
		protected:
			caret_selection_position _selection; ///< The selection being edited.
			/// The mouse button that triggered this mode.
			ui::mouse_button _trigger_button = ui::mouse_button::primary;

			/// Called when about to exit the mode. Adds the caret to the caret set. If \p release_capture is
			/// \p true, then \ref ui::window_base::release_mouse_capture() will also be called.
			void _exit(manager_t &man, bool release_capture) {
				man.get_contents_region().add_caret(_selection);
				if (release_capture) {
					man.get_contents_region().get_window()->release_mouse_capture();
				}
			}
		};
		/// Triggers \ref mouse_selection_mode.
		template <typename CaretSet> class mouse_single_selection_mode_activator :
			public interaction_mode_activator<CaretSet> {
		public:
			using typename interaction_mode_activator<CaretSet>::manager_t;
			using typename interaction_mode_activator<CaretSet>::mode_t;

			std::unique_ptr<mode_t> on_mouse_down(
				manager_t &man, ui::mouse_button_info &info
			) override {
				if (info.button == edit_button && info.modifiers == edit_modifiers) {
					// select a caret to be edited
					auto it = man.get_contents_region().get_carets().carets.begin(); // TODO select a better caret
					if (it == man.get_contents_region().get_carets().carets.end()) { // should not happen
						logger::get().log_warning(CP_HERE, "empty caret set when starting mouse interaction");
						return nullptr;
					}
					caret_selection_position sel = man.get_contents_region().extract_caret_selection_position(*it);
					man.get_contents_region().remove_caret(it);
					return std::make_unique<mouse_single_selection_mode<CaretSet>>(man, edit_button, sel);
				} else if (info.button == multiple_select_button && info.modifiers == multiple_select_modifiers) {
					return std::make_unique<mouse_single_selection_mode<CaretSet>>(man, multiple_select_button);
				} else if (info.button == ui::mouse_button::primary) {
					man.get_contents_region().clear_carets();
					return std::make_unique<mouse_single_selection_mode<CaretSet>>(man, ui::mouse_button::primary);
				}
				return nullptr;
			}

			ui::mouse_button
				/// The mouse button used for multiple selection.
				multiple_select_button = ui::mouse_button::primary,
				/// The mouse button used for editing existing selected regions.
				edit_button = ui::mouse_button::primary;
			ui::modifier_keys
				/// The modifier keys for multiple selection.
				multiple_select_modifiers = ui::modifier_keys::control,
				/// The modifiers for editing existing selected regions.
				edit_modifiers = ui::modifier_keys::shift;
		};

		/// Mode where the user is about to start dragging text with the mouse.
		template <typename CaretSet> class mouse_prepare_drag_mode : public interaction_mode<CaretSet> {
		public:
			using typename interaction_mode<CaretSet>::manager_t;

			/// Initializes \ref _init_pos and acquires mouse capture.
			mouse_prepare_drag_mode(manager_t &man, vec2d pos) : interaction_mode<CaretSet>(), _init_pos(pos) {
				man.get_contents_region().get_window()->set_mouse_capture(man.get_contents_region());
			}

			/// Exit on mouse up.
			bool on_mouse_up(manager_t &man, ui::mouse_button_info&) override {
				man.get_contents_region().get_window()->release_mouse_capture();
				// TODO replace caret
				return false;
			}
			/// Checks the position of the mouse, and starts drag drop and exits if the distance is enough.
			bool on_mouse_move(manager_t &man, ui::mouse_move_info &info) override {
				if ((info.new_position - _init_pos).length_sqr() > 25.0) { // TODO magic value
					logger::get().log_info(CP_HERE, "start drag drop");
					// TODO start
					man.get_contents_region().get_window()->release_mouse_capture();
					return false;
				}
				return true;
			}
			/// Exit on capture lost.
			bool on_capture_lost(manager_t&) override {
				return false;
			}

			/// Exit on edit operations.
			bool on_edit_operation(manager_t &man) override {
				man.get_contents_region().get_window()->release_mouse_capture();
				return false;
			}

			/// Returns \ref ui::cursor::normal.
			ui::cursor get_override_cursor(const manager_t&) const override {
				return ui::cursor::normal;
			}

			/// No temporary carets.
			std::vector<caret_selection_position> get_temporary_carets(manager_t&) const override {
				return {};
			}
		protected:
			vec2d _init_pos; ///< The initial position of the mouse.
		};
		/// Triggers \ref mouse_prepare_drag_mode.
		template <typename CaretSet> class mouse_prepare_drag_mode_activator :
			public interaction_mode_activator<CaretSet> {
		public:
			using typename interaction_mode_activator<CaretSet>::manager_t;
			using typename interaction_mode_activator<CaretSet>::mode_t;

			/// Activates a \ref mouse_prepare_drag_mode if the mouse is in a selected region.
			std::unique_ptr<mode_t> on_mouse_down(manager_t &man, ui::mouse_button_info &info) override {
				if (man.get_contents_region().get_carets().is_in_selection(man.get_mouse_position().position)) {
					return std::make_unique<mouse_prepare_drag_mode<CaretSet>>(man, info.position);
				}
				return nullptr;
			}

			/// Returns the override cursor in this mode. If the cursor is not overriden, return
			/// \ref ui::cursor::not_specified. If a mode does override the cursor, all following modes will be
			/// ignored.
			virtual ui::cursor get_override_cursor(const manager_t &man) const override {
				if (man.get_contents_region().get_carets().is_in_selection(man.get_mouse_position().position)) {
					return ui::cursor::normal;
				}
				return interaction_mode_activator<CaretSet>::get_override_cursor(man);
			}
		};
	}
}