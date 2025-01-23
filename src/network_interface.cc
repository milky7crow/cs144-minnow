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

  // sent arp request if:
  // 1. entry not present
  // 2. request timeout
  //
  // timeout entry will be cleared in tick()
  // assert( curr_time_ > mapping->second.time );
  if ( mapping == mapping_cache_.end()
       || ( !mapping->second.valid && curr_time_ - mapping->second.time >= ARP_RESENT_COOLDOWN_MS ) ) {
    tx_arp( next_hop );
    datagrams_cached_.push_back( { dgram, next_hop } );
  } else {
    tx_ipv4( dgram, mapping->second.eth_addr );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  InternetDatagram dgram;
  ARPMessage msg;
  uint32_t ip_numeral = 0;

  // TODO: parse failure fallback
  switch ( frame.header.type ) {
    case EthernetHeader::TYPE_IPv4:
      if ( parse( dgram, frame.payload ) ) {
        ip_numeral = dgram.header.src;
        datagrams_received_.push( dgram );
      }
    case EthernetHeader::TYPE_ARP:
      if ( parse( msg, frame.payload ) )
        ip_numeral = msg.sender_ip_address;
  }

  // refresh or add cache entry
  auto mapping = mapping_cache_.find( ip_numeral );
  if ( mapping != mapping_cache_.end() ) {
    // refresh
    auto entry = mapping->second;
    entry.eth_addr = frame.header.src;
    entry.valid = true;
    entry.time = curr_time_;
  } else {
    // add
    auto entry = CacheTableEntry( frame.header.src, true, curr_time_ );
    mapping_cache_.insert( {ip_numeral, entry });
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
    if ( curr_time_ - it->second.time >= ENTRY_VALID_MS ) {
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

void NetworkInterface::tx_arp( const Address& ip_addr )
{
  auto msg = ARPMessage();
  msg.opcode = ARPMessage::OPCODE_REQUEST;
  msg.sender_ethernet_address = ethernet_address_;
  msg.sender_ip_address = ip_address_.ipv4_numeric();
  msg.target_ethernet_address = ETHERNET_BROADCAST;
  msg.target_ip_address = ip_addr.ipv4_numeric();

  auto eth_header = EthernetHeader( ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP );
  auto eth_frame = EthernetFrame( eth_header, serialize( msg ) );
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