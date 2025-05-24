#pragma once

#include "ads-vocab.hpp"

namespace ads::concepts {

template <uint64_t Chs>
concept is_mono_data = Chs == 1;

template <typename ValueType, typename Fn>
concept is_multi_channel_read_fn = requires(Fn fn, const ValueType* buffer, channel_idx channel, frame_idx frame_start, ads::frame_count frame_count) {
	{ fn(buffer, channel, frame_start, frame_count) } -> std::same_as<ads::frame_count>;
};

template <typename ValueType, typename Fn>
concept is_multi_channel_write_fn = requires(Fn fn, ValueType* buffer, channel_idx channel, frame_idx frame_start, ads::frame_count frame_count) {
	{ fn(buffer, channel, frame_start, frame_count) } -> std::same_as<ads::frame_count>;
};

template <typename ValueType, typename Fn>
concept is_single_channel_read_fn = requires(Fn fn, const ValueType* buffer, frame_idx frame_start, ads::frame_count frame_count) {
	{ fn(buffer, frame_start, frame_count) } -> std::same_as<ads::frame_count>;
};

template <typename ValueType, typename Fn>
concept is_single_channel_write_fn = requires(Fn fn, ValueType* buffer, frame_idx frame_start, ads::frame_count frame_count) {
	{ fn(buffer, frame_start, frame_count) } -> std::same_as<ads::frame_count>;
};

template <typename ValueType, typename Fn>
concept is_multi_channel_provider_fn = requires(Fn fn, channel_idx ch, frame_idx fr) {
	{ fn(ch, fr) } -> std::same_as<ValueType>;
};

template <typename ValueType, typename Fn>
concept is_single_channel_provider_fn = requires(Fn fn, frame_idx fr) {
	{ fn(fr) } -> std::same_as<ValueType>;
};

template <typename ValueType, typename Fn> concept is_read_fn  = is_single_channel_read_fn<ValueType, Fn> || is_multi_channel_read_fn<ValueType, Fn>;
template <typename ValueType, typename Fn> concept is_write_fn = is_single_channel_write_fn<ValueType, Fn> || is_multi_channel_write_fn<ValueType, Fn>;

template <typename ValueType, typename Fn>
concept is_value_visitor_fn = std::invocable<Fn, channel_idx, frame_idx, ValueType>;

} // namespace ads::concepts
