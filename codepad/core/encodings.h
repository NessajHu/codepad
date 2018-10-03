#pragma once

/// \file
/// Encoding settings, and conversions between one another.
/// Currently supported encodings: UTF-8, UTF-16, UTF-32.
///
/// \todo Better handling of decoding errors.

#include <string>
#include <iterator>

#include <rapidjson/document.h>

namespace codepad {
	/// Specifies the byte order of words.
	enum class endianness : unsigned char {
		little_endian, ///< Little endian.
		big_endian ///< Big endian.
	};
	/// The endianness of the current system.
	constexpr endianness system_endianness = endianness::little_endian;

	/// Default definition for string literals.
#define CP_STRLIT(X) u8 ## X
	/// STL string with default character type.
	using str_t = std::basic_string<char>;
	/// Type used to store codepoints. \p char32_t is not used because its range is 0~0x10FFFF which may not be able
	/// to correctly represent invalid codepoints.
	using codepoint = std::uint32_t;

	constexpr codepoint
		replacement_character = 0xFFFD, ///< Unicode replacement character.
		invalid_min = 0xD800, ///< Minimum code point value reserved by UTF-16.
		invalid_max = 0xDFFF, ///< Maximum code point value (inclusive) reserved by UTF-16.
		unicode_max = 0x10FFFF; ///< Maximum code point value (inclusive) of Unicode.

	/// A template version of \p std::strlen().
	template <typename Char> inline size_t get_unit_count(const Char *cs) {
		size_t i = 0;
		for (; *cs; ++i, ++cs) {
		}
		return i;
	}

	/// Determines if a codepoint is a `new line' character.
	inline bool is_newline(codepoint c) {
		return c == '\n' || c == '\r';
	}
	/// Determines if a codepoint is a graphical char, i.e., is not blank.
	///
	/// \todo May not be complete.
	inline bool is_graphical_char(codepoint c) {
		return c != '\n' && c != '\r' && c != '\t' && c != ' ';
	}
	/// Determines if a codepoint lies in the valid range of Unicode points.
	inline bool is_valid_codepoint(codepoint c) {
		return c < invalid_min || (c > invalid_max && c <= unicode_max);
	}

	/// Implementation of various encodings. All implementations accept only byte sequences as input.
	namespace encodings {
		/// UTF-8 encoding.
		///
		/// \sa https://en.wikipedia.org/wiki/UTF-8.
		class utf8 {
		public:
			constexpr static std::byte
				mask_1{0x80}, ///< Mask for detecting single-byte codepoints.
				sig_1{0x00}, ///< Expected masked value of single-byte codepoints.
				mask_2{0xE0}, ///< Mask for detecting bytes leading double-byte codepoints.
				sig_2{0xC0}, ///< Expected masked value of bytes leading double-byte codepoints.
				mask_3{0xF0}, ///< Mask for detecting triple-byte codepoints.
				sig_3{0xE0}, ///< Expected masked value of bytes leading triple-byte codepoints.
				mask_4{0xF8}, ///< Mask for detecting quadruple-byte codepoints.
				sig_4{0xF0}, ///< Expected masked value of bytes leading quadruple-byte codepoints.
				mask_cont{0xC0}, ///< Mask for detecting continuation bytes.
				sig_cont{0x80}; ///< Expected masked value of continuation bytes.

			/// Returns `UTF-8'.
			inline static str_t get_name() {
				return CP_STRLIT("UTF-8");
			}

