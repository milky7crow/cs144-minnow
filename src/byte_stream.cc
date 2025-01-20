#include "byte_stream.hh"
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), buffer_( capacity, '\0' ) {}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( string data )
{
  // trim data if no enough absolute space
  if ( data.size() > available_capacity() ) {
    data.resize( available_capacity() );
  }

  // move data to the beginning of the buffer if no enough space in the back
  if ( capacity_ - tail_ < data.size() ) {
    copy( buffer_.begin() + head_, buffer_.begin() + tail_, buffer_.begin() );
    tail_ = tail_ - head_;
    head_ = 0;
  }

  copy( data.begin(), data.end(), buffer_.begin() + tail_ );
  
  tail_ += data.size();
  bytes_pushed_ += data.size();

  return;
}

void Writer::close()
{
  closed_ = true;
  return;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - ( tail_ - head_ );
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

bool Reader::is_finished() const
{
  return closed_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}

string_view Reader::peek() const
{
  return string_view( buffer_.begin() + head_, buffer_.begin() + tail_ );
}

void Reader::pop( uint64_t len )
{
  len = min( len, bytes_buffered() );
  head_ += len;
  bytes_popped_ += len;

  return;
}

uint64_t Reader::bytes_buffered() const
{
  return tail_ - head_;
}
