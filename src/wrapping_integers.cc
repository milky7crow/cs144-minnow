#include "wrapping_integers.hh"

using namespace std;

uint64_t uint64_dist( const uint64_t& a, const uint64_t& b ) {
  return a < b ? b - a : a - b;
}

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32{ static_cast<uint32_t>( n & 0x00000000ffffffffULL ) + zero_point.raw_value_ };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint32_t offset = raw_value_ - zero_point.raw_value_;
  uint64_t l_candidate, r_candidate;
  if ( ( checkpoint & 0x00000000ffffffff ) < 0x7fffffff ) {
    l_candidate = ( checkpoint & 0xffffffff00000000 ) - 0x100000000 + offset;
    r_candidate = ( checkpoint & 0xffffffff00000000 ) + offset;
  } else {
    l_candidate = ( checkpoint & 0xffffffff00000000 ) + offset;
    r_candidate = ( checkpoint & 0xffffffff00000000 ) + 0x100000000 + offset;
  }
  return uint64_dist( checkpoint, l_candidate ) < uint64_dist( checkpoint, r_candidate ) ? l_candidate : r_candidate;
}