			/// Moves the iterator to the next codepoint, extracting the current codepoint to \p v, and returns whether
			/// it is valid. The caller is responsible of determining if <tt>i == end</tt>. If the codepoint is not
			/// valid, \p v will contain the byte that \p i initially points to, and \p i will be moved to point to the
			/// next byte.
			template <typename It1, typename It2> inline static bool next_codepoint(
				It1 &i, const It2 &end, codepoint &v
			) {
				std::byte fb = *i;
				if ((fb & mask_1) == sig_1) {
					v = static_cast<codepoint>(fb & ~mask_1);
				} else if ((fb & mask_2) == sig_2) {
					if (++i == end || (*i & mask_cont) != sig_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask_2) << 6;
					v |= static_cast<codepoint>(*i & ~mask_cont);
				} else if ((fb & mask_3) == sig_3) {
					if (++i == end || (*i & mask_cont) != sig_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask_3) << 12;
					v |= static_cast<codepoint>(*i & ~mask_cont) << 6;
					if (++i == end || (*i & mask_cont) != sig_cont) {
						--i;
						v = static_cast<codepoint>(fb);
						return false;
					}
					v |= static_cast<codepoint>(*i & ~mask_cont);
				} else if ((fb & mask_4) == sig_4) {
					if (++i == end || (*i & mask_cont) != sig_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask_4) << 18;
					v |= static_cast<codepoint>(*i & ~mask_cont) << 12;
					if (++i == end || (*i & mask_cont) != sig_cont) {
						--i;
						v = static_cast<codepoint>(fb);
						return false;
					}
					v |= static_cast<codepoint>(*i & mask_cont) << 6;
					if (++i == end || (*i & mask_cont) != sig_cont) {
						--i;
						--i;
						v = static_cast<codepoint>(fb);
						return false;
					}
					v |= static_cast<codepoint>(*i & ~mask_cont);
				} else {
					++i;
					return false;
				}
				++i;
				return true;
			}
			/// Moves the iterator to the next codepoint and returns whether it is valid. The caller is responsible
			/// of determining if <tt>i == end</tt>. If the codepoint is not valid, \p v will contain the byte that
			/// \p i initially points to, and \p i will be moved to point to the next byte.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end) {
				std::byte fb = *i;
				if ((fb & mask_1) != sig_1) {
					if ((fb & mask_2) == sig_2) {
						if (++i == end || (*i & mask_cont) != sig_cont) {
							return false;
						}
					} else if ((fb & mask_3) == sig_3) {
						if (++i == end || (*i & mask_cont) != sig_cont) {
							return false;
						}
						if (++i == end || (*i & mask_cont) != sig_cont) {
							--i;
							return false;
						}
					} else if ((fb & mask_4) == sig_4) {
						if (++i == end || (*i & mask_cont) != sig_cont) {
							return false;
						}
						if (++i == end || (*i & mask_cont) != sig_cont) {
							--i;
							return false;
						}
						if (++i == end || (*i & mask_cont) != sig_cont) {
							--i;
							--i;
							return false;
						}
					} else {
						++i;
						return false;
					}
				}
				++i;
				return true;
			}
			/// Returns the UTF-8 representation of a Unicode codepoint.
			inline static std::basic_string<std::byte> encode_codepoint(codepoint c) {
				if (c < 0x80) {
					return {static_cast<std::byte>(c) & ~mask_1};
				} else if (c < 0x800) {
					return {
						(static_cast<std::byte>(c >> 6) & ~mask_2) | sig_2,
						(static_cast<std::byte>(c) & ~mask_cont) | sig_cont
					};
				} else if (c < 0x10000) {
					return {
						(static_cast<std::byte>(c >> 12) & ~mask_3) | sig_3,
						(static_cast<std::byte>(c >> 6) & ~mask_cont) | sig_cont,
						(static_cast<std::byte>(c) & ~mask_cont) | sig_cont
					};
				} else {
					return {
						(static_cast<std::byte>(c >> 18) & ~mask_4) | sig_4,
						(static_cast<std::byte>(c >> 12) & ~mask_cont) | sig_cont,
						(static_cast<std::byte>(c >> 6) & ~mask_cont) | sig_cont,
						(static_cast<std::byte>(c) & ~mask_cont) | sig_cont
					};
				}
			}
		};

