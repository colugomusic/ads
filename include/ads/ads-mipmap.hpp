#pragma once

#include "ads.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace ads::mipmap_detail {

template <typename REP> [[nodiscard]] constexpr auto VALUE_MAX() -> REP    { return std::numeric_limits<REP>::max() - 1; }
template <typename REP> [[nodiscard]] constexpr auto VALUE_MIN() -> REP    { return std::numeric_limits<REP>::min(); }
template <typename REP> [[nodiscard]] constexpr auto VALUE_SILENT() -> REP { return REP(std::floor(double(VALUE_MAX<REP>()) / 2)); }

template <typename REP> struct max { REP value = VALUE_SILENT<REP>(); };
template <typename REP> struct min { REP value = VALUE_SILENT<REP>(); };

} // namespace ads::mipmap_detail

namespace ads {

struct lod_index { uint64_t value = 0; };

template <typename REP> struct mipmap_minmax { mipmap_detail::min<REP> min; mipmap_detail::max<REP> max; };
struct mipmap_region { ads::frame_idx beg; ads::frame_idx end; };

//	lower is better quality, but uses more memory
//	0: is a regular mipmap, i.e. each level is half the size of the previous one
//	1: each level is 1/3 the size of the previous one
//	2: 1/4,
//  3: 1/5,
//  etc. 
//	level zero is always the original sample size
struct mipmap_resolution { uint8_t value = 0; };

//	If the source data exceeds the range -1.0f..1.0f, set this to the max amount by
//	which it exceeds. For example if a frame of data hits -1.543f, this should be set
//	to 0.543f. This will prevent the mipmap values from being clipped to the REP range.
//	If you're dynamically writing to the mipmap from the audio thread or something then
//	you probably won't know ahead of time what that value will be, so you can just leave
//	this at 0.0f and live with the clipping.
struct max_source_clip { float value = 0.0f; };

template <typename REP> [[nodiscard]]
auto encode(max_source_clip max_source_clip, float value) -> REP {
	const auto limit = 1.0f + max_source_clip.value;
	value  = std::clamp(value, -limit, limit);
	value /= limit;
	value += 1;
	value *= mipmap_detail::VALUE_SILENT<REP>();
	return static_cast<REP>(value);
}

template <typename REP> [[nodiscard]]
auto encode(float value) -> REP {
	return encode<REP>({}, value);
}

} // namespace ads

