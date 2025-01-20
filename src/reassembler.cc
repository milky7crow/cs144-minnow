#include "reassembler.hh"
#include <algorithm>
#include <cstdint>
#include <sys/types.h>
#include <utility>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  uint64_t past_end_index = first_index + data.size();
  uint64_t first_unacceptable_index = expecting_ + output_.writer().available_capacity();

  // update last index
  if ( is_last_substring ) {
    past_last_index_ = past_end_index;
    if ( expecting_ == past_last_index_ ) {
      output_.writer().close();
      return;
    }
  }

  // reject fully-accepted / fully-unaccepable data
  if ( past_end_index <= expecting_  || first_index >= first_unacceptable_index ) {
    return;
  }

  auto accepting_begin_it = data.cbegin();
  auto accepting_end_it = data.cend();
  // trim accepted data
  if ( first_index < expecting_ ) {
    accepting_begin_it += expecting_ - first_index;
    first_index = expecting_;
  }
  // trim unacceptable data
  if ( past_end_index > first_unacceptable_index ) {
    accepting_end_it -= ( past_end_index - first_unacceptable_index );
    past_end_index = first_unacceptable_index;
  }

  // copy data to buffer
  std::copy( accepting_begin_it, accepting_end_it, buffer_.begin() + first_index - expecting_ );

  // maintain segment location info
  auto seg = std::make_pair(first_index, past_end_index - 1 );
  auto it = std::lower_bound( seg_locs_.begin(), seg_locs_.end(), seg );
  seg_locs_.insert( it, seg );
  merge_locs();

  // push if possible
  if ( expecting_ == seg_locs_.front().first ) {
    auto pushing_size = seg_locs_.front().second - expecting_ + 1;

    output_.writer().push( buffer_.substr(0, pushing_size ) );
    seg_locs_.pop_front();
    if ( !seg_locs_.empty() ) {
      std::copy( buffer_.cbegin() + ( seg_locs_.front().first - expecting_ ),
                 buffer_.cbegin() + ( seg_locs_.back().second - expecting_ + 1 ),
                 buffer_.begin() + ( seg_locs_.front().first - expecting_ - pushing_size ) );
    }

    expecting_ += pushing_size;
    if ( expecting_ == past_last_index_ ) {
      output_.writer().close();
    }
  }
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t res = 0;
  for ( const auto &loc : seg_locs_ ) {
    res += loc.second - loc.first + 1;
  }

  return res;
}

void Reassembler::merge_locs()
{
  if ( seg_locs_.size() <= 1 ) {
    return;
  }

  auto i = seg_locs_.begin();
  while ( i != seg_locs_.end() ) {
    auto j = std::next( i );
    if ( j == seg_locs_.end() )
      break;

    if ( i->second + 1 >= j->first ) {
      i->second = std::max( i->second, j->second );
      seg_locs_.erase( j );
    } else {
      ++i;
    }
  }
}