#include "element.h"

/// \file
/// Implementation of certain methods related to ui::element.

#include "../os/window.h"
#include "panel.h"
#include "manager.h"

using namespace codepad::os;

namespace codepad::ui {
	void element::invalidate_layout() {
		manager::get().invalidate_layout(*this);
	}

	void element::revalidate_layout() {
		manager::get().revalidate_layout(*this);
	}

	void element::invalidate_visual() {
		manager::get().invalidate_visual(*this);
	}

	void element::_on_mouse_down(mouse_button_info &p) {
		if (p.button == input::mouse_button::primary) {
			if (_can_focus && !p.focus_set()) {
				p.mark_focus_set();
				get_window()->set_window_focused_element(*this);
			}
			set_state_bits(manager::get().get_predefined_states().mouse_down, true);
		}
		mouse_down.invoke(p);
	}

	void element::_on_mouse_enter() {
		set_state_bits(manager::get().get_predefined_states().mouse_over, true);
		mouse_enter.invoke();
	}

	void element::_on_mouse_leave() {
		set_state_bits(manager::get().get_predefined_states().mouse_over, false);
		mouse_leave.invoke();
	}

	void element::_on_got_focus() {
		set_state_bits(manager::get().get_predefined_states().focused, true);
		got_focus.invoke();
	}

	void element::_on_lost_focus() {
		set_state_bits(manager::get().get_predefined_states().focused, false);
		lost_focus.invoke();
	}

	void element::_on_mouse_up(mouse_button_info &p) {
		if (p.button == os::input::mouse_button::primary) {
			set_state_bits(manager::get().get_predefined_states().mouse_down, false);
		}
		mouse_up.invoke(p);
	}

	void element::_on_update() {
		if (!_config.all_stationary()) {
			if (!_config.visual_config.get_state().all_stationary) {
				invalidate_visual();
			}
			if (!_config.metrics_config.get_state().all_stationary) {
				invalidate_layout();
			}
			if (!_config.update(manager::get().update_delta_time())) {
				manager::get().schedule_update(*this);
			}
		}
	}

	void element::_on_render() {
		if (is_visible()) {
			_on_prerender();
			_config.visual_config.render(get_layout());
			_custom_render();
			_on_postrender();
		}
	}

	void element::_on_state_changed(value_update_info<element_state_id>&) {
		manager::get().schedule_update(*this);
	}

	void element::_recalc_horizontal_layout(double xmin, double xmax) {
		anchor anc = get_anchor();
		thickness margin = get_margin();
		auto wprop = get_layout_width();
		_layout.xmin = xmin;
		_layout.xmax = xmax;
		layout_on_direction(
			test_bits_all(anc, anchor::left), wprop.second, test_bits_all(anc, anchor::right),
			_layout.xmin, _layout.xmax, margin.left, wprop.first, margin.right
		);
	}

	void element::_recalc_vertical_layout(double ymin, double ymax) {
		anchor anc = get_anchor();
		thickness margin = get_margin();
		auto hprop = get_layout_height();
		_layout.ymin = ymin;
		_layout.ymax = ymax;
		layout_on_direction(
			test_bits_all(anc, anchor::top), hprop.second, test_bits_all(anc, anchor::bottom),
			_layout.ymin, _layout.ymax, margin.top, hprop.first, margin.bottom
		);
	}

	void element::_initialize(const str_t &cls, const element_metrics &metrics) {
#ifdef CP_CHECK_USAGE_ERRORS
		_initialized = true;
#endif
		_config.visual_config = visual_configuration(
			manager::get().get_class_visuals().get_visual_or_default(cls), _state
		);
		_config.metrics_config = metrics_configuration(metrics, _state);
		_config.hotkey_config = manager::get().get_class_hotkeys().try_get(cls);
	}

	void element::_dispose() {
		if (_parent) {
			_parent->_children.remove(*this);
		}
#ifdef CP_CHECK_USAGE_ERRORS
		_initialized = false;
#endif
	}

	void element::set_zindex(int v) {
		if (_parent) {
			_parent->_children.set_zindex(*this, v);
		} else {
			_zindex = v;
		}
	}

	window_base *element::get_window() {
		element *cur = this;
		while (cur->_parent != nullptr) {
			cur = cur->_parent;
		}
		return dynamic_cast<window_base*>(cur);
	}

	bool element::is_mouse_over() const {
		return test_bits_all(_state, manager::get().get_predefined_states().mouse_over);
	}

	bool element::is_visible() const {
		return !test_bits_all(_state, manager::get().get_predefined_states().invisible);
	}

	void element::set_visibility(bool val) {
		set_state_bits(manager::get().get_predefined_states().invisible, !val);
	}

	bool element::is_interactive() const {
		return !test_bits_all(_state, manager::get().get_predefined_states().ghost);
	}

	void element::set_is_interactive(bool val) {
		set_state_bits(manager::get().get_predefined_states().ghost, !val);
	}

	bool element::is_focused() const {
		return test_bits_all(_state, manager::get().get_predefined_states().focused);
	}

	bool element::is_vertical() const {
		return test_bits_all(_state, manager::get().get_predefined_states().vertical);
	}

	void element::set_is_vertical(bool v) {
		set_state_bits(manager::get().get_predefined_states().vertical, v);
	}


	void decoration::_on_visual_changed() {
		if (_wnd) {
			_wnd->invalidate_visual();
		}
	}

	decoration::~decoration() {
		if (_wnd) {
			_wnd->_on_decoration_destroyed(*this);
		}
	}

	void decoration::set_class(const str_t &cls) {
		_class = cls;
		_vis_config = visual_configuration(manager::get().get_class_visuals().get_visual_or_default(_class), _state);
		_on_visual_changed();
	}
}
