#pragma once

#include <fstream>
#include <cassert>

#include "bit.hpp"

namespace encoding {

template <typename T>
static T ASCII(std::istream& ifs) {
  const auto c = ifs.get();
  assert( c < 0x80 );
  return T(c);
}

template <typename T>
static T UTF8(std::istream& ifs) {
  const auto a = ifs.get();

  if( (EOF == a) or (bit<7>(a) == 0) ) { // EOF || 0aaa aaaa
    return T(a);
  }

  if( bit<5,3>( a ) == 0b110 ) { // 110a aaaa 10bb bbbb
    const auto b = ifs.get();
    assert( (bit<6,2>( b ) == 0b10) );
    return (bit<0,5>( a ) << 6) | bit<0,6>( b );
  }

  if( bit<4,4>( a ) == 0b1110 ) { // 1110aaaa 10bbbbbb 10cccccc
    const auto b = ifs.get();
    assert( (bit<6,2>( b ) == 0b10) );
    const auto c = ifs.get();
    assert( (bit<6,2>( c ) == 0b10) );
    return (bit<0,4>( a ) << 12) | (bit<0,6>( b ) << 6) | bit<0,6>( c );
  }

  if( bit<3,5>( a ) == 0b11110 ) { // 11110aaa 10bbbbbb 10cccccc 10dddddd
    const auto b = ifs.get();
    assert( (bit<6,2>( b ) == 0b10) );
    const auto c = ifs.get();
    assert( (bit<6,2>( c ) == 0b10) );
    const auto d = ifs.get();
    assert( (bit<6,2>( d ) == 0b10) );
    return (bit<0,4>( a ) << 18) | (bit<0,6>( b ) << 12) | (bit<0,6>( c ) << 6) | bit<0,6>( d );
  }

  assert(false);
}

}
