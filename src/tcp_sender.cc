#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <cstddef>
#include <cstdint>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t res = 0;
  for ( const auto& msg : outstanding_ )
    res += msg.sequence_length();
  return res;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmition_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  auto msg = make_empty_message();
  size_t mod_window_size = window_size_ == 0 ? 1 : window_size_;

  // NOTE: non-zero window size does NOT MEAN NON-FULL window
  auto payload_size_limit = std::min( TCPConfig::MAX_PAYLOAD_SIZE, static_cast<size_t>( mod_window_size - sequence_numbers_in_flight() - msg.SYN ) );
  msg.payload = reader().peek().substr( 0, payload_size_limit );
  input_.reader().pop( msg.payload.size() );

  msg.FIN = reader().is_finished();
  msg.RST = input_.has_error();

  if ( msg.sequence_length() > mod_window_size - sequence_numbers_in_flight() )
    msg.payload.erase( msg.payload.end() - ( msg.sequence_length() - mod_window_size ), msg.payload.end() );

  // empty message & beyond-window flag-only message
  if ( msg.sequence_length() == 0 || msg.sequence_length() > mod_window_size - sequence_numbers_in_flight() )
    return;
  transmit( msg );

  outstanding_.push_back( msg );
  current_sn_ = current_sn_ + msg.sequence_length();

  // NOTE: resetting and starting timer is differenent, since resetting timer here will wipe former counter
  start_timer();

  // NOTE: try to drain the input
  if ( reader().bytes_buffered() )
    push( transmit );
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  auto res = TCPSenderMessage();
  // stream_index == 0 equals to: this message has SYN flag
  res.SYN = current_sn_ == isn_;
  res.seqno = current_sn_;
  return res;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // basics: update window size, set error, ack messages
  window_size_ = msg.window_size;

  if ( msg.RST )
    input_.set_error();

  for ( auto it = outstanding_.begin(); it != outstanding_.end(); ) {
    // TODO: is this unwrap necessary?
    if ( it->seqno.unwrap( isn_, reader().bytes_popped() ) + it->sequence_length()
         <= msg.ackno->unwrap( isn_, reader().bytes_popped() ) ) {
      // acked new message
      it = outstanding_.erase( it );

      current_RTO_ms_ = initial_RTO_ms_;
      reset_timer();
      consecutive_retransmition_ = 0;
    } else {
      ++it;
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // do tick
  if ( timer_enabled_ )
    timer_count_ += ms_since_last_tick;

  if ( is_timer_expired() ) {
    // retransmit earliest outstanding message
    transmit( outstanding_.front() );

    if ( window_size_ != 0 ) {
      consecutive_retransmition_ += 1;
      // exponential backoff
      current_RTO_ms_ *= 2;
    }

    reset_timer();
  }
}

void TCPSender::reset_timer() {
  timer_enabled_ = true;
  timer_count_ = 0;
}

void TCPSender::start_timer() {
  timer_enabled_ = true;
}

void TCPSender::stop_timer() {
  timer_enabled_ = false;
  timer_count_ = 0;
}

bool TCPSender::is_timer_expired() const {
  return timer_count_ >= current_RTO_ms_;
}