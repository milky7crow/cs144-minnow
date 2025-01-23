#pragma once

#include <cstddef>
#include <cstdint>
#include <queue>
#include <string>
#include <unordered_map>
#include <list>

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
public:
  // An abstraction for the physical output port where the NetworkInterface sends Ethernet frames
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next
  // hop. Sending is accomplished by calling `transmit()` (a member variable) on the frame.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, pushes the datagram to the datagrams_in queue.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  void recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  // Accessors
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }

private:
  // Human-readable name of the interface
  std::string name_;

  // The physical output port (+ a helper function `transmit` that uses it to send an Ethernet frame)
  std::shared_ptr<OutputPort> port_;
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  // Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as internet-layer or network-layer) address of the interface
  Address ip_address_;

  // Datagrams that have been received
  std::queue<InternetDatagram> datagrams_received_ {};


  // Datagrams that's waiting to be sent
  std::list<std::pair<InternetDatagram, Address>> datagrams_cached_ {};

  struct CacheTableEntry {
    EthernetAddress eth_addr;
    // true if received arp reply
    // flase if not yet
    bool valid;
    // time of entry last used if valid is true
    // time of arp request sent if valid is false
    size_t time;
  };
  // ip to ethernet address mapping
  std::unordered_map<uint32_t, CacheTableEntry> mapping_cache_ {};

  size_t curr_time_ { 0 };
  const size_t ENTRY_VALID_MS { 30 * 1000 };
  const size_t ARP_RESENT_COOLDOWN_MS { 5 * 1000 };

  // helper methods
  void tx_ipv4( const InternetDatagram& dgram, const EthernetAddress& eth_addr );
  void tx_arp_request( uint32_t ip_numeric );
  void tx_arp_reply( const EthernetAddress& eth_addr, const uint32_t ip_numeric );
  // check if there are send-able dgrams cached, and send them
  void send_cached();

  // make methods
  ARPMessage make_arp( uint16_t opcode, const EthernetAddress& target_eth, uint32_t target_ip ) const;
  template<class T>
  EthernetFrame make_frame( const EthernetAddress& dst,
                            uint16_t type,
                            const T& payload ) const;
};
