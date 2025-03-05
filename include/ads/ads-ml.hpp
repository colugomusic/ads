#pragma once

#include "ads.hpp"
#pragma warning(push, 0)
#include <DSP/MLDSPOps.h>
#pragma warning(pop)

namespace ads {

inline
auto copy(const ml::DSPVector& from, float* to, ads::frame_count frame_count) -> ads::frame_count {
	if (frame_count.value <= kFloatsPerDSPVector) { std::copy(from.getConstBuffer(), from.getConstBuffer() + frame_count.value, to); }
	else                                          { ml::storeAligned(from, to); }
	return frame_count;
}

inline
auto copy(const float* from, ml::DSPVector* to, ads::frame_count frame_count) -> ads::frame_count {
	if (frame_count.value <= kFloatsPerDSPVector) { std::copy(from, from + frame_count.value, to->getBuffer()); }
	else                                          { ml::loadAligned(*to, from); }
	return frame_count;
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto read(const data<ValueType, Chs, Frs>& src, frame_idx start, ml::DSPVectorDynamic* dest) -> frame_count {
	return src.read(start, {kFloatsPerDSPVector}, [dest](const float* buffer, channel_idx ch, frame_idx frame_start, ads::frame_count frame_count) {
		return copy(buffer, &dest->operator[](int(ch.value)), frame_count);
	});
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto read(const data<ValueType, Chs, Frs>& src, ml::DSPVectorDynamic* dest) -> frame_count {
	return read(src, frame_idx{0}, dest);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
	requires (detail::is_mono_data<Chs>)
auto read(const data<ValueType, Chs, Frs>& src, frame_idx start, ml::DSPVector* dst) -> frame_count {
	return src.read(start, {kFloatsPerDSPVector}, [dst](const float* buffer, frame_idx frame_start, ads::frame_count frame_count) {
		return copy(buffer, dst, frame_count);
	});
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
	requires (detail::is_mono_data<Chs>)
auto read(const data<ValueType, Chs, Frs>& src, ml::DSPVector* dst) -> frame_count {
	return read(src, frame_idx{0}, dst);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto read(const data<ValueType, Chs, Frs>& src, channel_idx ch, frame_idx start, ml::DSPVector* dst) -> frame_count {
	return src.read(ch, start, {kFloatsPerDSPVector}, [dst](const float* buffer, frame_idx frame_start, ads::frame_count frame_count) {
		return copy(buffer, dst, frame_count);
	});
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto read(const data<ValueType, Chs, Frs>& src, channel_idx ch, ml::DSPVector* dst) -> frame_count {
	return read(src, ch, frame_idx{0}, dst);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto write(data<ValueType, Chs, Frs>* dst, frame_idx start, const ml::DSPVectorDynamic& src) -> frame_count {
	return dst->write(start, {kFloatsPerDSPVector}, [&src](float* buffer, channel_idx ch, frame_idx frame_start, ads::frame_count frame_count) {
		return copy(src.operator[](int(ch.value)), buffer, frame_count);
	});
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto write(data<ValueType, Chs, Frs>* dst, const ml::DSPVectorDynamic& src) -> frame_count {
	return write(dst, frame_idx{0}, src);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
	requires (detail::is_mono_data<Chs>)
auto write(data<ValueType, Chs, Frs>* dst, frame_idx start, const ml::DSPVector& signal) -> frame_count {
	return dst->write(start, {kFloatsPerDSPVector}, [&signal](float* buffer, frame_idx frame_start, ads::frame_count frame_count) {
		return copy(signal, buffer, frame_count);
	});
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
	requires (detail::is_mono_data<Chs>)
auto write(data<ValueType, Chs, Frs>* dst, const ml::DSPVector& signal) -> frame_count {
	return write(dst, frame_idx{0}, signal);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto write(data<ValueType, Chs, Frs>* dst, channel_idx ch, frame_idx start, const ml::DSPVector& signal) -> frame_count {
	return dst->write(ch, start, {kFloatsPerDSPVector}, [&signal](float* buffer, frame_idx frame_start, ads::frame_count frame_count) {
		return copy(signal, buffer, frame_count);
	});
}

template <typename ValueType, uint64_t Chs, uint64_t Frs>
auto write(data<ValueType, Chs, Frs>* dst, channel_idx ch, const ml::DSPVector& signal) -> frame_count {
	return write(dst, ch, frame_idx{0}, signal);
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, size_t ROWS>
auto write(data<ValueType, Chs, Frs>* dst, frame_idx start, const ml::DSPVectorArray<ROWS>& signal) -> frame_count {
	return dst->write(start, {kFloatsPerDSPVector}, [&signal](float* buffer, channel_idx ch, frame_idx frame_start, ads::frame_count frame_count) {
		return copy(signal.constRow(int(ch.value)), buffer, frame_count);
	});
}

template <typename ValueType, uint64_t Chs, uint64_t Frs, size_t ROWS>
auto write(data<ValueType, Chs, Frs>* dst, const ml::DSPVectorArray<ROWS>& signal) -> frame_count {
	return write(dst, frame_idx{0}, signal);
}

} // namespace ads

