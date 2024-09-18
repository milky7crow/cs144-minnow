#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring )
    beyond_last_ = first_index + data.size();

  auto writer_cap = output_.writer().available_capacity();

  if ( first_index == expected_ ) {
    /*
    push as much as possible
    then clear the "pushed" part of the buffer (if exists)
    */
    auto effective_size = min( data.size(), writer_cap );
    output_.writer().push( data.substr( 0, effective_size ) );
    expected_ += effective_size;

    while ( buffer_head_ < expected_ ) {
      auto& item = buffer_[buffer_head_++ % buffer_cap_];
      if ( item.second )
        --pending_;
      item.second = false;
    }
  } else if ( first_index > expected_ ) {
    /*
    buffer as much as possible
    skip bytes that are already buffered
    */
    if ( first_index >= expected_ + writer_cap )
      return;

    auto effective_size = min( data.size(), expected_ + writer_cap - first_index );
    for ( size_t i = 0; i < effective_size; ++i ) {
      auto& item = buffer_[( first_index + i ) % buffer_cap_];
      if ( !item.second ) {
        item.first = data[i];
        item.second = true;
        ++pending_;
      }
    }
  } else {
    if ( first_index + data.size() > expected_ ) {
      insert( expected_, data.substr( expected_ - first_index, SIZE_MAX ), is_last_substring );
    }
    return;
  }

  /*
  check if upcoming bytes in the buffer are ready to be pushed
  */
  string upcoming_data = "";
  auto& item = buffer_[buffer_head_ % buffer_cap_];
  while ( item.second ) {
    upcoming_data.append( { item.first } );
    item.second = false;
    --pending_;
    item = buffer_[++buffer_head_ % buffer_cap_];
  }
  output_.writer().push( upcoming_data );
  expected_ += upcoming_data.size();

  if ( expected_ >= beyond_last_ )
    output_.writer().close();
}

uint64_t Reassembler::bytes_pending() const
{
  return pending_;
}