#include "wrapping_integers.hh"
#include <cstdint>
#include <cstdlib>
#include <sys/types.h>

using namespace std;

const uint64_t PAST_U32_MAX = 1UL << 32;

uint64_t abs_u64( uint64_t a, uint64_t b ) {
  return a < b ? b - a : a - b;
}

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + ( n % PAST_U32_MAX );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint32_t seq_offset = raw_value_ - zero_point.raw_value_;
  uint64_t checkpoint_head = checkpoint & ~( PAST_U32_MAX - 1 );
  uint64_t min_diff = UINT64_MAX;
  uint64_t res = 0;
  for ( int i = -1; i <= 1; ++i ) {
    uint64_t curr = checkpoint_head + i * PAST_U32_MAX + seq_offset;
    uint64_t diff = abs_u64( curr, checkpoint );
    if ( diff < min_diff ) {
      min_diff = diff;
      res = curr;
    }
  }

  return res;
}