		/// UTF-16 encoding.
		template <endianness Endianness = system_endianness> class utf16 {
		public:
			/// Returns either `UTF-16 LE' or `UTF-16 BE', depending on the Endianness.
			inline static str_t get_name() {
				if constexpr (Endianness == endianness::little_endian) {
					return CP_STRLIT("UTF-16 LE");
				} else {
					return CP_STRLIT("UTF-16 BE");
				}
			}

			/// Moves the iterator to the next codepoint, extracting the current codepoint,
			/// and returns whether it is valid. The caller is responsible of determining if <tt>i == end</tt>.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end, codepoint &v) {
				std::uint16_t word;
				if (!_extract_word(i, end, word)) {
					v = word;
					return false;
				}
				if ((word & 0xDC00) == 0xD800) {
					if (i == end) {
						v = word;
						return false;
					}
					std::uint16_t w2;
					if (!_extract_word(i, end, w2)) {
						--i;
						v = word;
						return false;
					}
					if ((w2 & 0xDC00) != 0xDC00) {
						--i;
						--i;
						v = word;
						return false;
					}
					v = (static_cast<codepoint>(word & 0x03FF) << 10) | (w2 & 0x03FF);
				} else {
					v = word;
					if ((word & 0xDC00) == 0xDC00) {
						return false;
					}
				}
				return true;
			}
			/// Moves the iterator to the next codepoint and returns whether it is valid.
			/// The caller is responsible of determining if <tt>i == end</tt>.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end) {
				std::uint16_t word;
				if (!_extract_word(i, end, word)) {
					return false;
				}
				if ((word & 0xDC00) == 0xD800) {
					if (i == end) {
						return false;
					}
					std::uint16_t w2;
					if (!_extract_word(i, end, w2)) {
						--i;
						return false;
					}
					if ((w2 & 0xDC00) != 0xDC00) {
						--i;
						--i;
						return false;
					}
				} else {
					if ((word & 0xDC00) == 0xDC00) {
						return false;
					}
				}
				return true;
			}
			/// Returns the UTF-16 representation of a Unicode codepoint.
			inline static std::basic_string<std::byte> encode_codepoint(codepoint c) {
				if (c < 0x10000) {
					return _encode_word(static_cast<std::uint16_t>(c));
				} else {
					codepoint mined = c - 0x10000;
					return
						_encode_word(static_cast<std::uint16_t>((mined >> 10) | 0xD800)) +
						_encode_word(static_cast<std::uint16_t>((mined & 0x03FF) | 0xDC00));
				}
			}
		protected:
			/// Extracts a two-byte word from the given range of bytes, with the specified endianness.
			///
			/// \return A boolean indicating whether it was successfully extracted. This operation fails only if
			///         there are not enough bytes.
			template <typename It1, typename It2> inline static bool _extract_word(
				It1 &i, It2 end, std::uint16_t &word
			) {
				std::byte b1 = static_cast<std::byte>(*i);
				if (++i == end) {
					word = static_cast<std::uint16_t>(b1);
					return false;
				}
				std::byte b2 = static_cast<std::byte>(*i);
				++i;
				if constexpr (Endianness == endianness::little_endian) {
					word = static_cast<std::uint16_t>(b1) | (static_cast<std::uint16_t>(b2) << 8);
				} else {
					word = static_cast<std::uint16_t>(b2) | (static_cast<std::uint16_t>(b1) << 8);
				}
				return true;
			}
			/// Rearranges the two bytes of the given word according to the current endianness.
			inline static std::basic_string<std::byte> _encode_word(std::uint16_t word) {
				if constexpr (Endianness == endianness::little_endian) {
					return {
						static_cast<std::byte>(word & 0xFF),
						static_cast<std::byte>(word >> 8)
					};
				} else {
					return {
						static_cast<std::byte>(word >> 8),
						static_cast<std::byte>(word & 0xFF)
					};
				}
			}
		};

		/// UTF-32 encoding.
		///
		/// \tparam C Type of characters.
		template <typename C = codepoint> class utf32 {
		public:
			using char_type = C; ///< The character type.

			static_assert(sizeof(C) >= 3, "invalid character type for utf-32");
			/// Moves the iterator to the next codepoint, extracting the current codepoint,
			/// and returns whether it is valid. The caller is responsible of determining if <tt>i == end</tt>.
			///
			/// \param i The `current' iterator.
			/// \param end The end of the string.
			/// \param v The value will hold the value of the current codepoint if the function returns \p true.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end, codepoint &v) {
				static_cast<void>(end);
				v = static_cast<codepoint>(*i);
				++i;
				return is_valid_codepoint(v);
			}
			/// Moves the iterator to the next codepoint and returns whether it is valid.
			/// The caller is responsible of determining if <tt>i == end</tt>.
			///
			/// \param i The `current' iterator.
			/// \param end The end of the string.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end) {
				bool res = is_valid_codepoint(*i);
				++i;
				return res;
			}
			/// next_codepoint(It1, It2) without error checking.
			/// Also, the caller doesn't need to check if <tt>i == end</tt>.
			template <typename It1, typename It2> inline static void next_codepoint_rough(It1 &i, It2 end) {
				if (i != end) {
					++i;
				}
			}
			/// Go back to the previous codepoint. Note that the result is only an estimate.
			///
			/// \param i The `current' iterator.
			/// \param beg The beginning of the string.
			template <typename It1, typename It2> inline static void previous_codepoint_rough(It1 &i, It2 beg) {
				if (i != beg) {
					--i;
				}
			}
			/// Returns the UTF-32 representation of a Unicode codepoint.
			inline static std::basic_string<C> encode_codepoint(codepoint c) {
				return std::u32string(1, c);
			}

			/// Counts the number of codepoints in the given range.
			/// Uses the distance between the two iterators if possible, otherwise falls back to the default behavior.
			template <typename It1, typename It2> inline static size_t count_codepoints(It1 beg, It2 end) {
				if constexpr (std::is_same_v<It1, It2>) {
					return std::distance(beg, end);
				} else {
					return codepad::count_codepoints<It1, It2, codepad::utf32>(beg, end);
				}
			}
			/// Skips an iterator forward, until the end is reached or a number of codepoints is skipped.
			/// Directly increments the iterator if possible, otherwise falls back to the default behavior.
			template <typename It1, typename It2> inline static size_t skip_codepoints(It1 &beg, It2 end, size_t num) {
				if constexpr (std::is_same_v<It1, It2> && std::is_base_of_v<
					std::random_access_iterator_tag, typename std::iterator_traits<It1>::iterator_category
				>) {
					auto dist = std::min(num, static_cast<size_t>(end - beg));
					beg = beg + num;
					return dist;
				} else {
					return codepad::skip_codepoints<It1, It2, codepad::utf32>(beg, end, num);
				}
			}
		};
	}

	/// Settings and utilities of RapidJSON library.
	namespace json {
		// encoding settings
		/// Default encoding used by RapidJSON.
		using encoding = rapidjson::UTF8<char>;

		/// RapidJSON type that holds a JSON object.
		using value_t = rapidjson::GenericValue<encoding>;
		/// RapidJSON type that holds a JSON object, and can be used to parse JSON text.
		using parser_value_t = rapidjson::GenericDocument<encoding>;

		/// Returns a STL string for a JSON string object. The caller is responsible of checking if the object is
		/// actually a string.
		///
		/// \param v A JSON object. The caller must guarantee that the object is a string.
		/// \return A STL string whose content is the same as the JSON object.
		/// \remark This is the preferred way to get a string object's contents
		///         since it may contain null characters.
		inline str_t get_as_string(const value_t &v) {
			return str_t(v.GetString(), v.GetStringLength());
		}

		namespace _details {
			template <typename Res, typename Obj, bool(Obj::*TypeVerify)() const, Res(Obj::*Getter)() const> inline bool try_get(
				const Obj &v, const str_t &s, Res &ret
			) {
				auto found = v.FindMember(s.c_str());
				if (found != v.MemberEnd() && (found->value.*TypeVerify)()) {
					ret = (found->value.*Getter)();
					return true;
				}
				return false;
			}
		}
		/// Checks if the object has a member with a certain name which is of type \p T,
		/// then returns the value of the member if there is.
		///
		/// \param val A JSON object.
		/// \param name The desired name of the member.
		/// \param ret If the function returns true, then \p ret holds the value of the member.
		/// \return \p true if it has a member of type \p T.
		template <typename T> bool try_get(const value_t &val, const str_t &name, T &ret);
		template <> inline bool try_get<bool>(const value_t &val, const str_t &name, bool &ret) {
			return _details::try_get<bool, value_t, &value_t::IsBool, &value_t::GetBool>(val, name, ret);
		}
		template <> inline bool try_get<double>(const value_t &val, const str_t &name, double &ret) {
			return _details::try_get<double, value_t, &value_t::IsNumber, &value_t::GetDouble>(val, name, ret);
		}
		template <> inline bool try_get<str_t>(const value_t &val, const str_t &name, str_t &ret) {
			auto found = val.FindMember(name.c_str());
			if (found != val.MemberEnd() && found->value.IsString()) {
				ret = get_as_string(found->value);
				return true;
			}
			return false;
		}

		/// \ref try_get with a default value.
		///
		/// \param v A JSON object.
		/// \param s The desired name of the member.
		/// \param def The default value if no such member is found.
		/// \return The value if one exists, or the default otherwise.
		template <typename T> inline T get_or_default(const value_t &v, const str_t &s, const T &def) {
			T res;
			if (try_get(v, s, res)) {
				return res;
			}
			return def;
		}
	}
}
