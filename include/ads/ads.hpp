#pragma once

#include <algorithm>
#include <array>
#include <boost/align/aligned_allocator.hpp>
#include <boost/container/small_vector.hpp>
#include <cmath>
#include <format>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

namespace ads {

struct channel_count { uint64_t value = 0; auto operator<=>(const channel_count&) const = default; };
struct channel_idx   { uint64_t value = 0; auto operator<=>(const channel_idx&) const = default; };
struct frame_count   { uint64_t value = 0; auto operator<=>(const frame_count&) const = default; };
struct frame_idx     { uint64_t value = 0; auto operator<=>(const frame_idx&) const = default; };

static constexpr auto DYNAMIC_EXTENT   = std::numeric_limits<uint64_t>::max();
static constexpr auto DYNAMIC_CHANNELS = channel_count{DYNAMIC_EXTENT};
static constexpr auto DYNAMIC_FRAMES   = frame_count{DYNAMIC_EXTENT};

template <typename ValueType, uint64_t Chs> struct frame                            { using type = std::array<ValueType, Chs>; };
template <typename ValueType>               struct frame<ValueType, DYNAMIC_EXTENT> { using type = boost::container::small_vector<ValueType, 2>; };
template <typename ValueType, uint64_t Chs = DYNAMIC_EXTENT> using frame_t = typename frame<ValueType, Chs>::type;

template <typename ValueType, uint64_t Chs, bool Const> struct frame_ref;
template <typename ValueType>                           struct frame_ref<ValueType, DYNAMIC_EXTENT, false> { using type = boost::container::small_vector<ValueType*, 2>; };
template <typename ValueType>                           struct frame_ref<ValueType, DYNAMIC_EXTENT, true>  { using type = boost::container::small_vector<const ValueType*, 2>; };
template <typename ValueType, uint64_t Chs>             struct frame_ref<ValueType, Chs, false>            { using type = std::array<ValueType*, Chs>; };
template <typename ValueType, uint64_t Chs>             struct frame_ref<ValueType, Chs, true>             { using type = std::array<const ValueType*, Chs>; };
template <typename ValueType, uint64_t Chs, bool Const> using frame_ref_t = typename frame_ref<ValueType, Chs, Const>::type;

template <typename T, typename ValueType>
concept is_frame_ref =
	std::same_as<std::ranges::range_value_t<T>, ValueType*> ||
	std::same_as<std::ranges::range_value_t<T>, const ValueType*>;

namespace detail {

// Constants used to protect against underflows
// as this is such a common bug.
static constexpr auto SANE_NUMBER_OF_CHANNELS = 1024ULL;
static constexpr auto SANE_NUMBER_OF_FRAMES   = 44100ULL * 604800ULL; // 1 week of audio at 44100 Hz

template <typename ValueType, uint64_t Frs> struct channel_data                            { using type = std::array<ValueType, Frs>; };
template <typename ValueType>               struct channel_data<ValueType, DYNAMIC_EXTENT> { using type = std::vector<ValueType, boost::alignment::aligned_allocator<ValueType, 16>>; };
template <typename ValueType, uint64_t Frs> using channel_data_t = channel_data<ValueType, Frs>::type;

template <uint64_t Chs, typename ChannelData> struct channels                              { using type = std::array<ChannelData, Chs>; };
template <typename ChannelData>               struct channels<DYNAMIC_EXTENT, ChannelData> { using type = std::vector<ChannelData>; };
template <uint64_t Chs, typename ChannelData> using channels_t = typename channels<Chs, ChannelData>::type;

template <typename ValueType, uint64_t Frs> using channel_iterator_t       = typename channel_data_t<ValueType, Frs>::iterator;
template <typename ValueType, uint64_t Frs> using const_channel_iterator_t = typename channel_data_t<ValueType, Frs>::const_iterator;

template <typename ValueType, uint64_t Chs, uint64_t Frs>
struct storage : channels_t<Chs, channel_data_t<ValueType, Frs>> {
	static constexpr auto CHANNEL_COUNT = Chs;
	static constexpr auto FRAME_COUNT   = Frs;
	using value_type             = ValueType;
	using channel_iterator       = channel_iterator_t<ValueType, Frs>;
	using const_channel_iterator = const_channel_iterator_t<ValueType, Frs>;
};

template <typename Storage> [[nodiscard]]
auto at(Storage& st, channel_idx channel) -> channel_data_t<typename Storage::value_type, Storage::FRAME_COUNT>& {
	return st.at(channel.value);
}

template <typename Storage> [[nodiscard]]
auto at(const Storage& st, channel_idx channel) -> const channel_data_t<typename Storage::value_type, Storage::FRAME_COUNT>& {
	return st.at(channel.value);
}

template <typename Storage> [[nodiscard]]
auto at(const Storage& st, channel_idx channel, frame_idx frame) -> const typename Storage::value_type& {
	return st.at(channel.value).at(frame.value);
}

template <typename Storage> [[nodiscard]]
auto at(Storage& st, channel_idx channel, frame_idx frame) -> typename Storage::value_type& {
	return st.at(channel.value).at(frame.value);
}

template <typename Storage> [[nodiscard]]
auto at(const Storage& st, channel_idx channel, float frame) -> typename Storage::value_type {
	const auto index0 = frame_idx{static_cast<uint64_t>(std::floor(frame))};
	const auto index1 = frame_idx{static_cast<uint64_t>(std::ceil(frame))};
	const auto t      = frame - index0.value;
	return std::lerp(at(st, channel, index0), at(st, channel, index1), t);
}

template <typename Storage> [[nodiscard]]
auto at(Storage& st, ads::frame_idx frame_idx) -> frame_ref_t<typename Storage::value_type, Storage::CHANNEL_COUNT, false> {
	frame_ref_t<typename Storage::value_type, Storage::CHANNEL_COUNT, false> frame;
	for (size_t c = 0; c < Storage::CHANNEL_COUNT; c++) {
		frame[c] = &st[c].at(frame_idx.value);
	}
	return frame;
}

template <typename Storage> [[nodiscard]]
auto at(const Storage& st, ads::frame_idx frame_idx) -> frame_ref_t<typename Storage::value_type, Storage::CHANNEL_COUNT, true> {
	frame_ref_t<typename Storage::value_type, Storage::CHANNEL_COUNT, true> frame;
	for (size_t c = 0; c < Storage::CHANNEL_COUNT; c++) {
		frame[c] = &st[c].at(frame_idx.value);
	}
	return frame;
}

template <typename Storage>
	requires (Storage::CHANNEL_COUNT == DYNAMIC_EXTENT)
[[nodiscard]]
auto at(Storage& st, ads::frame_idx frame_idx) -> frame_ref_t<typename Storage::value_type, DYNAMIC_EXTENT, false> {
	frame_ref_t<typename Storage::value_type, DYNAMIC_EXTENT, false> frame;
	std::ranges::transform(st, std::back_inserter(frame), [frame_idx](auto& channel) {
		return &channel.at(frame_idx.value);
	});
	return frame;
}

template <typename Storage>
	requires (Storage::CHANNEL_COUNT == DYNAMIC_EXTENT)
[[nodiscard]]
auto at(const Storage& st, ads::frame_idx frame_idx) -> frame_ref_t<typename Storage::value_type, DYNAMIC_EXTENT, true> {
	frame_ref_t<typename Storage::value_type, DYNAMIC_EXTENT, true> frame;
	std::ranges::transform(st, std::back_inserter(frame), [frame_idx](const auto& channel) {
		return &channel.at(frame_idx.value);
	});
	return frame;
}

template <typename Storage>
	requires (Storage::CHANNEL_COUNT == DYNAMIC_EXTENT)
[[nodiscard]]
auto get_channel_count(const Storage& st) -> channel_count {
	return {st.size()};
}

template <typename Storage>
	requires (Storage::FRAME_COUNT == DYNAMIC_EXTENT)
[[nodiscard]]
auto get_frame_count(const Storage& st) -> frame_count {
	return st.empty() ? frame_count{0} : frame_count{st.front().size()};
}

template <typename Storage> [[nodiscard]] consteval auto get_channel_count() -> channel_count { return {Storage::CHANNEL_COUNT}; }
template <typename Storage> [[nodiscard]] consteval auto get_frame_count()   -> frame_count   { return {Storage::FRAME_COUNT}; }

template <typename Storage> [[nodiscard]]
auto data(Storage& st, channel_idx ch) -> typename Storage::value_type* {
	return st.at(ch.value).data();
}

template <typename Storage> [[nodiscard]]
auto data(const Storage& st, channel_idx ch) -> const typename Storage::value_type* {
	return st.at(ch.value).data();
}

template <typename ValueType>
auto resize(storage<ValueType, DYNAMIC_EXTENT, DYNAMIC_EXTENT>& st, ads::channel_count channel_count, ads::frame_count frame_count) -> void {
	st.resize(channel_count.value);
	for (auto& channel : st) {
		channel.resize(frame_count.value);
	}
}

template <typename ValueType, uint64_t Chs>
auto resize(storage<ValueType, Chs, DYNAMIC_EXTENT>& st, ads::frame_count frame_count) -> void {
	for (auto& channel : st) {
		channel.resize(frame_count.value);
	}
}

template <typename ValueType, uint64_t Frs>
auto resize(storage<ValueType, DYNAMIC_EXTENT, Frs>& st, ads::channel_count channel_count) -> void {
	st.resize(channel_count.value);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto set(storage<ValueType, Chs, Frs>& st, channel_idx channel, frame_idx frame, ValueType value) -> void {
	st.at(channel.value).at(frame.value) = value;
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto set(storage<ValueType, Chs, Frs>& st, ads::frame_idx frame_idx, frame_t<ValueType, Chs> value) -> void {
	for (uint64_t c = 0; c < Chs; c++) {
		st[c].at(frame_idx.value) = value[c];
	}
}

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

template <typename ValueType, uint64_t Chs, uint64_t Frs, typename ReadFn>
	requires is_single_channel_read_fn<ValueType, ReadFn>
auto read(const storage<ValueType, Chs, Frs>& st, channel_idx ch, frame_idx start, ads::frame_count frame_count, ReadFn read_fn) -> ads::frame_count {
	if (start.value > SANE_NUMBER_OF_FRAMES) {
		throw std::underflow_error{std::format("ads::detail::read() with frame start = {} is insane", start.value)};
	}
	const auto& channel = at(st, ch);
	if (start.value >= channel.size()) {
		return {0};
	}
	if (start.value + frame_count.value >= channel.size()) {
		frame_count.value = channel.size() - start.value;
	}
	return read_fn(channel.data() + start.value, start, frame_count);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, typename ReadFn>
	requires is_multi_channel_read_fn<ValueType, ReadFn>
auto read(const storage<ValueType, Chs, Frs>& st, channel_idx ch, frame_idx start, ads::frame_count frame_count, ReadFn read_fn) -> ads::frame_count {
	return read(st, ch, start, frame_count, [ch, read_fn](const ValueType* buffer, frame_idx frame_start, ads::frame_count frame_count) {
		return read_fn(buffer, ch, frame_start, frame_count);
	});
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, typename ReadFn>
	requires is_single_channel_read_fn<ValueType, ReadFn> && is_mono_data<Chs>
auto read(const storage<ValueType, Chs, Frs>& st, frame_idx start, ads::frame_count frame_count, ReadFn read_fn) -> ads::frame_count {
	return read(st, channel_idx{0}, start, frame_count, read_fn);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, typename ReadFn>
	requires ((is_single_channel_read_fn<ValueType, ReadFn> && !is_mono_data<Chs>) || is_multi_channel_read_fn<ValueType, ReadFn>)
auto read(const storage<ValueType, Chs, Frs>& st, frame_idx start, ads::frame_count frame_count, ReadFn read_fn) -> ads::frame_count {
	auto frames_read = ads::frame_count{0};
	for (size_t c = 0; c < st.size(); c++) {
		const auto channel_frames_read = read(st, channel_idx{c}, start, frame_count, read_fn);
		if (c == 0) { frames_read = channel_frames_read; }
		else if (frames_read != channel_frames_read) {
			throw std::runtime_error{std::format("ads::detail::read() frame count mismatch ({} != {})", frames_read.value, channel_frames_read.value)};
		}
	}
	return frames_read;
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, typename WriteFn>
	requires is_single_channel_write_fn<ValueType, WriteFn>
auto write(storage<ValueType, Chs, Frs>& st, channel_idx ch, frame_idx start, ads::frame_count frame_count, WriteFn write_fn) -> ads::frame_count {
	if (start.value > SANE_NUMBER_OF_FRAMES) {
		throw std::underflow_error{std::format("ads::detail::write() with frame start = {} is insane", start.value)};
	}
	auto& channel = at(st, ch);
	if (start.value >= channel.size()) {
		return {0};
	}
	if (start.value + frame_count.value >= channel.size()) {
		frame_count.value = channel.size() - start.value;
	}
	return write_fn(channel.data() + start.value, start, frame_count);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, typename WriteFn>
	requires is_multi_channel_write_fn<ValueType, WriteFn>
auto write(storage<ValueType, Chs, Frs>& st, channel_idx ch, frame_idx start, ads::frame_count frame_count, WriteFn write_fn) -> ads::frame_count {
	return write(st, ch, start, frame_count, [ch, write_fn](ValueType* buffer, frame_idx frame_start, ads::frame_count frame_count) {
		return write_fn(buffer, ch, frame_start, frame_count);
	});
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, typename WriteFn>
	requires is_single_channel_write_fn<ValueType, WriteFn> && is_mono_data<Chs>
auto write(storage<ValueType, Chs, Frs>& st, frame_idx start, ads::frame_count frame_count, WriteFn write_fn) -> ads::frame_count {
	return write(st, channel_idx{0}, start, frame_count, write_fn);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, typename WriteFn>
	requires ((is_single_channel_write_fn<ValueType, WriteFn> && !is_mono_data<Chs>) || is_multi_channel_write_fn<ValueType, WriteFn>)
auto write(storage<ValueType, Chs, Frs>& st, frame_idx start, ads::frame_count frame_count, WriteFn write_fn) -> ads::frame_count {
	auto frames_written = ads::frame_count{0};
	for (size_t c = 0; c < st.size(); c++) {
		const auto channel_frames_written = write(st, channel_idx{c}, start, frame_count, write_fn);
		if (c == 0) { frames_written = channel_frames_written; }
		else if (frames_written != channel_frames_written) {
			throw std::runtime_error{std::format("ads::detail::write() frame count mismatch ({} != {})", frames_written.value, channel_frames_written.value)};
		}
	}
	return frames_written;
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto write(storage<ValueType, Chs, Frs>& dest, frame_idx start, ads::frame_count frame_count, const storage<ValueType, Chs, Frs>& src) -> ads::frame_count {
	return write(dest, start, frame_count, [&src](ValueType* buffer, channel_idx ch, frame_idx frame_start, ads::frame_count frame_count) {
		const auto& channel = at(src, ch);
		std::copy_n(channel.begin(), frame_count.value, buffer);
		return frame_count;
	});
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, bool Const>
struct frame_iterator_base {
	using iterator_category = std::random_access_iterator_tag;
	using value_type        = frame_ref_t<ValueType, Chs, Const>;
	using difference_type   = int64_t;
	using pointer           = value_type*;
	using reference         = value_type&;
	using storage_type      = std::conditional_t<Const, const storage<ValueType, Chs, Frs>, storage<ValueType, Chs, Frs>>;
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

template <typename ValueType, uint64_t Chs, uint64_t Frs> using frame_iterator       = frame_iterator_base<ValueType, Chs, Frs, false>;
template <typename ValueType, uint64_t Chs, uint64_t Frs> using const_frame_iterator = frame_iterator_base<ValueType, Chs, Frs, true>;

} // namespace detail

namespace detail {

template <typename ValueType, uint64_t Chs, uint64_t Frs>
struct impl {
	impl() = default;
	impl(storage<ValueType, Chs, Frs>&& st) : st_{std::move(st)} {}
	impl& operator=(const impl&)     = default;
	impl& operator=(impl&&) noexcept = default;
	impl(const impl&)                = default;
	impl(impl&&) noexcept            = default;
	[[nodiscard]] constexpr
	auto get_channel_count() const -> channel_count {
		if constexpr (Chs == DYNAMIC_EXTENT) { return detail::get_channel_count(st_); }
		else                                 { return {Chs}; }
	}
	[[nodiscard]] constexpr
	auto get_frame_count() const -> frame_count {
		if constexpr (Frs == DYNAMIC_EXTENT) { return detail::get_frame_count(st_); }
		else                                 { return {Frs}; }
	}
	[[nodiscard]] auto at(channel_idx ch) -> channel_data_t<ValueType, Frs>&                            { return detail::at(st_, ch); }
	[[nodiscard]] auto at(channel_idx ch) const -> const channel_data_t<ValueType, Frs>&                { return detail::at(st_, ch); }
	[[nodiscard]] auto at(channel_idx ch, frame_idx f) -> ValueType&                                    { return detail::at(st_, ch, f); }
	[[nodiscard]] auto at(channel_idx ch, frame_idx f) const -> const ValueType                         { return detail::at(st_, ch, f); }
	[[nodiscard]] auto at(channel_idx ch, float f) const -> ValueType                                   { return detail::at(st_, ch, f); }
	[[nodiscard]] auto begin() -> frame_iterator<ValueType, Chs, Frs>                                   { return {st_}; }
	[[nodiscard]] auto end() -> frame_iterator<ValueType, Chs, Frs>                                     { return {}; }
	[[nodiscard]] auto begin() const -> const_frame_iterator<ValueType, Chs, Frs>                       { return {st_}; }
	[[nodiscard]] auto end() const -> const_frame_iterator<ValueType, Chs, Frs>                         { return {}; }
	[[nodiscard]] auto cbegin() const -> const_frame_iterator<ValueType, Chs, Frs>                      { return {st_}; }
	[[nodiscard]] auto cend() const -> const_frame_iterator<ValueType, Chs, Frs>                        { return {}; }
	[[nodiscard]] auto channels_begin() -> channel_iterator_t<ValueType, Frs>                           { return std::begin(st_); }
	[[nodiscard]] auto channels_end()   -> channel_iterator_t<ValueType, Frs>                           { return std::end(st_); }
	[[nodiscard]] auto channels_begin() const                                                           { return std::cbegin(st_); }
	[[nodiscard]] auto channels_end() const                                                             { return std::cend(st_); }
	[[nodiscard]] auto channels_cbegin() const                                                          { return std::cbegin(st_); }
	[[nodiscard]] auto channels_cend() const                                                            { return std::cend(st_); }
	[[nodiscard]] auto data(channel_idx ch) -> ValueType*                                               { return detail::data(st_, ch); }
	[[nodiscard]] auto data(channel_idx ch) const -> const ValueType*                                   { return detail::data(st_, ch); }
	[[nodiscard]] auto at() -> channel_data_t<ValueType, Frs>&             requires (is_mono_data<Chs>) { return detail::at(st_, channel_idx{0}); }
	[[nodiscard]] auto at() const -> const channel_data_t<ValueType, Frs>& requires (is_mono_data<Chs>) { return detail::at(st_, channel_idx{0}); }
	[[nodiscard]] auto data() -> ValueType*                                requires (is_mono_data<Chs>) { return detail::data(st_, channel_idx{0}); }
	[[nodiscard]] auto data() const -> const ValueType*                    requires (is_mono_data<Chs>) { return detail::data(st_, channel_idx{0}); }
	[[nodiscard]] auto at(frame_idx f) -> ValueType&                       requires (is_mono_data<Chs>) { return detail::at(st_, channel_idx{0}, f); }
	[[nodiscard]] auto at(frame_idx f) const -> const ValueType&           requires (is_mono_data<Chs>) { return detail::at(st_, channel_idx{0}, f); }
	[[nodiscard]] auto at(float f) const -> ValueType                      requires (is_mono_data<Chs>) { return detail::at(st_, channel_idx{0}, f); }
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
		detail::resize(st_, frame_count);
	}
	auto set(frame_idx f, frame_t<ValueType, Chs> value) -> void {
		auto pos = std::begin(value);
		for (size_t c = 0; c < detail::get_channel_count(st_).value; c++) {
			detail::set(st_, channel_idx{c}, f, *pos++);
		}
	}
	auto set(channel_idx ch, frame_idx f, ValueType value) -> void {
		detail::set(st_, ch, f, value);
	}
	template <typename ReadFn>
	auto read(ReadFn read_fn) const -> frame_count {
		return detail::read(st_, frame_idx{0}, get_frame_count(), read_fn);
	}
	template <typename ReadFn>
	auto read(frame_idx start, ReadFn read_fn) const -> frame_count {
		return detail::read(st_, start, get_frame_count(), read_fn);
	}
	template <typename ReadFn>
	auto read(frame_count n, ReadFn read_fn) const -> frame_count {
		return detail::read(st_, frame_idx{0}, n, read_fn);
	}
	template <typename ReadFn>
	auto read(frame_idx start, frame_count n, ReadFn read_fn) const -> frame_count {
		return detail::read(st_, start, n, read_fn);
	}
	template <typename ReadFn>
	auto read(channel_idx ch, ReadFn read_fn) const -> frame_count {
		return detail::read(st_, ch, frame_idx{0}, get_frame_count(), read_fn);
	}
	template <typename ReadFn>
	auto read(channel_idx ch, frame_idx start, ReadFn read_fn) const -> frame_count {
		return detail::read(st_, ch, start, get_frame_count(), read_fn);
	}
	template <typename ReadFn>
	auto read(channel_idx ch, frame_count n, ReadFn read_fn) const -> frame_count {
		return detail::read(st_, ch, frame_idx{0}, n, read_fn);
	}
	template <typename ReadFn>
	auto read(channel_idx ch, frame_idx start, frame_count n, ReadFn read_fn) const -> frame_count {
		return detail::read(st_, ch, start, n, read_fn);
	}
	auto write(frame_idx start, const impl<ValueType, Chs, Frs>& data) -> frame_count {
		return detail::write(st_, start, data.get_frame_count(), data.st_);
	}
	auto write(const impl<ValueType, Chs, Frs>& data) -> frame_count {
		return detail::write(st_, frame_idx{0}, data.get_frame_count(), data.st_);
	}
	template <typename WriteFn>
	auto write(WriteFn write_fn) -> frame_count {
		return detail::write(st_, frame_idx{0}, get_frame_count(), write_fn);
	}
	template <typename WriteFn>
	auto write(frame_count n, WriteFn write_fn) -> frame_count {
		return detail::write(st_, frame_idx{0}, n, write_fn);
	}
	template <typename WriteFn>
	auto write(frame_idx start, WriteFn write_fn) -> frame_count {
		return detail::write(st_, start, get_frame_count(), write_fn);
	}
	template <typename WriteFn>
	auto write(frame_idx start, frame_count n, WriteFn write_fn) -> frame_count {
		return detail::write(st_, start, n, write_fn);
	}
	template <typename WriteFn>
	auto write(channel_idx ch, WriteFn write_fn) -> frame_count {
		return detail::write(st_, ch, frame_idx{0}, get_frame_count(), write_fn);
	}
	template <typename WriteFn>
	auto write(channel_idx ch, frame_count n, WriteFn write_fn) -> frame_count {
		return detail::write(st_, ch, frame_idx{0}, n, write_fn);
	}
	template <typename WriteFn>
	auto write(channel_idx ch, frame_idx start, WriteFn write_fn) -> frame_count {
		return detail::write(st_, ch, start, get_frame_count(), write_fn);
	}
	template <typename WriteFn>
	auto write(channel_idx ch, frame_idx start, frame_count n, WriteFn write_fn) -> frame_count {
		return detail::write(st_, ch, start, n, write_fn);
	}
private:
	alignas(16) storage<ValueType, Chs, Frs> st_;
};

} // namespace detail

template <typename ValueType, uint64_t Chs, uint64_t Frs> using data   = detail::impl<ValueType, Chs, Frs>;
template <typename ValueType, uint64_t Frs>               using mono   = data<ValueType, 1, Frs>;
template <typename ValueType, uint64_t Frs>               using stereo = data<ValueType, 2, Frs>;
template <typename ValueType> using dynamic_mono   = mono<ValueType, DYNAMIC_EXTENT>;
template <typename ValueType> using dynamic_stereo = stereo<ValueType, DYNAMIC_EXTENT>;
template <typename ValueType> using fully_dynamic  = data<ValueType, DYNAMIC_EXTENT, DYNAMIC_EXTENT>;

template <typename ValueType, uint64_t Chs, uint64_t Frs> [[nodiscard]] auto as_channel_range(data<ValueType, Chs, Frs>& st)       { return std::ranges::subrange(st.channels_begin(), st.channels_end()); }
template <typename ValueType, uint64_t Chs, uint64_t Frs> [[nodiscard]] auto as_channel_range(const data<ValueType, Chs, Frs>& st) { return std::ranges::subrange(st.channels_cbegin(), st.channels_cend()); }

template <typename ValueType> [[nodiscard]]
auto make(ads::channel_count channel_count, ads::frame_count frame_count) -> data<ValueType, DYNAMIC_EXTENT, DYNAMIC_EXTENT> {
	if (channel_count.value > detail::SANE_NUMBER_OF_CHANNELS) { throw std::invalid_argument{std::format("ads::make(): Channel count {} is too high", channel_count.value)}; }
	if (frame_count.value > detail::SANE_NUMBER_OF_FRAMES)     { throw std::invalid_argument{std::format("ads::make(): Frame count {} is too high", frame_count.value)}; }
	detail::storage<ValueType, DYNAMIC_EXTENT, DYNAMIC_EXTENT> st;
	st.resize(channel_count.value);
	for (auto& channel : st) {
		channel.resize(frame_count.value);
	}
	return {std::move(st)};
}

template <typename ValueType, uint64_t Chs> [[nodiscard]]
auto make(ads::frame_count frame_count) -> data<ValueType, Chs, DYNAMIC_EXTENT> {
	if (frame_count.value > detail::SANE_NUMBER_OF_FRAMES) { throw std::invalid_argument{std::format("ads::make(): Frame count {} is too high", frame_count.value)}; }
	detail::storage<ValueType, Chs, DYNAMIC_EXTENT> st;
	for (auto& channel : st) {
		channel.resize(frame_count.value);
	}
	return {std::move(st)};
}

template <typename ValueType, uint64_t Frs> [[nodiscard]]
auto make(ads::channel_count channel_count) -> data<ValueType, DYNAMIC_EXTENT, Frs> {
	if (channel_count.value > detail::SANE_NUMBER_OF_CHANNELS) { throw std::invalid_argument{std::format("ads::make(): Channel count {} is too high", channel_count.value)}; }
	detail::storage<ValueType, DYNAMIC_EXTENT, Frs> st;
	st.resize(channel_count.value);
	return {std::move(st)};
}

template <typename ValueType, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto make() -> data<ValueType, Chs, Frs> {
	return {};
}

template <typename ValueType, uint64_t Frs> [[nodiscard]] auto make_mono() -> mono<ValueType, Frs>     { return make<ValueType, 1, Frs>(); } 
template <typename ValueType, uint64_t Frs> [[nodiscard]] auto make_stereo() -> stereo<ValueType, Frs> { return make<ValueType, 2, Frs>(); }
template <typename ValueType> [[nodiscard]] auto make_mono(ads::frame_count frame_count) -> dynamic_mono<ValueType>     { return make<ValueType, 1>(frame_count); } 
template <typename ValueType> [[nodiscard]] auto make_stereo(ads::frame_count frame_count) -> dynamic_stereo<ValueType> { return make<ValueType, 2>(frame_count); } 

template <std::ranges::input_range Frames, typename OutputIterator>
	requires
		is_frame_ref<std::iter_value_t<OutputIterator>, std::ranges::range_value_t<Frames>>
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
		is_frame_ref<std::ranges::range_value_t<Input>, std::iter_value_t<OutputIterator>>
auto interleave(Input&& input, OutputIterator output) -> void {
	for (const auto& frame : input) {
		for (const auto channel_value : frame) {
			*output++ = *channel_value;
		}
	}
}

template <std::ranges::input_range Input, typename OutputIterator>
	requires std::is_same_v<std::ranges::range_value_t<std::ranges::range_reference_t<Input>>, std::iter_value_t<OutputIterator>>
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
				*output++ = *iterators[c]++;
				progress = true;
			}
		}
	} while (progress);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, typename OutputIterator>
auto interleave(const data<ValueType, Chs, Frs>& input, OutputIterator output) -> void {
	interleave(as_channel_range(input), output);
}

template <typename ValueType>
struct interleaved {
	interleaved() = default;
	interleaved(ads::channel_count channel_count, ads::frame_count frame_count)
		: channel_count_{channel_count}
		, frame_count_{frame_count}
		, data_{make_mono<ValueType>({channel_count.value * frame_count.value})}
	{}
	[[nodiscard]] auto get_channel_count() const -> channel_count   { return channel_count_; }
	[[nodiscard]] auto get_frame_count() const -> frame_count       { return frame_count_; }
	[[nodiscard]] auto at(uint64_t index) -> ValueType&             { return data_.at(frame_idx{index}); }
	[[nodiscard]] auto at(uint64_t index) const -> const ValueType& { return data_.at(frame_idx{index}); }
	[[nodiscard]] auto begin()                                      { return data_.at().begin(); }
	[[nodiscard]] auto end()                                        { return data_.at().end(); }
	[[nodiscard]] auto begin() const                                { return data_.at().begin(); }
	[[nodiscard]] auto end() const                                  { return data_.at().end(); }
	[[nodiscard]] auto cbegin() const                               { return data_.at().begin(); }
	[[nodiscard]] auto cend() const                                 { return data_.at().end(); }
	[[nodiscard]] auto data() -> ValueType*                         { return data_.data(); }
	[[nodiscard]] auto data() const -> const ValueType*             { return data_.data(); }
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
	dynamic_mono<ValueType> data_;
};

namespace concepts {

template <typename ValueType, typename Fn> concept write_fn = detail::is_write_fn<ValueType, Fn>;
template <typename ValueType, typename Fn> concept read_fn  = detail::is_read_fn<ValueType, Fn>;

} // concepts

} // namespace ads
