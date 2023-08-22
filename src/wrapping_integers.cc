#include "wrapping_integers.hh"
#include <cstdint>
#include <iostream>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + ( n & ( ( 1ULL << 32 ) - 1 ) );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t offset = raw_value_ - zero_point.raw_value_;
  uint64_t checkpoint_32 = checkpoint & ( ( 1ULL << 32 ) - 1 );
  if ( checkpoint >= 1ULL << 32 && offset > checkpoint_32 && offset - checkpoint_32 >= 1ULL << 31 ) {
    return offset + checkpoint - checkpoint_32 - ( 1ULL << 32 );
  }
  if ( checkpoint_32 > offset && checkpoint_32 - offset >= 1ULL << 31 ) {
    return offset + checkpoint - checkpoint_32 + ( 1ULL << 32 );
  }
  return offset + checkpoint - checkpoint_32;
}
