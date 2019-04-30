#pragma once

#include <type_traits>
#include <cstddef>
#include <cstdint>

template <typename T>
struct mask_type {
  typedef typename std::conditional<
    sizeof(T)==1, uint8_t,
    typename std::conditional<
      sizeof(T)==2, uint16_t,
      typename std::conditional<
        sizeof(T)==4, uint32_t,
        uint64_t
      >::type
    >::type
  >::type value_type;
};

template <size_t N, typename T>
static inline constexpr bool bit(const T& t) noexcept  {
  return 0 != (t & ( 1 << N ));
}

template <size_t O, size_t N, typename T, typename M = typename mask_type<T>::value_type>
static inline constexpr T bit(const T& t) noexcept {
  static_assert ( N > 0, "");
  static_assert( N + O <= sizeof(T) * 8, "");
  return T((M(t) >> O) & ((M(~0)) >> ((sizeof(T) * 8) - N)));
}

static_assert( bit<0>(0) == 0, "");
static_assert( bit<0>(1) == 1, "");
static_assert( bit<1>(1) == 0, "");
static_assert( bit<0,1>(0) == 0, "");
static_assert( bit<0,1>(1) == 1, "");
static_assert( bit<1,1>(1) == 0, "");
static_assert( bit<1,1>(2) == 1, "");
static_assert( bit<0,2>(2) == 2, "");
static_assert( bit<1,2>(2) == 1, "");
static_assert( bit<15,1>(wchar_t(0x8000)) == 1, "");
static_assert( bit<2,2>(0b0101) == 0b01, "");
static_assert( bit<0,2>(0b101010) == 0b10, "");
static_assert( bit<0,6>(0b101010) == 0b101010, "");
static_assert( bit<2,2>(0b101010) == 0b10, "");
