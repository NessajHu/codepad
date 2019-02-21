// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Definition of certain global objects and functions,
/// and auxiliary structs to log their construction and destruction.

#include <stack>
#include <string>

#include "misc.h"
#include "../os/current/all.h"
#include "../ui/commands.h"
#include "../ui/common_elements.h"
#include "../ui/renderer.h"
#include "../ui/manager.h"
#include "../ui/element_classes.h"
#include "../ui/font_family.h"
#include "../ui/native_commands.h"
#include "../ui/visual.h"
#include "../editors/tabs.h"
#include "../editors/buffer_manager.h"
#include "../editors/code/components.h"
#include "../editors/code/contents_region.h"
#include "../editors/code/document_formatting_cache.h"

using namespace std;

using namespace codepad::os;
using namespace codepad::ui;
using namespace codepad::editors;
using namespace codepad::editors::code;

namespace codepad {
	double minimap::_target_height = 2.0;

	/*std::optional<document_formatting_cache::_in_effect_params> document_formatting_cache::_eff;*/


	chrono::high_resolution_clock::time_point get_app_epoch() {
		static chrono::high_resolution_clock::time_point _epoch = chrono::high_resolution_clock::now();
		return _epoch;
	}
	chrono::duration<double> get_uptime() {
		return chrono::duration<double>(chrono::high_resolution_clock::now() - get_app_epoch());
	}


	void initialize(int argc, char **argv) {
		os::initialize(argc, argv);

		get_app_epoch(); // initialize epoch

		/*document_formatting_cache::enable();*/
	}


	/// Type names of initializing global objects are pushed onto this stack to make dependency clear.
	stack<string> _global_init_stk;
	string _cur_global_dispose; ///< Type name of the global object that's currently being destructed.

	/// Wrapper struct for a global variable.
	/// Logs the beginning and ending of the creation and destruction of the underlying object,
	///
	/// \tparam T The type of the global variable.
	template <typename T> struct _global_wrapper {
	public:
		/// Constructor. Logs the object's construction.
		/// The beginning of the object's construction is logged by \ref _marker.
		/// This function then logs the ending, and pops the type name from \ref _global_init_stk.
		///
		/// \param args Arguments that are forwarded to the construction of the underlying object.
		template <typename ...Args> explicit _global_wrapper(Args &&...args) : object(forward<Args>(args)...) {
			// logging is not performed for logger since it may lead to recursive initialization
			if constexpr (!is_same_v<T, logger>) {
				logger::get().log_info(
					CP_HERE,
					string((_global_init_stk.size() - 1) * 2, ' '), // indent
					"finish init: ", _global_init_stk.top()
				);
			}
			_global_init_stk.pop();
		}
		/// Desturctor. Logs the object's destruction.
		~_global_wrapper() {
			assert_true_logical(_cur_global_dispose.length() == 0, "nested disposal of global objects");
			_cur_global_dispose = demangle(typeid(T).name());
		}

	protected:
		/// Helper struct used to log the beginning of the object's construction and the object's destruction.
		struct _init_marker {
			/// Constructor. Logs the beginning of the object's construction and
			/// pushes the type name onto \ref _global_init_stk.
			_init_marker() {
				string tname = demangle(typeid(T).name());
				if constexpr (!is_same_v<T, logger>) { // logging is not performed for logger
					logger::get().log_info(
						CP_HERE, string(_global_init_stk.size() * 2, ' '),
						"begin init: ", tname
					);
				}
				_global_init_stk.emplace(move(tname));
			}
			/// Destructor. Logs when the object has been destructed.
			~_init_marker() {
				if constexpr (!is_same_v<T, logger>) { // logging is not performed for logger
					logger::get().log_info(CP_HERE, "disposed: ", _cur_global_dispose);
				}
				_cur_global_dispose.clear();
			}
		};

		/// Helper object. Put before \ref object so that this is initialized before,
		/// and disposed after, \ref object.
		_init_marker _marker;
	public:
		T object; ///< The actual global object.
	};

	// all singleton getters
	logger &logger::get() {
		static _global_wrapper<logger> _v;
		return _v.object;
	}
	namespace os {
#ifdef CP_PLATFORM_WINDOWS
		wic_image_loader &wic_image_loader::get() {
			static _global_wrapper<wic_image_loader> _v;
			return _v.object;
		}
		window::_wndclass &window::_wndclass::get() {
			static _global_wrapper<_wndclass> _v;
			return _v.object;
		}
		window::_ime &window::_ime::get() {
			static _global_wrapper<_ime> _v;
			return _v.object;
		}
#endif
#ifdef CP_PLATFORM_UNIX
#	ifdef CP_USE_GTK
		namespace _details {
			cursor_set &cursor_set::get() {
				static _global_wrapper<cursor_set> _v;
				return _v.object;
			}
		}
#	else
		_details::xlib_link &_details::xlib_link::get() {
			static _global_wrapper<xlib_link> _v;
			return _v.object;
		}
#	endif
		freetype_font::_font_config &freetype_font::_font_config::get() {
			static _global_wrapper<_font_config> _v;
			return _v.object;
		}
#endif
		freetype_font_base::_library &freetype_font_base::_library::get() {
			static _global_wrapper<_library> _v;
			return _v.object;
		}
	}
	namespace editors {
		buffer_manager &buffer_manager::get() {
			static _global_wrapper<buffer_manager> _v;
			return _v.object;
		}
		namespace code {
			contents_region::_appearance_config &contents_region::_get_appearance() {
				static _global_wrapper<_appearance_config> _v;
				return _v.object;
			}
			encoding_manager &encoding_manager::get() {
				static _global_wrapper<encoding_manager> _v;
				return _v.object;
			}
		}
	}
}
