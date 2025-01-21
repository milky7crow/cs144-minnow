#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.SYN )
    isn_ = std::make_optional( message.seqno );
  if ( message.RST )
    reassembler_.reader().set_error();
  if ( !isn_.has_value() )
    return;

  // if current message doesn't have SYN, then SYN must have been received, so minus 1
  reassembler_.insert(
      message.SYN ? 0 : message.seqno.unwrap( isn_.value(), reassembler_.writer().bytes_pushed() ) - 1,
      message.payload,
      message.FIN
  );
}

TCPReceiverMessage TCPReceiver::send() const
{
  return {
      isn_.has_value() 
          // ackno = SYN + stream expecting index + FIN
          // if isn_.has_value() is true, then SYN must have been received, however FIN is unknown
          ? isn_.value() + static_cast<uint32_t>( 1 + reassembler_.writer().bytes_pushed() + reassembler_.writer().is_closed() ) 
          : static_cast<std::optional<Wrap32>>( std::nullopt ),
      reassembler_.writer().available_capacity() > static_cast<uint64_t>( UINT16_MAX )
          // note that uint64_t capacity may exceed uint16_t upper limit
          ? static_cast<uint16_t>( UINT16_MAX ) 
          : static_cast<uint16_t>( reassembler_.writer().available_capacity() ),
      reassembler_.writer().has_error()
  };
}
