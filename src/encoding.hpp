#pragma once

#include <fstream>

#include "bit.hpp"
#include "logging.hpp"

namespace encoding {

template <typename T>
static T ASCII(std::istream& ifs) {
  const auto c = ifs.get();
  enforce( c < 0x80, c << " is not a valid ASCII character" );
  return T(c);
}

template <typename T>
static T LATIN1(std::istream& ifs) { // TODO: this is incomplete, need to check if latin1 -> unicode mapping is 1-1
  const auto c = ifs.get();
  enforce( c < 0x100, c << " is not a valid ISO/IEC 8859-1 character" );
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
    enforce( (bit<6,2>( b ) == 0b10), "unexpected UTF8 value: " << b );
    return (bit<0,5>( a ) << 6) | bit<0,6>( b );
  }

  if( bit<4,4>( a ) == 0b1110 ) { // 1110aaaa 10bbbbbb 10cccccc
    const auto b = ifs.get();
    enforce( (bit<6,2>( b ) == 0b10), "unexpected UTF8 value: " << b );
    const auto c = ifs.get();
    enforce( (bit<6,2>( c ) == 0b10), "unexpected UTF8 value: " << c );
    return (bit<0,4>( a ) << 12) | (bit<0,6>( b ) << 6) | bit<0,6>( c );
  }

  if( bit<3,5>( a ) == 0b11110 ) { // 11110aaa 10bbbbbb 10cccccc 10dddddd
    const auto b = ifs.get();
    enforce( (bit<6,2>( b ) == 0b10), "unexpected UTF8 value: " << b );
    const auto c = ifs.get();
    enforce( (bit<6,2>( c ) == 0b10), "unexpected UTF8 value: " << c );
    const auto d = ifs.get();
    enforce( (bit<6,2>( d ) == 0b10), "unexpected UTF8 value: " << d );
    return (bit<0,4>( a ) << 18) | (bit<0,6>( b ) << 12) | (bit<0,6>( c ) << 6) | bit<0,6>( d );
  }

  critical("unexpected UTF8 value: " << a);
}

}
