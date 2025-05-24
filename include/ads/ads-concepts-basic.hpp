#pragma once

#include <type_traits>

namespace ads::concepts {

template <typename T> concept arithmetic = std::is_arithmetic_v<T>;

} // namespace ads::concepts
