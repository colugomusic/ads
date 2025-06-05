#pragma once

#include "ads-concepts-basic.hpp"
#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>

namespace ads {

struct channel_count { uint64_t value = 0; };
struct channel_idx   { uint64_t value = 0; };
struct frame_count   { uint64_t value = 0; };
struct frame_idx     { int64_t value = 0; };

[[nodiscard]] constexpr inline auto operator*(frame_count lhs, std::integral auto rhs) -> frame_count { return {lhs.value * rhs}; }
[[nodiscard]] constexpr inline auto operator*(frame_idx lhs, std::integral auto rhs) -> frame_idx     { return {lhs.value * static_cast<int64_t>(rhs)}; }
[[nodiscard]] constexpr inline auto operator*(std::integral auto lhs, frame_idx rhs) -> frame_idx     { return {lhs * rhs.value}; }
[[nodiscard]] constexpr inline auto operator+(double lhs, frame_count rhs) -> double                  { return lhs + rhs.value; }
[[nodiscard]] constexpr inline auto operator+(frame_count lhs, frame_idx rhs) -> frame_count          { return {lhs.value + rhs.value}; }
[[nodiscard]] constexpr inline auto operator%(frame_count lhs, std::integral auto rhs) -> frame_count { return {lhs.value % rhs}; }
[[nodiscard]] constexpr inline auto operator%(frame_idx lhs, std::integral auto rhs) -> frame_idx     { return {lhs.value % static_cast<int64_t>(rhs)}; }
[[nodiscard]] constexpr inline auto operator%(frame_idx lhs, frame_count rhs) -> frame_idx            { return {lhs.value % static_cast<int64_t>(rhs.value)}; }
[[nodiscard]] constexpr inline auto operator+(channel_idx lhs, std::integral auto rhs) -> channel_idx { return {lhs.value + rhs}; }
[[nodiscard]] constexpr inline auto operator+(frame_idx lhs, frame_idx rhs) -> frame_idx              { return {lhs.value + rhs.value}; }
[[nodiscard]] constexpr inline auto operator+(frame_idx lhs, frame_count rhs) -> frame_idx            { return {lhs.value + static_cast<int64_t>(rhs.value)}; }
[[nodiscard]] constexpr inline auto operator+(frame_idx lhs, std::integral auto rhs) -> frame_idx     { return {lhs.value + static_cast<int64_t>(rhs)}; }
[[nodiscard]] constexpr inline auto operator+(std::integral auto lhs, frame_idx rhs) -> frame_idx     { return {lhs + rhs.value}; }
[[nodiscard]] constexpr inline auto operator-(frame_count lhs, frame_idx rhs) -> frame_count          { return {lhs.value - rhs.value}; }
[[nodiscard]] constexpr inline auto operator-(frame_count lhs, frame_count rhs) -> frame_count        { return {lhs.value - rhs.value}; }
[[nodiscard]] constexpr inline auto operator-(frame_count lhs, std::integral auto rhs) -> frame_count { return {lhs.value - rhs}; }
[[nodiscard]] constexpr inline auto operator-(frame_idx lhs, frame_idx rhs) -> frame_idx              { return {lhs.value - rhs.value}; }
[[nodiscard]] constexpr inline auto operator-(frame_idx lhs, std::integral auto rhs) -> frame_idx     { return {lhs.value - static_cast<int64_t>(rhs)}; }
[[nodiscard]] constexpr inline auto operator-(std::integral auto lhs, frame_idx rhs) -> frame_idx     { return {lhs - rhs.value}; }
[[nodiscard]] constexpr inline auto operator/(frame_count lhs, std::integral auto rhs) -> frame_count { return {lhs.value / rhs}; }
[[nodiscard]] constexpr inline auto operator/(frame_idx lhs, frame_count rhs) -> frame_idx            { return {lhs.value / static_cast<int64_t>(rhs.value)}; }
[[nodiscard]] constexpr inline auto operator/(frame_idx lhs, std::integral auto rhs) -> frame_idx     { return {lhs.value / static_cast<int64_t>(rhs)}; }
[[nodiscard]] constexpr inline auto operator<=>(channel_count lhs, channel_count rhs)             { return lhs.value <=> rhs.value; }
[[nodiscard]] constexpr inline auto operator<=>(channel_count lhs, channel_idx rhs)               { return lhs.value <=> rhs.value; }
[[nodiscard]] constexpr inline auto operator<=>(channel_count lhs, concepts::arithmetic auto rhs) { return lhs.value <=> rhs; }
[[nodiscard]] constexpr inline auto operator<=>(channel_idx lhs, channel_count rhs)               { return lhs.value <=> rhs.value; }
[[nodiscard]] constexpr inline auto operator<=>(channel_idx lhs, channel_idx rhs)                 { return lhs.value <=> rhs.value; }
[[nodiscard]] constexpr inline auto operator<=>(concepts::arithmetic auto lhs, frame_count rhs)   { return lhs <=> rhs.value; }
[[nodiscard]] constexpr inline auto operator<=>(concepts::arithmetic auto lhs, frame_idx rhs)     { return lhs <=> rhs.value; }
[[nodiscard]] constexpr inline auto operator<=>(frame_count lhs, concepts::arithmetic auto rhs)   { return lhs.value <=> rhs; }
[[nodiscard]] constexpr inline auto operator<=>(frame_count lhs, frame_count rhs)                 { return lhs.value <=> rhs.value; }
[[nodiscard]] constexpr inline auto operator<=>(frame_count lhs, frame_idx rhs)                   { return lhs.value <=> rhs.value; }
[[nodiscard]] constexpr inline auto operator<=>(frame_idx lhs, concepts::arithmetic auto rhs)     { return lhs.value <=> rhs; }
[[nodiscard]] constexpr inline auto operator<=>(frame_idx lhs, frame_count rhs)                   { return lhs.value <=> rhs.value; }
[[nodiscard]] constexpr inline auto operator<=>(frame_idx lhs, frame_idx rhs)                     { return lhs.value <=> rhs.value; }
[[nodiscard]] constexpr inline auto operator==(channel_count lhs, channel_count rhs)         { return lhs.value == rhs.value; }
[[nodiscard]] constexpr inline auto operator==(channel_count lhs, std::integral auto rhs)    { return lhs.value == rhs; }
[[nodiscard]] constexpr inline auto operator==(channel_count lhs, channel_idx rhs)           { return lhs.value == rhs.value; }
[[nodiscard]] constexpr inline auto operator==(channel_idx lhs, channel_count rhs)           { return lhs.value == rhs.value; }
[[nodiscard]] constexpr inline auto operator==(channel_idx lhs, channel_idx rhs)             { return lhs.value == rhs.value; }
[[nodiscard]] constexpr inline auto operator==(frame_count lhs, frame_count rhs)             { return lhs.value == rhs.value; }
[[nodiscard]] constexpr inline auto operator==(frame_count lhs, frame_idx rhs)               { return lhs.value == rhs.value; }
[[nodiscard]] constexpr inline auto operator==(frame_count lhs, std::integral auto rhs)      { return lhs.value == rhs; }
[[nodiscard]] constexpr inline auto operator==(frame_idx lhs, frame_count rhs)               { return lhs.value == rhs.value; }
[[nodiscard]] constexpr inline auto operator==(frame_idx lhs, frame_idx rhs)                 { return lhs.value == rhs.value; }
[[nodiscard]] constexpr inline auto operator==(frame_idx lhs, std::integral auto rhs)        { return lhs.value == rhs; }
[[nodiscard]] constexpr inline auto operator!=(channel_count lhs, std::integral auto rhs)    { return lhs.value != rhs; }
[[nodiscard]] constexpr inline auto operator!=(channel_count lhs, channel_count rhs)         { return lhs.value != rhs.value; }
[[nodiscard]] constexpr inline auto operator!=(channel_count lhs, channel_idx rhs)           { return lhs.value != rhs.value; }
[[nodiscard]] constexpr inline auto operator!=(channel_idx lhs, std::integral auto rhs)      { return lhs.value != rhs; }
[[nodiscard]] constexpr inline auto operator!=(channel_idx lhs, channel_count rhs)           { return lhs.value != rhs.value; }
[[nodiscard]] constexpr inline auto operator!=(channel_idx lhs, channel_idx rhs)             { return lhs.value != rhs.value; }
[[nodiscard]] constexpr inline auto operator!=(frame_count lhs, frame_count rhs)             { return lhs.value != rhs.value; }
[[nodiscard]] constexpr inline auto operator!=(frame_count lhs, frame_idx rhs)               { return lhs.value != rhs.value; }
[[nodiscard]] constexpr inline auto operator!=(frame_count lhs, std::integral auto rhs)      { return lhs.value != rhs; }
[[nodiscard]] constexpr inline auto operator!=(frame_idx lhs, frame_count rhs)               { return lhs.value != rhs.value; }
[[nodiscard]] constexpr inline auto operator!=(frame_idx lhs, frame_idx rhs)                 { return lhs.value != rhs.value; }
[[nodiscard]] constexpr inline auto operator!=(frame_idx lhs, std::integral auto rhs)        { return lhs.value != rhs; }
constexpr inline auto operator+=(float& lhs, frame_count rhs) -> float&                      { lhs += rhs.value; return lhs; }
constexpr inline auto operator+=(frame_count& lhs, frame_count rhs) -> frame_count&          { lhs.value += rhs.value; return lhs; }
constexpr inline auto operator+=(frame_count& lhs, std::integral auto rhs) -> frame_count&   { lhs.value += rhs; return lhs; }
constexpr inline auto operator-=(frame_count& lhs, frame_count rhs) -> frame_count&          { lhs.value -= rhs.value; return lhs; }
constexpr inline auto operator-=(std::integral auto& lhs, frame_count rhs) -> decltype(auto) { lhs -= rhs.value; return lhs; }
constexpr inline auto operator-=(frame_count& lhs, std::integral auto rhs) -> frame_count&   { lhs.value -= rhs; return lhs; }
constexpr inline auto operator+=(frame_idx& lhs, frame_count rhs) -> frame_idx&              { lhs.value += rhs.value; return lhs; }
constexpr inline auto operator+=(frame_idx& lhs, std::integral auto rhs) -> frame_idx&       { lhs.value += rhs; return lhs; }
constexpr inline auto operator/=(frame_count& lhs, std::integral auto rhs) -> frame_count&   { lhs.value /= rhs; return lhs; }
constexpr inline auto operator/=(frame_idx& lhs, std::integral auto rhs) -> frame_idx&       { lhs.value /= rhs; return lhs; }
constexpr inline auto operator++(frame_idx& lhs) -> frame_idx&             { lhs.value++; return lhs; }
constexpr inline auto operator++(frame_idx& lhs, int) -> frame_idx         { auto tmp = lhs; lhs.value++; return tmp; }
constexpr inline auto operator++(channel_count& lhs) -> channel_count&     { lhs.value++; return lhs; }
constexpr inline auto operator++(channel_count& lhs, int) -> channel_count { auto tmp = lhs; lhs.value++; return tmp; }
constexpr inline auto operator++(channel_idx& lhs) -> channel_idx&         { lhs.value++; return lhs; }
constexpr inline auto operator++(channel_idx& lhs, int) -> channel_idx     { auto tmp = lhs; lhs.value++; return tmp; }
constexpr inline auto operator--(frame_idx& lhs) -> frame_idx&             { lhs.value--; return lhs; }
constexpr inline auto operator--(frame_idx& lhs, int) -> frame_idx         { auto tmp = lhs; lhs.value--; return tmp; }

struct region {
	frame_idx beg, end;
	[[nodiscard]] constexpr auto size() const noexcept -> frame_count {
		assert (beg <= end);
		return {static_cast<uint64_t>(end.value - beg.value)};
	}
	[[nodiscard]] constexpr auto min() const noexcept -> frame_idx { return {std::min(beg.value, end.value)}; }
	[[nodiscard]] constexpr auto max() const noexcept -> frame_idx { return {std::max(beg.value, end.value)}; }
};

} // ads