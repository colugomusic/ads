#pragma once

#include <algorithm>
#include <array>
#include <boost/align/aligned_allocator.hpp>
#include <boost/container/small_vector.hpp>
#include <cmath>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

#ifndef ADS_ASSERT
#include <cassert>
#define ADS_ASSERT(x) assert(x)
#endif

namespace ads {

struct channel_count { uint64_t value = 0; auto operator<=>(const channel_count&) const = default; };
struct channel_idx   { uint64_t value = 0; auto operator<=>(const channel_idx&) const = default; };
struct frame_count   { uint64_t value = 0; auto operator<=>(const frame_count&) const = default; };
struct frame_idx     { uint64_t value = 0; auto operator<=>(const frame_idx&) const = default; };

static constexpr auto DYNAMIC_EXTENT   = std::numeric_limits<uint64_t>::max();
static constexpr auto DYNAMIC_CHANNELS = channel_count{DYNAMIC_EXTENT};
static constexpr auto DYNAMIC_FRAMES   = frame_count{DYNAMIC_EXTENT};

template <uint64_t Chs> struct frame                 { using type = std::array<float, Chs>; };
template <>             struct frame<DYNAMIC_EXTENT> { using type = boost::container::small_vector<float, 2>; };
template <uint64_t Chs = DYNAMIC_EXTENT> using frame_t = typename frame<Chs>::type;

template <uint64_t Chs, bool Const> struct frame_ref;
template <>                         struct frame_ref<DYNAMIC_EXTENT, false> { using type = boost::container::small_vector<float*, 2>; };
template <>                         struct frame_ref<DYNAMIC_EXTENT, true>  { using type = boost::container::small_vector<const float*, 2>; };
template <uint64_t Chs>             struct frame_ref<Chs, false>            { using type = std::array<float*, Chs>; };
template <uint64_t Chs>             struct frame_ref<Chs, true>             { using type = std::array<const float*, Chs>; };
template <uint64_t Chs, bool Const> using frame_ref_t = typename frame_ref<Chs, Const>::type;

template <typename T>
concept is_frame_ref =
	std::same_as<std::ranges::range_value_t<T>, float*> ||
	std::same_as<std::ranges::range_value_t<T>, const float*>;

namespace detail {

// Constants used to protect against underflows
// as this is such a common bug.
static constexpr auto SANE_NUMBER_OF_CHANNELS = 1024ULL;
static constexpr auto SANE_NUMBER_OF_FRAMES   = 44100ULL * 604800ULL; // 1 week of audio at 44100 Hz

template <uint64_t Frs> struct channel_data                 { using type = std::array<float, Frs>; };
template <>             struct channel_data<DYNAMIC_EXTENT> { using type = std::vector<float, boost::alignment::aligned_allocator<float, 16>>; };
template <uint64_t Frs> using channel_data_t = channel_data<Frs>::type;

template <uint64_t Chs, typename ChannelData> struct channels                              { using type = std::array<ChannelData, Chs>; };
template <typename ChannelData>               struct channels<DYNAMIC_EXTENT, ChannelData> { using type = std::vector<ChannelData>; };
template <uint64_t Chs, typename ChannelData> using channels_t = typename channels<Chs, ChannelData>::type;

template <uint64_t Chs, uint64_t Frs>
struct storage : channels_t<Chs, channel_data_t<Frs>> {
	static constexpr auto CHANNEL_COUNT = Chs;
	static constexpr auto FRAME_COUNT   = Frs;
};

template <typename Storage> [[nodiscard]]
auto at(Storage& st, channel_idx channel) -> channel_data_t<Storage::FRAME_COUNT>& {
	return st.at(channel.value);
}

template <typename Storage> [[nodiscard]]
auto at(const Storage& st, channel_idx channel) -> const channel_data_t<Storage::FRAME_COUNT>& {
	return st.at(channel.value);
}

template <typename Storage> [[nodiscard]]
auto at(const Storage& st, channel_idx channel, frame_idx frame) -> const float& {
	return st.at(channel.value).at(frame.value);
}

template <typename Storage> [[nodiscard]]
auto at(Storage& st, channel_idx channel, frame_idx frame) -> float& {
	return st.at(channel.value).at(frame.value);
}

template <typename Storage> [[nodiscard]]
auto at(const Storage& st, channel_idx channel, float frame) -> float {
	const auto index0 = frame_idx{static_cast<uint64_t>(std::floor(frame))};
	const auto index1 = frame_idx{static_cast<uint64_t>(std::ceil(frame))};
	const auto t      = frame - index0.value;
	return std::lerp(at(st, channel, index0), at(st, channel, index1), t);
}

template <typename Storage> [[nodiscard]]
auto at(Storage& st, ads::frame_idx frame_idx) -> frame_ref_t<Storage::CHANNEL_COUNT, false> {
	frame_ref_t<Storage::CHANNEL_COUNT, false> frame;
	for (uint64_t c = 0; c < Storage::CHANNEL_COUNT; c++) {
		frame[c] = &st[c].at(frame_idx.value);
	}
	return frame;
}

template <typename Storage> [[nodiscard]]
auto at(const Storage& st, ads::frame_idx frame_idx) -> frame_ref_t<Storage::CHANNEL_COUNT, true> {
	frame_ref_t<Storage::CHANNEL_COUNT, true> frame;
	for (uint64_t c = 0; c < Storage::CHANNEL_COUNT; c++) {
		frame[c] = &st[c].at(frame_idx.value);
	}
	return frame;
}

template <typename Storage>
	requires (Storage::CHANNEL_COUNT == DYNAMIC_EXTENT)
[[nodiscard]]
auto at(Storage& st, ads::frame_idx frame_idx) -> frame_ref_t<DYNAMIC_EXTENT, false> {
	frame_ref_t<DYNAMIC_EXTENT, false> frame;
	std::ranges::transform(st, std::back_inserter(frame), [frame_idx](auto& channel) {
		return &channel.at(frame_idx.value);
	});
	return frame;
}

template <typename Storage>
	requires (Storage::CHANNEL_COUNT == DYNAMIC_EXTENT)
[[nodiscard]]
auto at(const Storage& st, ads::frame_idx frame_idx) -> frame_ref_t<DYNAMIC_EXTENT, true> {
	frame_ref_t<DYNAMIC_EXTENT, true> frame;
	std::ranges::transform(st, std::back_inserter(frame), [frame_idx](const auto& channel) {
		return &channel.at(frame_idx.value);
	});
	return frame;
}

template <typename Storage> [[nodiscard]] constexpr
auto get_channel_count(const Storage& st) -> channel_count {
	return {st.size()};
}

template <typename Storage> [[nodiscard]]
auto get_frame_count(const Storage& st) -> frame_count {
	return st.empty() ? frame_count{0} : frame_count{st.front().size()};
}

template <typename Storage> [[nodiscard]]
auto data(Storage& st, channel_idx ch) -> float* {
	return st.at(ch.value).data();
}

template <typename Storage> [[nodiscard]]
auto data(const Storage& st, channel_idx ch) -> const float* {
	return st.at(ch.value).data();
}

[[nodiscard]] inline
auto resize(storage<DYNAMIC_EXTENT, DYNAMIC_EXTENT>& st, ads::channel_count channel_count, ads::frame_count frame_count) -> void {
	st.resize(channel_count.value);
	for (auto& channel : st) {
		channel.resize(frame_count.value);
	}
}

template <uint64_t Chs> [[nodiscard]]
auto resize(storage<Chs, DYNAMIC_EXTENT>& st, ads::frame_count frame_count) -> void {
	for (auto& channel : st) {
		channel.resize(frame_count.value);
	}
}

template <uint64_t Frs> [[nodiscard]]
auto resize(storage<DYNAMIC_EXTENT, Frs>& st, ads::channel_count channel_count) -> void {
	st.resize(channel_count.value);
}

template <uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto set(storage<Chs, Frs>& st, channel_idx channel, frame_idx frame, float value) -> void {
	st.at(channel.value).at(frame.value) = value;
}

template <uint64_t Chs, uint64_t Frs>
auto set(storage<Chs, Frs>& st, ads::frame_idx frame_idx, frame_t<Chs> value) -> void {
	for (uint64_t c = 0; c < Chs.value; c++) {
		st[c].at(frame_idx.value) = value[c];
	}
}

template <uint64_t Chs>
concept is_mono_data = Chs == 1;

template <typename Fn>
concept is_multi_channel_read_fn = requires(Fn fn, const float* buffer, channel_idx channel, frame_idx frame_start, ads::frame_count frame_count) {
	{ fn(buffer, channel, frame_start, frame_count) } -> std::same_as<ads::frame_count>;
};

template <typename Fn>
concept is_multi_channel_write_fn = requires(Fn fn, float* buffer, channel_idx channel, frame_idx frame_start, ads::frame_count frame_count) {
	{ fn(buffer, channel, frame_start, frame_count) } -> std::same_as<ads::frame_count>;
};

template <typename Fn>
concept is_mono_read_fn = requires(Fn fn, const float* buffer, frame_idx frame_start, ads::frame_count frame_count) {
	{ fn(buffer, frame_start, frame_count) } -> std::same_as<ads::frame_count>;
};

template <typename Fn>
concept is_mono_write_fn = requires(Fn fn, float* buffer, frame_idx frame_start, ads::frame_count frame_count) {
	{ fn(buffer, frame_start, frame_count) } -> std::same_as<ads::frame_count>;
};

template <typename Fn> concept is_read_fn  = is_mono_read_fn<Fn> || is_multi_channel_read_fn<Fn>;
template <typename Fn> concept is_write_fn = is_mono_write_fn<Fn> || is_multi_channel_write_fn<Fn>;

template <uint64_t Chs, uint64_t Frs, typename ReadFn>
	requires is_mono_read_fn<ReadFn>
auto read(const storage<Chs, Frs>& st, channel_idx ch, frame_idx start, ads::frame_count frame_count, ReadFn read_fn) -> ads::frame_count {
	ADS_ASSERT (start.value <= SANE_NUMBER_OF_FRAMES && "Insane frame start!");
	const auto& channel = at(st, ch);
	if (start.value >= channel.size()) {
		return {0};
	}
	if (start.value + frame_count.value >= channel.size()) {
		frame_count.value = channel.size() - start.value;
	}
	return read_fn(channel.data() + start.value, start, frame_count);
}

template <uint64_t Chs, uint64_t Frs, typename ReadFn>
	requires is_multi_channel_read_fn<ReadFn>
auto read(const storage<Chs, Frs>& st, channel_idx ch, frame_idx start, ads::frame_count frame_count, ReadFn read_fn) -> ads::frame_count {
	return read(st, ch, start, frame_count, [ch, read_fn](const float* buffer, frame_idx frame_start, ads::frame_count frame_count) {
		return read_fn(buffer, ch, frame_start, frame_count);
	});
}

template <uint64_t Chs, uint64_t Frs, typename ReadFn>
	requires is_multi_channel_read_fn<ReadFn>
auto read(const storage<Chs, Frs>& st, frame_idx start, ads::frame_count frame_count, ReadFn read_fn) -> ads::frame_count {
	auto frames_read = ads::frame_count{0};
	for (size_t c = 0; c < st.size(); c++) {
		const auto channel_frames_read = read(st, channel_idx{c}, start, frame_count, read_fn);
		if (c == 0) { frames_read = channel_frames_read; }
		else        { ADS_ASSERT (frames_read == channel_frames_read && "Frame count mismatch"); }
	}
	return frames_read;
}

template <uint64_t Chs, uint64_t Frs, typename WriteFn>
	requires is_mono_write_fn<WriteFn>
auto write(storage<Chs, Frs>& st, channel_idx ch, frame_idx start, ads::frame_count frame_count, WriteFn write_fn) -> ads::frame_count {
	ADS_ASSERT (start.value <= SANE_NUMBER_OF_FRAMES && "Insane frame start!");
	auto& channel = at(st, ch);
	if (start.value >= channel.size()) {
		return {0};
	}
	if (start.value + frame_count.value >= channel.size()) {
		frame_count.value = channel.size() - start.value;
	}
	return write_fn(channel.data() + start.value, start, frame_count);
}

template <uint64_t Chs, uint64_t Frs, typename WriteFn>
	requires is_multi_channel_write_fn<WriteFn>
auto write(storage<Chs, Frs>& st, channel_idx ch, frame_idx start, ads::frame_count frame_count, WriteFn write_fn) -> ads::frame_count {
	return write(st, ch, start, frame_count, [ch, write_fn](float* buffer, frame_idx frame_start, ads::frame_count frame_count) {
		return write_fn(buffer, ch, frame_start, frame_count);
	});
}

template <uint64_t Chs, uint64_t Frs, typename WriteFn>
	requires is_mono_write_fn<WriteFn> && is_mono_data<Chs>
auto write(storage<Chs, Frs>& st, frame_idx start, ads::frame_count frame_count, WriteFn write_fn) -> ads::frame_count {
	return write(st, channel_idx{0}, start, frame_count, write_fn);
}

template <uint64_t Chs, uint64_t Frs, typename WriteFn>
	requires is_multi_channel_write_fn<WriteFn>
auto write(storage<Chs, Frs>& st, frame_idx start, ads::frame_count frame_count, WriteFn write_fn) -> ads::frame_count {
	auto frames_written = ads::frame_count{0};
	for (size_t c = 0; c < st.size(); c++) {
		const auto channel_frames_written = write(st, channel_idx{c}, start, frame_count, write_fn);
		if (c == 0) { frames_written = channel_frames_written; }
		else        { ADS_ASSERT (frames_written == channel_frames_written && "Frame count mismatch"); }
	}
	return frames_written;
}

template <uint64_t Chs, uint64_t Frs>
auto write(storage<Chs, Frs>& dest, frame_idx start, ads::frame_count frame_count, const storage<Chs, Frs>& src) -> ads::frame_count {
	return write(dest, start, frame_count, [&src](float* buffer, channel_idx ch, frame_idx frame_start, ads::frame_count frame_count) {
		const auto& channel = at(src, ch);
		std::copy_n(channel.begin(), frame_count.value, buffer);
		return frame_count;
	});
}

template <uint64_t Chs, uint64_t Frs, bool Const>
struct frame_iterator_base {
	using iterator_category = std::random_access_iterator_tag;
	using value_type        = frame_ref_t<Chs, Const>;
	using difference_type   = int64_t;
	using pointer           = value_type*;
	using reference         = value_type&;
	using storage_type      = std::conditional_t<Const, const storage<Chs, Frs>, storage<Chs, Frs>>;
	storage_type* st = nullptr;
	frame_idx frame  = {0};
	frame_iterator_base() = default;
	frame_iterator_base(storage_type& st)                  : st{st.empty() ? nullptr : &st} {}
	frame_iterator_base(storage_type& st, frame_idx frame) : st{st.empty() ? nullptr : &st}, frame{frame} {}
	frame_iterator_base(const frame_iterator_base&) = default;
	frame_iterator_base(frame_iterator_base&&) = default;
	frame_iterator_base& operator=(const frame_iterator_base&) = default;
	frame_iterator_base& operator=(frame_iterator_base&&) = default;
	[[nodiscard]] static
	auto is_end(const frame_iterator_base& it) -> bool {
		if (!it.st) return true;
		if (it.frame.value == std::size(*it.st->begin())) return true;
		return false;
	}
	auto operator*()        { return detail::at(*st, frame); }
	auto operator*() const  { return detail::at(*st, frame); }
	auto operator->()       { return detail::at(*st, frame); }
	auto operator->() const { return detail::at(*st, frame); }
	auto operator++(int)    { auto copy = *this; ++frame.value; return copy; }
	auto operator--(int)    { auto copy = *this; --frame.value; return copy; }
	auto operator+=(uint64_t n) { frame.value += n; return *this; }
	auto operator-=(uint64_t n) { frame.value -= n; return *this; }
	auto operator+(uint64_t n) const { return frame_iterator_base{*st, frame_idx{frame.value + n}}; }
	auto operator-(uint64_t n) const { return frame_iterator_base{*st, frame_idx{frame.value - n}}; }
	auto operator==(const frame_iterator_base& other) const {
		if (is_end(*this)) return is_end(other);
		if (is_end(other)) return false;
		return frame == other.frame;
	}
	auto operator!=(const frame_iterator_base& other) const { return !(*this == other); }
	auto operator<=>(const frame_iterator_base& other) const {
		if (is_end(*this)) {
			if (is_end(other)) { return std::strong_ordering::equal; }
			else               { return std::strong_ordering::greater; }
		}
		if (is_end(other)) { return std::strong_ordering::less; }
		if (st != other.st) { throw std::invalid_argument{"Invalid iterator comparison"}; }
		return frame <=> other.frame;
	}
	frame_iterator_base& operator--() { --frame.value; return *this; }
	frame_iterator_base& operator++() { ++frame.value; return *this; }
};

template <uint64_t Chs, uint64_t Frs> using frame_iterator       = frame_iterator_base<Chs, Frs, false>;
template <uint64_t Chs, uint64_t Frs> using const_frame_iterator = frame_iterator_base<Chs, Frs, true>;

} // namespace detail

template <std::ranges::input_range Frames, typename OutputIterator>
	requires
		std::same_as<std::ranges::range_value_t<Frames>, float> &&
		is_frame_ref<std::iter_value_t<OutputIterator>>
auto deinterleave(Frames&& input, OutputIterator output) -> void {
	static auto channel_count = std::size(*output);
	auto pos = std::begin(input);
	auto end = std::end(input);
	while (pos != end) {
		auto output_frame = *output++;
		for (auto ptr : output_frame) {
			*ptr = *pos++;
		}
	}
}

template <std::ranges::input_range Input, typename OutputIterator>
	requires
		std::same_as<std::iter_value_t<OutputIterator>, float> &&
		is_frame_ref<std::ranges::range_value_t<Input>>
auto interleave(Input&& input, OutputIterator output) -> void {
	std::vector<std::ranges::iterator_t<std::ranges::range_reference_t<Input>>> iterators;
	std::vector<std::ranges::sentinel_t<std::ranges::range_reference_t<Input>>> ends;
	for (auto&& channel : input) {
		iterators.push_back(std::begin(channel));
		ends.push_back(std::end(channel));
	}
	bool progress = false;
	do {
		progress = false;
		for (size_t c = 0; c < iterators.size(); c++) {
			if (iterators[c] != ends[c]) {
				*output++ = **iterators[c]++;
				progress = true;
			}
		}
	} while (progress);
}

namespace detail {

template <uint64_t Chs, uint64_t Frs>
struct impl {
	impl() = default;
	impl(storage<Chs, Frs>&& st) : st_{std::move(st)} {}
	impl& operator=(const impl&)     = default;
	impl& operator=(impl&&) noexcept = default;
	impl(const impl&)                = default;
	impl(impl&&) noexcept            = default;
	[[nodiscard]] constexpr auto get_channel_count() const -> channel_count   { return detail::get_channel_count(st_); }
	[[nodiscard]] auto get_frame_count() const -> frame_count                 { return detail::get_frame_count(st_); }
	[[nodiscard]] auto at(channel_idx ch) -> channel_data_t<Frs>&             { return detail::at(st_, ch); }
	[[nodiscard]] auto at(channel_idx ch) const -> const channel_data_t<Frs>& { return detail::at(st_, ch); }
	[[nodiscard]] auto at(channel_idx ch, frame_idx f) -> float&              { return detail::at(st_, ch, f); }
	[[nodiscard]] auto at(channel_idx ch, frame_idx f) const -> const float   { return detail::at(st_, ch, f); }
	[[nodiscard]] auto at(channel_idx ch, float f) const -> float             { return detail::at(st_, ch, f); }
	[[nodiscard]] auto begin() -> frame_iterator<Chs, Frs>                    { return {st_}; }
	[[nodiscard]] auto end() -> frame_iterator<Chs, Frs>                      { return {}; }
	[[nodiscard]] auto begin() const -> const_frame_iterator<Chs, Frs>        { return {st_}; }
	[[nodiscard]] auto end() const -> const_frame_iterator<Chs, Frs>          { return {}; }
	[[nodiscard]] auto cbegin() const -> const_frame_iterator<Chs, Frs>       { return {st_}; }
	[[nodiscard]] auto cend() const -> const_frame_iterator<Chs, Frs>         { return {}; }
	[[nodiscard]] auto data(channel_idx ch) -> float*                         { return detail::data(st_, ch); }
	[[nodiscard]] auto data(channel_idx ch) const -> const float*             { return detail::data(st_, ch); }
	[[nodiscard]] auto at() -> channel_data_t<Frs>&             requires (is_mono_data<Chs>) { return detail::at(st_, channel_idx{0}); }
	[[nodiscard]] auto at() const -> const channel_data_t<Frs>& requires (is_mono_data<Chs>) { return detail::at(st_, channel_idx{0}); }
	[[nodiscard]] auto data() -> float*                         requires (is_mono_data<Chs>) { return detail::data(st_, channel_idx{0}); }
	[[nodiscard]] auto data() const -> const float*             requires (is_mono_data<Chs>) { return detail::data(st_, channel_idx{0}); }
	[[nodiscard]] auto at(frame_idx f) -> float&                requires (is_mono_data<Chs>) { return detail::at(st_, channel_idx{0}, f); }
	[[nodiscard]] auto at(frame_idx f) const -> const float&    requires (is_mono_data<Chs>) { return detail::at(st_, channel_idx{0}, f); }
	[[nodiscard]] auto at(float f) const -> float               requires (is_mono_data<Chs>) { return detail::at(st_, channel_idx{0}, f); }
	auto resize(ads::channel_count channel_count, ads::frame_count frame_count) -> void
		requires (Chs == DYNAMIC_EXTENT && Frs == DYNAMIC_EXTENT)
	{
		detail::resize(st_, channel_count, frame_count);
	}
	auto resize(ads::channel_count channel_count) -> void
		requires (Chs == DYNAMIC_EXTENT)
	{
		detail::resize(st_, channel_count);
	}
	auto resize(ads::frame_count frame_count) -> void
		requires (Frs == DYNAMIC_EXTENT)
	{
		detail::resize<Chs>(st_, frame_count);
	}
	auto set(frame_idx f, frame_t<Chs> value) -> void {
		auto pos = std::begin(value);
		for (size_t c = 0; c < detail::get_channel_count(st_).value; c++) {
			detail::set(st_, channel_idx{c}, f, *pos++);
		}
	}
	auto set(channel_idx ch, frame_idx f, float value) -> void {
		detail::set(st_, ch, f, value);
	}
	template <typename ReadFn>
		requires detail::is_multi_channel_read_fn<ReadFn>
	auto read(frame_idx start, frame_count n, ReadFn read_fn) const -> frame_count {
		return detail::read(st_, start, n, read_fn);
	}
	template <typename ReadFn>
		requires detail::is_mono_read_fn<ReadFn>
	auto read(channel_idx ch, frame_idx start, frame_count n, ReadFn read_fn) const -> frame_count {
		return detail::read(st_, ch, start, n, read_fn);
	}
	auto write(frame_idx start, const impl<Chs, Frs>& data) -> frame_count {
		return detail::write(st_, start, data.get_frame_count(), data.st_);
	}
	auto write(const impl<Chs, Frs>& data) -> frame_count {
		return write(frame_idx{0}, data);
	}
	template <typename WriteFn>
		requires detail::is_write_fn<WriteFn>
	auto write(frame_idx start, frame_count n, WriteFn write_fn) -> frame_count {
		return detail::write(st_, start, n, write_fn);
	}
	template <typename WriteFn>
		requires detail::is_write_fn<WriteFn>
	auto write(frame_count n, WriteFn write_fn) -> frame_count {
		return write(frame_idx{0}, n, write_fn);
	}
	template <typename WriteFn>
		requires detail::is_mono_write_fn<WriteFn>
	auto write(channel_idx ch, frame_idx start, frame_count n, WriteFn write_fn) -> frame_count {
		return detail::write(st_, ch, start, n, write_fn);
	}
	template <typename WriteFn>
		requires detail::is_mono_write_fn<WriteFn>
	auto write(channel_idx ch, frame_count n, WriteFn write_fn) -> frame_count {
		return write(ch, frame_idx{0}, n, write_fn);
	}
private:
	alignas(16) storage<Chs, Frs> st_;
};

} // namespace detail

template <uint64_t Chs, uint64_t Frs> using data = detail::impl<Chs, Frs>;
template <uint64_t Frs> using mono   = data<1, Frs>;
template <uint64_t Frs> using stereo = data<2, Frs>;
using dynamic_mono   = data<1, DYNAMIC_EXTENT>;
using dynamic_stereo = data<2, DYNAMIC_EXTENT>;
using fully_dynamic  = data<DYNAMIC_EXTENT, DYNAMIC_EXTENT>;

[[nodiscard]] inline
auto make(ads::channel_count channel_count, ads::frame_count frame_count) -> data<DYNAMIC_EXTENT, DYNAMIC_EXTENT> {
	ADS_ASSERT (channel_count.value <= detail::SANE_NUMBER_OF_CHANNELS && "Channel count too high");
	ADS_ASSERT (frame_count.value <= detail::SANE_NUMBER_OF_FRAMES && "Frame count too high");
	detail::storage<DYNAMIC_EXTENT, DYNAMIC_EXTENT> st;
	st.resize(channel_count.value);
	for (auto& channel : st) {
		channel.resize(frame_count.value);
	}
	return {std::move(st)};
}

template <uint64_t Chs> [[nodiscard]]
auto make(ads::frame_count frame_count) -> data<Chs, DYNAMIC_EXTENT> {
	ADS_ASSERT (frame_count.value <= detail::SANE_NUMBER_OF_FRAMES && "Frame count too high");
	detail::storage<Chs, DYNAMIC_EXTENT> st;
	for (auto& channel : st) {
		channel.resize(frame_count.value);
	}
	return {std::move(st)};
}

template <uint64_t Frs> [[nodiscard]]
auto make(ads::channel_count channel_count) -> data<DYNAMIC_EXTENT, Frs> {
	ADS_ASSERT (channel_count.value <= detail::SANE_NUMBER_OF_CHANNELS && "Channel count too high");
	detail::storage<DYNAMIC_EXTENT, Frs> st;
	st.resize(channel_count.value);
	return {std::move(st)};
}

template <channel_count Chs, frame_count Frs> [[nodiscard]]
auto make() -> data<DYNAMIC_EXTENT, DYNAMIC_EXTENT> {
	return {};
}

template <uint64_t Frs> [[nodiscard]] auto make_mono() -> mono<Frs>     { return make<1, Frs>(); } 
template <uint64_t Frs> [[nodiscard]] auto make_stereo() -> stereo<Frs> { return make<2, Frs>(); }
[[nodiscard]] inline auto make_mono(ads::frame_count frame_count) -> dynamic_mono     { return make<1>(frame_count); } 
[[nodiscard]] inline auto make_stereo(ads::frame_count frame_count) -> dynamic_stereo { return make<2>(frame_count); } 

struct interleaved {
	interleaved() = default;
	interleaved(ads::channel_count channel_count, ads::frame_count frame_count)
		: channel_count_{channel_count}
		, frame_count_{frame_count}
		, data_{make_mono({channel_count.value * frame_count.value})}
	{}
	[[nodiscard]] auto get_channel_count() const -> channel_count { return channel_count_; }
	[[nodiscard]] auto get_frame_count() const -> frame_count     { return frame_count_; }
	[[nodiscard]] auto at(uint64_t index) -> float&               { return data_.at(frame_idx{index}); }
	[[nodiscard]] auto at(uint64_t index) const -> const float&   { return data_.at(frame_idx{index}); }
	[[nodiscard]] auto begin()                                    { return data_.at().begin(); }
	[[nodiscard]] auto end()                                      { return data_.at().end(); }
	[[nodiscard]] auto begin() const                              { return data_.at().begin(); }
	[[nodiscard]] auto end() const                                { return data_.at().end(); }
	[[nodiscard]] auto cbegin() const                             { return data_.at().begin(); }
	[[nodiscard]] auto cend() const                               { return data_.at().end(); }
	[[nodiscard]] auto data() -> float*                           { return data_.data(); }
	[[nodiscard]] auto data() const -> const float*               { return data_.data(); }
	auto resize(ads::channel_count channel_count) -> void {
		channel_count_ = channel_count;
		data_.resize(ads::frame_count{channel_count_.value * frame_count_.value});
	}
	auto resize(ads::frame_count frame_count) -> void {
		frame_count_ = frame_count;
		data_.resize(ads::frame_count{channel_count_.value * frame_count_.value});
	}
private:
	ads::channel_count channel_count_;
	ads::frame_count frame_count_;
	dynamic_mono data_;
};

} // namespace ads
