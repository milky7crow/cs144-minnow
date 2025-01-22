#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <cstddef>
#include <cstdint>

using namespace std;

// perform a - b, if underflows, return val
uint64_t nneg_else( uint64_t a, uint64_t b, uint64_t val ) {
  if ( a >= b )
    return a - b;
  else
   return val;
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return sequence_numbers_in_flight_;
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
  // NOTE: mod_window_size - in_flight - SYN might be underflow, since window size may change after a message was sent
  auto payload_size_limit = std::min( TCPConfig::MAX_PAYLOAD_SIZE, static_cast<size_t>( nneg_else( mod_window_size, sequence_numbers_in_flight() + msg.SYN, 0UL ) ) );
  msg.payload = reader().peek().substr( 0, payload_size_limit );
  input_.reader().pop( msg.payload.size() );

  msg.FIN = reader().is_finished();

  // NOTE: ensure fin can only be pushed once
  // TODO: any better way?
  if ( msg.FIN && fin_sent_ )
    return;

  // NOTE: when there's no window space for FIN, save it for the next message, instead of trimming payload
  if ( msg.sequence_length() > mod_window_size - sequence_numbers_in_flight() )
    // msg.payload.erase( msg.payload.end() - ( msg.sequence_length() - mod_window_size ), msg.payload.end() );
    msg.FIN = false;

  // empty message & beyond-window flag-only message
  if ( msg.sequence_length() == 0 || msg.sequence_length() > nneg_else( mod_window_size, sequence_numbers_in_flight(), 0UL ) )
    return;
  transmit( msg );

  fin_sent_ |= msg.FIN;
  outstanding_.push_back( msg );
  sequence_numbers_in_flight_ += msg.sequence_length();
  current_sn_ = current_sn_ + msg.sequence_length();
  // when fin_sent_ is true, current_sn_ is past_fin_sn_

  // NOTE: resetting and starting timer is differenent, since resetting timer here will wipe former counter
  start_timer();

  // NOTE: try to drain the input
  if ( reader().bytes_buffered() )
    push( transmit );
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  auto msg = TCPSenderMessage();
  msg.seqno = current_sn_;
  // NOTE: RST and SYN will not be changed later / FIN will possibly be changed
  // stream_index == 0 equals to: this message has SYN flag
  msg.SYN = current_sn_ == isn_;
  msg.RST = input_.has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // basics: update window size, set error, ack messages
  window_size_ = msg.window_size;

  if ( msg.RST )
    input_.set_error();

  if ( fin_sent_ && msg.ackno == current_sn_ )
    fin_acked_ = true;

  for ( auto it = outstanding_.begin(); it != outstanding_.end(); ) {
    // TODO: is this unwrap necessary?
    auto unwrapped_it_seqno = it->seqno.unwrap( isn_, reader().bytes_popped() );
    auto unwrapped_ackno = msg.ackno->unwrap( isn_, reader().bytes_popped() );
    auto unwrapped_curr_seqno = current_sn_.unwrap( isn_, reader().bytes_popped() );
    // NOTE: cannot ack a seqno that haven't been sent yet
    if ( unwrapped_it_seqno + it->sequence_length() <= unwrapped_ackno && unwrapped_ackno <= unwrapped_curr_seqno ) {
      // acked new message
      sequence_numbers_in_flight_ -= it->sequence_length();
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
  // should not response when fin acked
  if ( fin_acked_ )
    return;

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