namespace ads::mipmap_detail {

struct bin_size { int value = 0; };

template <typename REP, uint64_t Chs>
struct lod {
	ads::lod_index index;
	mipmap_detail::bin_size bin_size;
	ads::data<mipmap_minmax<REP>, Chs, ads::DYNAMIC_EXTENT> st;
	mipmap_region valid_region;
};

template <typename REP, uint64_t Chs, uint64_t Frs>
struct lod0 {
	ads::data<REP, Chs, Frs> st;
	mipmap_region valid_region;
};

template <typename REP, uint64_t Chs, uint64_t Frs>
struct impl {
	mipmap_resolution res;
	ads::max_source_clip max_source_clip;
	mipmap_detail::lod0<REP, Chs, Frs> lod0;
	std::vector<mipmap_detail::lod<REP, Chs>> lods;
};

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto read(const mipmap_detail::impl<REP, Chs, Frs>& impl, ads::lod_index lod_index, ads::channel_idx channel, ads::frame_idx lod_frame) -> mipmap_minmax<REP>;

struct lerp_helper {
	struct { ads::frame_idx a; ads::frame_idx b; } index;
	float t;
};

[[nodiscard]] inline
auto is_empty(const mipmap_region& r) -> bool {
	return r.beg >= r.end;
}

template <typename REP, uint64_t Chs, uint64_t Frs>
auto generate(const mipmap_detail::impl<REP, Chs, Frs>& impl, mipmap_detail::lod<REP, Chs>* lod, mipmap_resolution res, ads::channel_idx ch, ads::frame_idx fr) -> void {
	auto min = VALUE_MAX<REP>();
	auto max = VALUE_MIN<REP>();
	auto beg = fr.value * res.value;
	auto end = beg + res.value;
	for (uint64_t i = beg; i < end; i++) {
		const auto minmax = mipmap_detail::read(impl, ads::lod_index{lod->index.value - 1}, ch, {i});
		if (minmax.min.value < min) min = minmax.min.value;
		if (minmax.max.value > max) max = minmax.max.value;
	}
	lod->st.set(ch, fr, { min, max });
}

template <typename REP, uint64_t Chs, uint64_t Frs>
auto generate(const mipmap_detail::impl<REP, Chs, Frs>& impl, mipmap_detail::lod<REP, Chs>* lod, mipmap_resolution res, ads::channel_idx ch, mipmap_region region) -> void {
	for (auto fr = region.beg; fr < region.end; fr++) {
		generate(impl, lod, res, ch, fr);
	}
}

template <typename REP, uint64_t Chs, uint64_t Frs>
auto generate(const mipmap_detail::impl<REP, Chs, Frs>& impl, mipmap_detail::lod<REP, Chs>* lod, mipmap_resolution res, mipmap_region region) -> void {
	if (region.beg < lod->valid_region.beg) { lod->valid_region.beg = region.beg; }
	if (region.end > lod->valid_region.end) { lod->valid_region.end = region.end; }
	const auto channel_count = get_channel_count(impl);
	for (ads::channel_idx ch = {0ULL}; ch < channel_count; ch++) {
		generate(impl, lod, res, ch, region);
	}
}

template <typename Result, typename Value> [[nodiscard]]
auto lerp(Value a, Value b, float t) -> Result {
	const auto af = static_cast<float>(a);
	const auto bf = static_cast<float>(b);
	return static_cast<Result>((t * (bf - af)) + af);
}

template <typename Result, typename Value> [[nodiscard]]
auto lerp(const lerp_helper& lh, Value a, Value b) -> Result {
	return lerp<Result>(a, b, lh.t);
}

[[nodiscard]] inline
auto make_lerp_helper(float frame) -> lerp_helper {
	lerp_helper lh;
	lh.index.a.value = static_cast<uint64_t>(std::floor(frame));
	lh.index.b.value = static_cast<uint64_t>(std::ceil(frame));
	lh.t = frame - lh.index.a.value;
	return lh;
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto get_channel_count(const mipmap_detail::impl<REP, Chs, Frs>& impl) -> ads::channel_count {
	return impl.lod0.st.get_channel_count();
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto get_frame_count(const mipmap_detail::impl<REP, Chs, Frs>& impl) -> ads::frame_count {
	return impl.lod0.st.get_frame_count();
}

template <typename REP, uint64_t Chs> [[nodiscard]]
auto make_lod(ads::lod_index index, ads::frame_count frame_count, mipmap_resolution res) -> mipmap_detail::lod<REP, Chs> {
	mipmap_detail::lod<REP, Chs> lod;
	lod.index    = index;
	lod.bin_size = {int(std::pow(res.value, index.value))};
	lod.st.resize(frame_count, {VALUE_SILENT<REP>(), VALUE_SILENT<REP>()});
	return lod;
}

template <typename REP>
auto make_lod(ads::lod_index index, ads::channel_count channel_count, ads::frame_count frame_count, mipmap_resolution res) -> mipmap_detail::lod<REP, ads::DYNAMIC_EXTENT> {
	mipmap_detail::lod<REP, ads::DYNAMIC_EXTENT> lod;
	lod.index    = index;
	lod.bin_size = {int(std::pow(res.value, index.value))};
	lod.st.resize(channel_count, frame_count, {VALUE_SILENT<REP>(), VALUE_SILENT<REP>()});
	return lod;
}

template <typename REP, uint64_t Chs> [[nodiscard]]
auto read(const mipmap_detail::lod<REP, Chs>& lod, ads::channel_idx ch, ads::frame_idx lod_frame) -> mipmap_minmax<REP> {
	if (is_empty(lod.valid_region)) {
		return {};
	}
	lod_frame.value = std::min(lod.st.get_frame_count().value - 1, lod_frame.value);
	if (lod_frame.value < lod.valid_region.beg || lod_frame.value >= lod.valid_region.end) {
		return {};
	}
	return lod.st.at(ch, lod_frame);
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto read(const mipmap_detail::impl<REP, Chs, Frs>& impl, ads::channel_idx ch, ads::frame_idx fr) -> REP {
	if (is_empty(impl.lod0.valid_region)) {
		return VALUE_SILENT<REP>();
	}
	fr.value = std::min(impl.lod0.st.get_frame_count().value - 1, fr.value);
	if (fr < impl.lod0.valid_region.beg || fr >= impl.lod0.valid_region.end) {
		return VALUE_SILENT<REP>();
	}
	return impl.lod0.st.at(ch, fr);
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto read(const mipmap_detail::impl<REP, Chs, Frs>& impl, ads::channel_idx ch, float frame) -> REP {
	const auto lerp_frame = make_lerp_helper(frame);
	const auto a_value    = read(impl, ch, lerp_frame.index.a);
	const auto b_value    = read(impl, ch, lerp_frame.index.b);
	return lerp<REP>(lerp_frame, a_value, b_value);
}

template <typename REP> [[nodiscard]]
auto as_float(REP value, max_source_clip max_clip) -> float {
	const auto limit = 1.0f + max_clip.value;
	auto fvalue = float(value);
	fvalue /= VALUE_SILENT<REP>();
	fvalue -= 1.0f;
	fvalue *= limit;
	return fvalue;
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto init(mipmap_detail::impl<REP, Chs, Frs>* impl, mipmap_resolution res, ads::max_source_clip max_source_clip) -> void {
	impl->res             = {REP(res.value + 2)};
	impl->max_source_clip = max_source_clip;
	const auto frame_count = get_frame_count(*impl);
	if constexpr (Chs == ads::DYNAMIC_EXTENT) {
		const auto channel_count = get_channel_count(*impl);
		auto size  = ads::frame_count{frame_count.value / impl->res.value};
		auto index = lod_index{1};
		while (size > 0ULL) {
			impl->lods.push_back(mipmap_detail::make_lod<REP>(index, channel_count, {size}, impl->res));
			index.value++;
			size /= impl->res.value;
		}
	}
	else {
		auto size  = ads::frame_count{frame_count.value / impl->res.value};
		auto index = lod_index{1};
		while (size > 0ULL) {
			impl->lods.push_back(mipmap_detail::make_lod<REP, Chs>(index, {size}, impl->res));
			index.value++;
			size /= impl->res.value;
		}
	}
}

template <typename REP> [[nodiscard]]
auto make(ads::channel_count channel_count, ads::frame_count frame_count, mipmap_resolution res, ads::max_source_clip max_source_clip) -> mipmap_detail::impl<REP, ads::DYNAMIC_EXTENT, ads::DYNAMIC_EXTENT> {
	mipmap_detail::impl<REP, ads::DYNAMIC_EXTENT, ads::DYNAMIC_EXTENT> impl;
	impl->lod0.st.resize(channel_count, VALUE_SILENT<REP>());
	init(&impl, res, max_source_clip);
	return impl;
}

template <typename REP, uint64_t Chs> [[nodiscard]]
auto make(ads::frame_count frame_count, mipmap_resolution res, ads::max_source_clip max_source_clip) -> mipmap_detail::impl<REP, Chs, ads::DYNAMIC_EXTENT> {
	mipmap_detail::impl<REP, Chs, ads::DYNAMIC_EXTENT> impl;
	impl->lod0.st.resize(frame_count, VALUE_SILENT<REP>());
	init(&impl, res, max_source_clip);
	return impl;
}

template <typename REP, uint64_t Frs> [[nodiscard]]
auto make(ads::channel_count channel_count, mipmap_resolution res, ads::max_source_clip max_source_clip) -> mipmap_detail::impl<REP, ads::DYNAMIC_EXTENT, Frs> {
	mipmap_detail::impl<REP, ads::DYNAMIC_EXTENT, Frs> impl;
	impl->lod0.st.resize(channel_count);
	init(&impl, res, max_source_clip);
	return impl;
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto make(mipmap_resolution res, ads::max_source_clip max_source_clip) -> mipmap_detail::impl<REP, Chs, Frs> {
	mipmap_detail::impl<REP, Chs, Frs> impl;
	init(&impl, res, max_source_clip);
	return impl;
}

template <typename REP, uint64_t Chs, uint64_t Frs>
auto clear(mipmap_detail::impl<REP, Chs, Frs>* impl) -> void {
	impl->lod0.valid_region = {};
	for (auto& lod : impl->lods) {
		lod.valid_region = {};
	}
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto encode(const mipmap_detail::impl<REP, Chs, Frs>& impl, float value) -> REP {
	return encode<REP>(impl.max_source_clip, value);
}

template <typename REP> [[nodiscard]]
auto lerp(mipmap_minmax<REP> a, mipmap_minmax<REP> b, float t) -> mipmap_minmax<REP> {
	const auto min = mipmap_detail::lerp<REP>(a.min.value, b.min.value, t);
	const auto max = mipmap_detail::lerp<REP>(a.max.value, b.max.value, t);
	return { min, max };
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto lod_count(const mipmap_detail::impl<REP, Chs, Frs>& impl) -> size_t {
	return impl.lods.size() + 1;
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto read(const mipmap_detail::impl<REP, Chs, Frs>& impl, ads::lod_index lod_index, ads::channel_idx ch, float frame) -> mipmap_minmax<REP> {
	assert(ch < get_channel_count(impl));
	if (lod_index.value == 0) {
		const auto value = mipmap_detail::read(impl, ch, frame);
		return { value, value };
	}
	lod_index.value = std::min(lod_index.value, impl.lods.size());
	const auto& lod = impl.lods[lod_index.value - 1];
	frame /= lod.bin_size.value;
	const auto lerp_frame = mipmap_detail::make_lerp_helper(frame);
	const auto a_value    = mipmap_detail::read(lod, ch, lerp_frame.index.a);
	const auto b_value    = mipmap_detail::read(lod, ch, lerp_frame.index.b);
	const auto min        = mipmap_detail::min<REP>{mipmap_detail::lerp<REP>(lerp_frame, a_value.min.value, b_value.min.value)};
	const auto max        = mipmap_detail::max<REP>{mipmap_detail::lerp<REP>(lerp_frame, a_value.max.value, b_value.max.value)};
	return { min, max };
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto read(const mipmap_detail::impl<REP, Chs, Frs>& impl, float lod, ads::channel_idx ch, float frame) -> mipmap_minmax<REP> {
	assert(ch < get_channel_count(impl));
	assert(lod >= 0);
	const auto lerp_lod = mipmap_detail::make_lerp_helper(lod);
	const auto a_value  = read(impl, ads::lod_index{lerp_lod.index.a.value}, ch, frame);
	const auto b_value  = read(impl, ads::lod_index{lerp_lod.index.b.value}, ch, frame);
	const auto min      = mipmap_detail::min<REP>{mipmap_detail::lerp<REP>(lerp_lod, a_value.min.value, b_value.min.value)};
	const auto max      = mipmap_detail::max<REP>{mipmap_detail::lerp<REP>(lerp_lod, a_value.max.value, b_value.max.value)};
	return { min, max };
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto read(const mipmap_detail::impl<REP, Chs, Frs>& impl, ads::lod_index lod_index, ads::channel_idx ch, ads::frame_idx lod_frame) -> mipmap_minmax<REP> {
	assert(ch < get_channel_count(impl));
	if (lod_index.value == 0) {
		const auto value = mipmap_detail::read(impl, ch, lod_frame);
		return { value, value };
	}
	assert(lod_index.value <= impl.lods.size());
	const auto& lod = impl.lods[lod_index.value - 1];
	return mipmap_detail::read(lod, ch, lod_frame);
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto read(const mipmap_detail::impl<REP, Chs, Frs>& impl, float lod, ads::channel_idx ch, ads::frame_idx lod_frame) -> mipmap_minmax<REP> {
	assert(ch < get_channel_count(impl));
	assert(lod >= 0);
	const auto lerp_lod = mipmap_detail::make_lerp_helper(lod);
	const auto a_value  = read(impl, ads::lod_index{lerp_lod.index.a.value}, ch, lod_frame);
	const auto b_value  = read(impl, ads::lod_index{lerp_lod.index.b.value}, ch, lod_frame);
	return mipmap_detail::lerp(a_value, b_value, lerp_lod.t);
}

template <typename REP, uint64_t Chs, uint64_t Frs>
auto set(mipmap_detail::impl<REP, Chs, Frs>* impl, ads::channel_idx ch, ads::frame_idx fr, float value) -> void {
	impl->lod0.st.set(ch, fr, encode(impl->max_source_clip, value));
}

template <typename REP, uint64_t Chs, uint64_t Frs, typename WriterFn>
	requires ads::concepts::is_single_channel_write_fn<REP, WriterFn>
auto write(mipmap_detail::impl<REP, Chs, Frs>* impl, ads::channel_idx ch, ads::frame_idx start, ads::frame_count frames_to_write, WriterFn writer) -> ads::frame_count {
	return impl->lod0.st.write(ch, start, frames_to_write, writer);
}

template <typename REP, uint64_t Chs, uint64_t Frs, typename WriterFn>
	requires ads::concepts::is_multi_channel_write_fn<REP, WriterFn>
auto write(mipmap_detail::impl<REP, Chs, Frs>* impl, ads::frame_idx start, ads::frame_count frames_to_write, WriterFn writer) -> ads::frame_count {
	return impl->lod0.st.write(start, frames_to_write, writer);
}

template <typename REP, uint64_t Chs, uint64_t Frs>
auto write(mipmap_detail::impl<REP, Chs, Frs>* impl, ads::frame_idx start, ads::frame_count frames_to_write, auto provider, auto conversion) -> ads::frame_count {
	return write(impl, start, frames_to_write, [impl, provider, conversion](REP* buffer, ads::channel_idx ch, ads::frame_idx start, ads::frame_count frames_to_write) {
		for (frame_idx i = {0ULL}; i < frames_to_write; i++) {
			buffer[i.value] = conversion(provider(ch, i));
		}
		return frames_to_write;
	});
}

template <typename REP, uint64_t Chs, uint64_t Frs>
auto write(mipmap_detail::impl<REP, Chs, Frs>* impl, ads::channel_idx ch, ads::frame_idx start, ads::frame_count frames_to_write, auto provider, auto conversion) -> ads::frame_count {
	return write(impl, ch, start, frames_to_write, [impl, provider](REP* buffer, ads::frame_idx start, ads::frame_count frames_to_write) {
		for (frame_idx i = {0ULL}; i < frames_to_write; i++) {
			buffer[i.value] = conversion(provider(ch, i));
		}
		return frames_to_write;
	});
}

template <typename REP, uint64_t Chs, uint64_t Frs, typename ProviderFn>
	requires ads::concepts::is_multi_channel_provider_fn<uint8_t, ProviderFn>
auto write(mipmap_detail::impl<REP, Chs, Frs>* impl, ads::frame_idx start, ads::frame_count frames_to_write, ProviderFn provider) -> ads::frame_count {
	auto conversion = [impl](uint8_t value) { return value; };
	return write(impl, start, frames_to_write, provider, conversion);
}

template <typename REP, uint64_t Chs, uint64_t Frs, typename ProviderFn>
	requires ads::concepts::is_single_channel_provider_fn<uint8_t, ProviderFn>
auto write(mipmap_detail::impl<REP, Chs, Frs>* impl, ads::channel_idx ch, ads::frame_idx start, ads::frame_count frames_to_write, ProviderFn provider) -> ads::frame_count {
	auto conversion = [impl](uint8_t value) { return value; };
	return write(impl, ch, start, frames_to_write, provider, conversion);
}

template <typename REP, uint64_t Chs, uint64_t Frs, typename ProviderFn>
	requires ads::concepts::is_multi_channel_provider_fn<float, ProviderFn>
auto write(mipmap_detail::impl<REP, Chs, Frs>* impl, ads::frame_idx start, ads::frame_count frames_to_write, ProviderFn provider) -> ads::frame_count {
	auto conversion = [impl](float value) { return encode(*impl, value); };
	return write(impl, start, frames_to_write, provider, conversion);
}

template <typename REP, uint64_t Chs, uint64_t Frs, typename ProviderFn>
	requires ads::concepts::is_single_channel_provider_fn<float, ProviderFn>
auto write(mipmap_detail::impl<REP, Chs, Frs>* impl, ads::channel_idx ch, ads::frame_idx start, ads::frame_count frames_to_write, ProviderFn provider) -> ads::frame_count {
	auto conversion = [impl](float value) { return encode(*impl, value); };
	return write(impl, ch, start, frames_to_write, provider, conversion);
}

template <typename REP, uint64_t Chs, uint64_t Frs>
auto update(mipmap_detail::impl<REP, Chs, Frs>* impl, mipmap_region region) -> void {
	assert(region.end > region.beg);
	assert(region.end <= get_frame_count(*impl).value);
	if (region.beg < impl->lod0.valid_region.beg) impl->lod0.valid_region.beg = region.beg;
	if (region.end > impl->lod0.valid_region.end) impl->lod0.valid_region.end = region.end;
	for (auto& lod : impl->lods) {
		region.beg /= impl->res.value;
		region.end /= impl->res.value;
		mipmap_detail::generate(*impl, &lod, impl->res, region);
	}
}

template <typename REP, uint64_t Chs, uint64_t Frs> [[nodiscard]]
auto bin_size_to_lod(const mipmap_detail::impl<REP, Chs, Frs>& impl, float bin_size) -> float {
	if (bin_size <= 1) {
		return 0.0;
	}
	return float(std::log(bin_size) / std::log(impl.res.value));
}

} // ads::mipmap_detail

namespace ads {

template <typename REP> [[nodiscard]]
auto as_float(REP value, max_source_clip max_clip) -> float {
	return mipmap_detail::as_float(value, max_clip);
}

template <typename REP> [[nodiscard]]
auto lerp(mipmap_minmax<REP> a, mipmap_minmax<REP> b, float t) -> mipmap_minmax<REP> {
	return mipmap_detail::lerp(a, b, t);
}

// Mipmap representation of audio data, intended for waveform rendering,
// but also useful for audio analysis.
//
// All memory is allocated in mipmap_detail::make().
//
// You can call write() in the audio thread if you're feeling adventurous
//
// There is no locking. It is the caller's responsibility not to
// simultaneously read and write to the same region
//
// REP is the underlying unsigned integer type which will be used to
// encode the frame data. uint8_t is probably fine for most use cases.
template <typename REP, uint64_t Chs, uint64_t Frs>
struct mipmap {
	mipmap()                             = default;
	mipmap(const mipmap&)                = default;
	mipmap(mipmap&&) noexcept            = default;
	mipmap& operator=(const mipmap&)     = default;
	mipmap& operator=(mipmap&&) noexcept = default;
	mipmap(ads::channel_count channel_count, ads::frame_count frame_count, mipmap_resolution res, ads::max_source_clip max_source_clip)
		requires (Chs == ads::DYNAMIC_EXTENT) && (Frs == ads::DYNAMIC_EXTENT)
	{
		impl_.lod0.st.resize(channel_count, frame_count, mipmap_detail::VALUE_SILENT<REP>());
		init(&impl_, res, max_source_clip);
	}
	mipmap(ads::frame_count frame_count, mipmap_resolution res, ads::max_source_clip max_source_clip)
		requires (Chs != ads::DYNAMIC_EXTENT) && (Frs == ads::DYNAMIC_EXTENT)
	{
		impl_.lod0.st.resize(frame_count, mipmap_detail::VALUE_SILENT<REP>());
		init(&impl_, res, max_source_clip);
	}
	mipmap(ads::channel_count channel_count, mipmap_resolution res, ads::max_source_clip max_source_clip)
		requires (Chs == ads::DYNAMIC_EXTENT) && (Frs != ads::DYNAMIC_EXTENT)
	{
		impl_.lod0.st.resize(channel_count, mipmap_detail::VALUE_SILENT<REP>());
		init(&impl_, res, max_source_clip);
	}
	mipmap(mipmap_resolution res, ads::max_source_clip max_source_clip)
		requires (Chs != ads::DYNAMIC_EXTENT) && (Frs != ads::DYNAMIC_EXTENT)
	{
		init(&impl_, res, max_source_clip);
	}
	[[nodiscard]]
	auto as_float(REP value) const -> float {
		return mipmap_detail::as_float(value, impl_.max_source_clip);
	}
	[[nodiscard]]
	auto bin_size_to_lod(float bin_size) const -> float {
		return mipmap_detail::bin_size_to_lod(impl_, bin_size);
	}
	auto clear() -> void {
		mipmap_detail::clear(&impl_);
	}
	[[nodiscard]]
	auto encode(float value) const -> REP {
		return mipmap_detail::encode(impl_, value);
	}
	// Interpolate between two frames of the same LOD
	[[nodiscard]]
	auto read(ads::lod_index lod_index, ads::channel_idx ch, float frame) const -> mipmap_minmax<REP> {
		return mipmap_detail::read(impl_, lod_index, ch, frame);
	}
	// Interpolate between two LODs and two frames
	[[nodiscard]]
	auto read(float lod, ads::channel_idx ch, float frame) const -> mipmap_minmax<REP> {
		return mipmap_detail::read(impl_, lod, ch, frame);
	}
	// Interpolate between two LODs
	[[nodiscard]]
	auto read(float lod, ads::channel_idx ch, ads::frame_idx lod_frame) const -> mipmap_minmax<REP> {
		return mipmap_detail::read(impl_, lod, ch, lod_frame);
	}
	// No interpolation.
	// lod_frame is the LOD-local frame, i.e. in a 100-frame sample
	// with detail=0 so each LOD is half the size of the previous,
	// the lod_frame parameter would range from
	// 0-99 for LOD 0,
	// 0-49 for LOD 1,
	// 0-24 for LOD 2, etc.
	[[nodiscard]]
	auto read(ads::lod_index lod_index, ads::channel_idx ch, ads::frame_idx lod_frame) const -> mipmap_minmax<REP> {
		return mipmap_detail::read(impl_, lod_index, ch, lod_frame);
	}
	// Writes level zero data. Mipmap data for the other levels won't be generated until update() is called
	auto set(ads::channel_idx ch, ads::frame_idx fr, float value) -> void {
		mipmap_detail::set(&impl_, ch, fr, value);
	}
	// Generates mipmap data for the specified (top-level) region
	// This does both reading and writing of frames within the
	// region at all mipmap levels
	auto update(mipmap_region region) -> void {
		mipmap_detail::update(&impl_, region);
	};
	// Write level zero frame data beginning at frame_begin, using a custom writer function
	// The writer needs to encode the frames to the range VALUE_MIN<REP>..VALUE_MAX<REP> itself
	template <typename WriterFn>
		requires ads::concepts::is_single_channel_write_fn<REP, WriterFn>
	auto write(ads::channel_idx ch, ads::frame_idx start, ads::frame_count frame_count, WriterFn writer) -> ads::frame_count {
		return mipmap_detail::write(&impl_, ch, start, frame_count, writer);
	}
	// Write level zero frame data using a custom provider function which supplies the float data.
	template <typename ProviderFn>
		requires ads::concepts::is_multi_channel_provider_fn<float, ProviderFn>
	auto write(ads::frame_idx start, ads::frame_count frame_count, ProviderFn provider) -> ads::frame_count {
		return mipmap_detail::write(&impl_, start, frame_count, provider);
	}
	template <typename ProviderFn>
		requires ads::concepts::is_single_channel_provider_fn<float, ProviderFn>
	auto write(ads::channel_idx ch, ads::frame_idx start, ads::frame_count frame_count, ProviderFn provider) -> ads::frame_count {
		return mipmap_detail::write(&impl_, ch, start, frame_count, provider);
	}
	// Write level zero frame data using a custom provider function which supplies pre-encoded level zero data.
	template <typename ProviderFn>
		requires ads::concepts::is_multi_channel_provider_fn<uint8_t, ProviderFn>
	auto write(ads::frame_idx start, ads::frame_count frame_count, ProviderFn provider) -> ads::frame_count {
		return mipmap_detail::write(&impl_, start, frame_count, provider);
	}
	template <typename ProviderFn>
		requires ads::concepts::is_single_channel_provider_fn<uint8_t, ProviderFn>
	auto write(ads::channel_idx ch, ads::frame_idx start, ads::frame_count frame_count, ProviderFn provider) -> ads::frame_count {
		return mipmap_detail::write(&impl_, ch, start, frame_count, provider);
	}
private:
	mipmap_detail::impl<REP, Chs, Frs> impl_;
};

} // ads
