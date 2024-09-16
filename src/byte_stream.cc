#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity )
{
  buffer_.resize( capacity_ );
  it_write = buffer_.begin();
  it_read = buffer_.begin();
}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( string data )
{
  if ( is_closed() )
    return;

  // how much of the data will be pushed
  auto bytes_count = min( data.size(), available_capacity() );

  // not enough space in the back
  if ( static_cast<size_t>( buffer_.end() - it_write ) < bytes_count ) {
    copy( it_read, it_read + occupied_, buffer_.begin() );
    it_read = buffer_.begin();
    it_write = buffer_.begin() + occupied_;
  }

  copy( data.begin(), data.begin() + bytes_count, it_write );
  std::advance( it_write, bytes_count );

  bytes_pushed_ += bytes_count;
  occupied_ += bytes_count;

  return;
}

void Writer::close()
{
  closed_ = true;
  return;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - occupied_;
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

bool Reader::is_finished() const
{
  return closed_ && occupied_ == 0;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}

string_view Reader::peek() const
{
  return { it_read, it_write };
}

void Reader::pop( uint64_t len )
{
  auto bytes_count = std::min( len, buffer_.size() );
  std::advance( it_read, bytes_count );
  occupied_ -= bytes_count;
  bytes_popped_ += bytes_count;
  return;
}

uint64_t Reader::bytes_buffered() const
{
  return occupied_;
}
