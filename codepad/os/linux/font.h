#pragma once

 /// \file
 /// Implementation of fonts for the linux platform.

#include <fontconfig/fontconfig.h>

#include "../font.h"

namespace codepad {
	namespace os {
		/// Font based on freetype.
		class freetype_font : public freetype_font_base {
		public:
			/// Constructor. Finds the font matching the description using fontconfig,
			/// then loads the font by passing its file name to freetype.
			freetype_font(const str_t &str, double sz, font_style style) : freetype_font_base() {
				_font_config::get().refresh();
				FcPattern *pat = FcNameParse(reinterpret_cast<const FcChar8*>(str.c_str()));
				FcPatternAddInteger(pat, FC_SLANT, test_bits_any(
					style, font_style::italic
				) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
				FcPatternAddInteger(pat, FC_WEIGHT, test_bits_any(
					style, font_style::bold
				) ? FC_WEIGHT_BOLD : FC_WEIGHT_NORMAL);
				assert_true_sys(FcConfigSubstitute(nullptr, pat, FcMatchPattern) != FcFalse, "cannot set pattern");
				FcDefaultSubstitute(pat);
				FcResult res;
				FcPattern *font = FcFontMatch(nullptr, pat, &res);
				FcPatternDestroy(pat);
				assert_true_sys(font != nullptr, "cannot find matching font");
				FcChar8 *file;
				int index = 0;
				assert_true_sys(
					FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch, "cannot get font file name"
				);
				assert_true_sys(
					FcPatternGetInteger(font, FC_INDEX, 0, &index) == FcResultMatch, "cannot get font index"
				);
				_ft_verify(FT_New_Face(_library::get().lib, reinterpret_cast<const char*>(file), index, &_face));
				_ft_verify(FT_Set_Pixel_Sizes(_face, 0, static_cast<FT_UInt>(sz)));
				logger::get().log_info(CP_HERE, "font loaded: ", reinterpret_cast<const char*>(file), ":", index);
				FcPatternDestroy(font);

				_cache_kerning();
			}
			/// Destructor. Calls \p FT_Done_Face on the loaded font.
			~freetype_font() override {
				logger::get().log_verbose(CP_HERE, "font disposed");
				_ft_verify(FT_Done_Face(_face));
			}
		protected:
			/// Auxiliary struct used to load the fontconfig library.
			struct _font_config {
				/// Constructor. Calls \p FcInitLoadConfigAndFonts() to initialize fontconfig.
				_font_config() {
					assert_true_sys(FcInit() == FcTrue, "failed to initialize fontconfig");
				}
				/// Destructor. Destroys the loaded config and calls \p FcFini().
				///
				/// \todo `Assertion fcCacheChains[i] == NULL failed'.
				~_font_config() {
					FcFini(); // TODO right here
				}

				/// Refreshes the configuration.
				void refresh() {
					assert_true_sys(FcInitBringUptoDate() == FcTrue, "cannot refresh font library");
				}

				/// Returns the global \ref _font_config object.
				static _font_config &get();
			};
		};
		/// Uses \ref freetype_font by default.
		using default_font = freetype_font;
	}
}
