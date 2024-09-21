#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if ( message.SYN ) {
    isn_ = message.seqno;
    isn_set_ = true;
  }
  uint64_t abs_seqno = message.seqno.unwrap( isn_, writer().bytes_pushed() );
  uint64_t stream_index = message.SYN ? 0 : abs_seqno - 1;
  reassembler_.insert( stream_index, message.payload, message.FIN );
  ackno = Wrap32::wrap( writer().bytes_pushed() + 1, isn_ );
  if ( writer().is_closed() )
    ackno = ackno + 1;
}

TCPReceiverMessage TCPReceiver::send() const
{
  return { isn_set_ ? make_optional( ackno ) : nullopt,
           static_cast<uint16_t>( min( writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) ) ),
           writer().has_error() || reader().has_error() };
}
