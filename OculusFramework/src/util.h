#pragma once

#pragma warning(push)
#pragma warning(disable : 4245)
#include <string_view.h>  // GSL
#pragma warning(pop)

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <limits>
#include <string>

namespace util {

// These to<>() conversion functions deliberately throw from constexpr noexcept bodies, the intent
// is for the app to terminate immediately so it is safe to ignore this warning
#pragma warning(push)
#pragma warning(disable : 4297)  // function assumed not to throw an exception but does
template <typename To, typename From>
constexpr To to(From);

template<>
constexpr float to<float, double>(double x) noexcept {
    return static_cast<float>(x);
}

template<>
constexpr float to<float, int>(int x) noexcept {
    return static_cast<float>(x);
}

template <>
constexpr int to<int, std::size_t>(std::size_t x) noexcept {
    return x > static_cast<std::size_t>(std::numeric_limits<int>::max())
               ? throw std::invalid_argument{""}
               : static_cast<int>(x);
}

template <>
constexpr std::uint32_t to<std::uint32_t, int>(int x) noexcept {
    return x < 0 ? throw std::invalid_argument{""} : static_cast<std::uint32_t>(x);
}

template <>
constexpr uint32_t to<uint32_t, std::size_t>(std::size_t x) noexcept {
    return x > std::numeric_limits<uint32_t>::max() ? throw std::invalid_argument{""}
                                                    : static_cast<uint32_t>(x);
}

template <>
constexpr uint32_t to<uint32_t, float>(float x) noexcept {
    return x < 0 ? throw std::invalid_argument{""} : x > std::numeric_limits<uint32_t>::max()
                                                         ? throw std::invalid_argument{""}
                                                         : static_cast<uint32_t>(x);
}

template <>
constexpr int to<int, float>(float x) noexcept {
    return x > std::numeric_limits<int>::max()
               ? throw std::invalid_argument{""}
               : x < std::numeric_limits<int>::min() ? throw std::invalid_argument{""}
                                                     : static_cast<uint32_t>(x);
}

template <>
constexpr int to<int, double>(double x) noexcept {
    return x > std::numeric_limits<int>::max()
               ? throw std::invalid_argument{""}
               : x < std::numeric_limits<int>::min() ? throw std::invalid_argument{""}
                                                     : static_cast<uint32_t>(x);
}
#pragma warning(pop)

std::string narrow(gsl::cwstring_view<> x);
std::wstring widen(gsl::cstring_view<> x);

template<typename Cont>
constexpr auto const_array_view(const Cont& cont) {
    return gsl::array_view<const typename Cont::value_type>{gsl::as_array_view(cont)};
}

}
