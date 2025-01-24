#include <cassert>
#include <cstdint>
#include <iostream>

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "network_interface.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"

using namespace std;

const EthernetAddress ZERO_ETHERNET_ADDRESS = { 0, 0, 0, 0, 0, 0 };

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // try to find mapping in cache
  auto mapping = mapping_cache_.find( next_hop.ipv4_numeric() );

  // sent arp request if: entry not present
  //
  // waiting-for-reply timeout entres will be cleared in tick()
  // assert( curr_time_ > mapping->second.time );
  if ( mapping == mapping_cache_.end() ) {
    // NOTE: cache datagram before send arp req, or else the test would fail (nothing is cached)
    datagrams_cached_.push_back( { dgram, next_hop } );
    tx_arp_request( next_hop.ipv4_numeric() );
  } else if ( mapping->second.valid ) {
    // implement cooldown logic
    tx_ipv4( dgram, mapping->second.eth_addr );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // NOTE: frame with ethernet address that is not ours should be ignored
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST )
    return;

  InternetDatagram dgram;
  ARPMessage msg;
  uint32_t ip_numeric = 0;

  // TODO: parse failure fallback
  switch ( frame.header.type ) {
    case EthernetHeader::TYPE_IPv4:
      if ( parse( dgram, frame.payload ) ) {
        ip_numeric = dgram.header.src;
        datagrams_received_.push( dgram );
      }
    case EthernetHeader::TYPE_ARP:
      if ( parse( msg, frame.payload ) ) {
        ip_numeric = msg.sender_ip_address;
        if ( msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == ip_address_.ipv4_numeric() )
          tx_arp_reply( frame.header.src, msg.sender_ip_address );
      }
  
  }

  // refresh or add cache entry
  auto mapping = mapping_cache_.find( ip_numeric );
  if ( mapping != mapping_cache_.end() ) {
    // refresh
    auto entry = CacheTableEntry( frame.header.src, true, curr_time_ );
    mapping->second = entry;
  } else {
    // add
    auto entry = CacheTableEntry( frame.header.src, true, curr_time_ );
    mapping_cache_.insert( {ip_numeric, entry });
  }
  send_cached();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  curr_time_ += ms_since_last_tick;

  // expire outdated mapping cache entries
  // TODO: can we use ordered_map to boost time-based / ip-based query
  for ( auto it = mapping_cache_.begin(); it != mapping_cache_.end(); ) {
    // assert( curr_time_ > it->second.time );
    auto time_elapsed = curr_time_ - it->second.time;
    if ( ( it->second.valid && time_elapsed >= ENTRY_VALID_MS )
         || ( !it->second.valid && time_elapsed >= ARP_RESENT_COOLDOWN_MS ) ) {
      it = mapping_cache_.erase( it );
    } else {
      ++it;
    }
  }
}

void NetworkInterface::tx_ipv4( const InternetDatagram& dgram, const EthernetAddress& eth_addr )
{
  auto eth_header = EthernetHeader( eth_addr, ethernet_address_, EthernetHeader::TYPE_IPv4 );
  auto eth_frame = EthernetFrame( eth_header, serialize( dgram ) );
  transmit( eth_frame );
}

void NetworkInterface::tx_arp_request( uint32_t ip_numeric )
{
  auto arp_msg = make_arp( ARPMessage::OPCODE_REQUEST, ZERO_ETHERNET_ADDRESS, ip_numeric );
  auto eth_frame = make_frame( ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP, arp_msg );
  transmit( eth_frame );

  auto entry = CacheTableEntry( ZERO_ETHERNET_ADDRESS, false, curr_time_ );
  mapping_cache_.insert( { ip_numeric, entry } );
}

void NetworkInterface::tx_arp_reply( const EthernetAddress& eth_addr, const uint32_t ip_numeric ) {
  auto arp_msg = make_arp( ARPMessage::OPCODE_REPLY, eth_addr, ip_numeric );
  auto eth_frame = make_frame( eth_addr, EthernetHeader::TYPE_ARP, arp_msg );
  transmit( eth_frame );
}

void NetworkInterface::send_cached()
{
  for ( auto it = datagrams_cached_.begin(); it != datagrams_cached_.end(); ) {
    auto mapping = mapping_cache_.find( it->second.ipv4_numeric() );
    if ( mapping != mapping_cache_.end() && mapping->second.valid ) {
      // ready to transmit
      tx_ipv4( it->first, mapping->second.eth_addr );
      it = datagrams_cached_.erase( it );
    } else {
      ++it;
    }
  }
}

ARPMessage NetworkInterface::make_arp( uint16_t opcode,
                                       const EthernetAddress& target_eth,
                                       uint32_t target_ip ) const
{
  auto msg = ARPMessage();
  msg.opcode = opcode;
  msg.sender_ethernet_address = ethernet_address_;
  msg.sender_ip_address = ip_address_.ipv4_numeric();
  // NOTE: Broadcast ethernet address is placed at ethernet header,
  //       target ethernet address in arp message should be ignored.
  //       Anyway, tests require it to be 0.
  msg.target_ethernet_address = target_eth;
  msg.target_ip_address = target_ip;
  return msg;
}

template<class T>
EthernetFrame NetworkInterface::make_frame( const EthernetAddress& dst,
                                            uint16_t type,
                                            const T& payload ) const
{
  auto eth_header = EthernetHeader( dst, ethernet_address_, type );
  return EthernetFrame( eth_header, serialize( payload ));
}