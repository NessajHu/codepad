// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Animation-related classes and structs.

#include <optional>

#include "misc.h"

namespace codepad::ui {
	class manager;

	using animation_clock_t = std::chrono::high_resolution_clock; ///< Type of the clock used for animation updating.
	using animation_time_point_t = animation_clock_t::time_point; ///< Represents a time point in an animation.
	using animation_duration_t = animation_clock_t::duration; ///< Represents a duration in an animation.

	/// Transition functions used in animations.
	namespace transition_functions {
		/// The linear transition function.
		inline double linear(double v) {
			return v;
		}

		/// The smoothstep transition function.
		inline double smoothstep(double v) {
			return v * v * (3.0 - 2.0 * v);
		}

		/// The concave quadratic transition function.
		inline double concave_quadratic(double v) {
			return v * v;
		}
		/// The convex quadratic transition function.
		inline double convex_quadratic(double v) {
			v = 1.0 - v;
			return 1.0 - v * v;
		}

		/// The concave cubic transition function.
		inline double concave_cubic(double v) {
			return v * v * v;
		}
		/// The convex cubic transition function.
		inline double convex_cubic(double v) {
			v = 1.0 - v;
			return 1.0 - v * v * v;
		}
	}

	/// Type of the transition function used to control the process of a \ref animated_property. This function
	/// accepts a double in the range of [0, 1] and return a double in the same range. The input indicates the
	/// current process of the animation, and the output is used to linearly interpolate the current value between
	/// the starting value and the destination value.
	using transition_function = std::function<double(double)>;


	/// Represents the subject of a \ref playing_animation_base.
	class animation_subject_base {
	public:
		/// Default virtual destructor.
		virtual ~animation_subject_base() = default;

		/// Determines if two subjects are the same. False negatives are allowed.
		virtual bool equals(const animation_subject_base&) const = 0;
	};
	/// Basic interface of an ongoing animation.
	class playing_animation_base {
	public:
		/// Default virtual destructor.
		virtual ~playing_animation_base() = default;

		/// Updates the animation.
		///
		/// \return The time before this animation needs to be updated again. Return \p std::nullopt to end the
		///         animation.
		virtual std::optional<animation_duration_t> update(animation_time_point_t) = 0;

		/// Returns the subject of this animation.
		virtual const animation_subject_base &get_subject() const = 0;
	};
	/// Basic interface for animation definitions.
	class animation_definition_base {
	public:
		/// Default virtual destructor.
		virtual ~animation_definition_base() = default;

		/// Starts the animation for the given \ref animation_subject_base, and returns the corresponding
		/// \ref playing_animation_base.
		virtual std::unique_ptr<playing_animation_base> start(std::unique_ptr<animation_subject_base>) const = 0;
	};


	/// Basic interface of \ref animation_subject_base with a specific type.
	template <typename T> class typed_animation_subject_base : public animation_subject_base {
	public:
		/// Returns the current value.
		virtual const T &get() const = 0;
		/// Sets the current value.
		virtual void set(T) = 0;
	};


	/// Used to interpolate between values using \ref lerp(). If such interpolation is not possible, returns the from
	/// value.
	template <typename T> struct default_lerp {
	public:
		/// Calls \ref lerp() if \ref _can_lerp::value is \p true.
		T operator()(T from, [[maybe_unused]] T to, [[maybe_unused]] double perc) const {
			if constexpr (_can_lerp::value) {
				return lerp(from, to, perc);
			} else {
				return from;
			}
		}
	protected:
		/// Used to test if lerping is possible for this type.
		struct _can_lerp {
			/// Matches if all necessary operators are implemented.
			template <typename U = T> static std::true_type test(decltype(
				std::declval<U>() + (std::declval<U>() - std::declval<U>()) * 0.5, 0
				));
			/// Matches otherwise.
			template <typename U = T> static std::false_type test(...);

			/// The result.
			constexpr static bool value = std::is_same_v<decltype(test(0)), std::true_type>;
		};
	};
	/// Returns the target value without interpolating.
	struct no_lerp {
		/// Returns \p to.
		template <typename T> T operator()(T, T to, double) const {
			return to;
		}
	};


	/// Defines a keyframe animation. Key frames contain target values, durations, and transition functions.
	template <typename T, typename Lerp = default_lerp<T>> class keyframe_animation_definition :
		public animation_definition_base {
	public:
		/// A key frame.
		struct keyframe {
			/// Default constructor.
			keyframe() = default;
			/// Initializes all fields of this struct.
			keyframe(T tar, animation_duration_t d, transition_function t) :
				target(std::move(tar)), duration(d), transition_func(std::move(t)) {
			}

			T target{}; ///< The target value.
			/// The duration of this key frame, i.e., the time after the last key frame and before this key frame.
			animation_duration_t duration{0};
			/// The transition function. If this is \p nullptr, then the animation will immediately reach \ref target
			/// value at this \ref keyframe.
			transition_function transition_func;
		};

		/// Default constructor.
		keyframe_animation_definition() = default;
		/// Initializes all fields of this struct.
		keyframe_animation_definition(std::vector<keyframe> kfs, size_t repeat) :
			keyframes(std::move(kfs)), repeat_times(repeat) {
		}

		/// Starts a \ref playing_keyframe_animation.
		std::unique_ptr<playing_animation_base> start(std::unique_ptr<animation_subject_base>) const override;

		std::vector<keyframe> keyframes; ///< The list of key frames.
		/// The number of times to repeat the whole animation. If this is 0, then the animation will be repeated
		/// indefinitely.
		size_t repeat_times = 1;
	};

