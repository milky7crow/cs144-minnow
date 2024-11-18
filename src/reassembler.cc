#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  auto past_last = first_index + data.size();
  auto available_capacity = output_.writer().available_capacity();
  auto past_acceptable = expected_ + available_capacity;

  // needs unmodified data
  if ( is_last_substring ) {
    past_final_index_ = past_last;
  }
  // close the stream in time
  if ( expected_ == past_final_index_ ) {
    output_.writer().close();
  }

  // reject wholly unacceptable data
  if ( first_index >= past_acceptable )
    return;

  // reject wholly received data
  if ( past_last <= expected_ )
    return;

  // cutoff confirmed bytes for partly received data
  if ( first_index < expected_ ) {
    insert( expected_, data.substr( expected_ - first_index ), is_last_substring );
    return;
  }

  // cutoff unacceptable data
  if ( past_last > past_acceptable ) {
    data.resize( past_acceptable - first_index );
    past_last = first_index + data.size();
  }

  // adjust buffer size
  buffer_.resize( std::max( buffer_.size(), available_capacity ), { '\0', false} );

  // copy data into buffer
  auto dst = buffer_.begin() + ( first_index - expected_ );
  for ( const auto& ch : data ) {
    if ( !dst->second )
      ++valid_count_;
    dst->first = ch;
    dst->second = true;
    ++dst;
  }

  // push if possible
  if ( buffer_[0].second ) {
    // chars => string
    std::string load;
    auto src = buffer_.begin();
    for ( ; src != buffer_.end(); ++src ) {
      if ( !src->second )
        break;

      load += src->first;
    }

    // push
    output_.writer().push( load );

    // adjust buffer
    auto copied_size = buffer_.end() - src;
    std::copy( src, buffer_.end(), buffer_.begin() );
    // wipe validness beyond last copied byte
    for ( auto it = buffer_.begin() + copied_size; it != buffer_.end(); ++it ) {
      it->second = false;
    }
    // adjust valid count
    valid_count_ -= load.size();
    // adjust expected
    expected_ += load.size();
  }
  
  // double check
  if ( expected_ == past_final_index_ ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return valid_count_;
}