	/// Stores generic keyframe animation parameters that can be further processed into a
	/// \ref keyframe_animation_definition.
	struct generic_keyframe_animation_definition {
		/// A key frame.
		struct keyframe {
			/// Stores the target value which has not yet been parsed.
			///
			/// \sa keyframe_animation_definition::keyframe::target
			json::value_storage target;
			animation_duration_t duration{0}; ///< \sa keyframe_animation_definition::keyframe::duration
			transition_function transition_func; ///< \sa keyframe_animation_definition::keyframe::transition_func
		};

		std::vector<keyframe> keyframes; ///< The list of key frames.
		size_t repeat_times = 1; ///< \sa keyframe_animation_definition::repeat_times 
	};


	/// Used to parse values used in animations, and to start keyframe animations.
	class animation_value_parser_base {
	public:
		/// Default virtual destructor.
		virtual ~animation_value_parser_base() = default;

		/// Parses a keyframe animation.
		virtual std::unique_ptr<animation_definition_base> parse_keyframe_animation(
			const generic_keyframe_animation_definition&, manager&
		) const = 0;
	};

	/// Value parser for a specific type.
	template <typename T> class typed_animation_value_parser : public animation_value_parser_base {
	public:
		/// Tries to parse the given JSON value into a specific value. By default this function simply calls
		/// \ref json::object_parsers::try_parse().
		virtual bool try_parse(const json::value_storage&, manager&, T&) const;

		/// Parses a \ref keyframe_animation_definition.
		std::unique_ptr<animation_definition_base> parse_keyframe_animation(
			const generic_keyframe_animation_definition&, manager&
		) const override;
	};


	/// An ongoing \ref keygrame_animation_definition
	template <typename T, typename Lerp> class playing_keyframe_animation : public playing_animation_base {
	public:
		/// The maximum number of key frames to advance per update. This is to prevent repeating key frames with zero
		/// duration from locking up the program.
		constexpr static size_t maximum_frames_per_update = 1000;

		using definition_t = keyframe_animation_definition<T, Lerp>; ///< The type of animation definition.

		/// Initializes this playing animation.
		playing_keyframe_animation(
			const definition_t &def, std::unique_ptr<typed_animation_subject_base<T>> sub
		) : _from(sub->get()), _keyframe_start(animation_clock_t::now()), _def(&def), _subject(std::move(sub)) {
		}

		/// Updates this animation.
		///
		/// \param now The time of now.
		/// \return The time before this \ref state needs to be updated again.
		std::optional<animation_duration_t> update(animation_time_point_t now) override {
			for (size_t i = 0; i < maximum_frames_per_update; ++i) { // go through the frames
				if (_cur_frame >= _def->keyframes.size()) { // animation has finished
					_subject->set(_def->keyframes.back().target);
					return std::nullopt;
				}
				const definition_t::keyframe &f = _def->keyframes[_cur_frame];
				animation_time_point_t key_frame_end = _keyframe_start + f.duration;
				if (key_frame_end > now) { // at the correct frame
					if (f.transition_func) {
						_subject->set(Lerp()(_from, f.target, f.transition_func(
							std::chrono::duration<double>(now - _keyframe_start) / f.duration
						)));
						return animation_duration_t(0); // update immediately
					} else { // wait until this key frame is over
						_subject->set(f.target);
						return key_frame_end - now;
					}
				}
				// advance frame
				_keyframe_start += f.duration;
				_from = f.target; // next frame starts at the target value of this frame
				if (++_cur_frame == _def->keyframes.size()) { // at the end, check if should repeat
					++_repeated;
					if (_def->repeat_times == 0 || _repeated < _def->repeat_times) { // yes
						_cur_frame = 0;
					} else { // this animation has finished
						_subject->set(f.target);
						return std::nullopt;
					}
				}
			}
			logger::get().log_warning(CP_HERE, "potential zero-duration loop in animation");
			return std::nullopt;
		}

		/// Returns \ref _subject.
		const animation_subject_base &get_subject() const override {
			return *_subject;
		}
	protected:
		T _from; ///< The value of the last key frame, or the original value.
		animation_time_point_t _keyframe_start; ///< Time when the last \ref keyframe was reached.
		size_t
			_cur_frame = 0, ///< The index of the current \ref keyframe.
			_repeated = 0; ///< The number of times that this animation has been repeated.
		std::unique_ptr<typed_animation_subject_base<T>> _subject; ///< The subject of this animation.
		const definition_t *_def = nullptr; ///< The definition of this animation.
	};

	template <
		typename T, typename Lerp
	> std::unique_ptr<playing_animation_base> keyframe_animation_definition<T, Lerp>::start(
		std::unique_ptr<animation_subject_base> subject
	) const {
		auto *typed = dynamic_cast<typed_animation_subject_base<T>*>(subject.get());
		if (typed) {
			subject.release();
			return std::make_unique<playing_keyframe_animation<T, Lerp>>(
				*this, std::unique_ptr<typed_animation_subject_base<T>>(typed)
				);
		}
		logger::get().log_warning(CP_HERE, "the given subject of the animation is not typed");
		return nullptr;
	}
}
