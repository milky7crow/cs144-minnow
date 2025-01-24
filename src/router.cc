#include "router.hh"

#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  router_table_.push_back( {
    route_prefix,
    prefix_length,
    next_hop,
    interface_num
  } );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for (auto ni : _interfaces ) {
    while ( !ni->datagrams_received().empty() ) {
      auto& dgram = ni->datagrams_received().front();
      size_t curr_match = _interfaces.size();
      std::optional<Address> next_hop = std::nullopt;
      uint8_t curr_prefix_length = 0;

      for ( const auto& entry : router_table_ ) {
        if ( longest_prefix_match( dgram.header.dst, entry.route_prefix, entry.prefix_length ) ) {
          if ( entry.route_prefix >= curr_prefix_length ) {
            curr_match = entry.interface_num;
            next_hop = entry.next_hop;
            curr_prefix_length = entry.prefix_length;
          }
        }
      }

      dgram.header.ttl -= 1;
      dgram.header.compute_checksum();
      // drop if TTL reaches 0
      if ( dgram.header.ttl == 0 )
        continue;

      // drop if no routes found
      if ( curr_match == _interfaces.size() )
        continue;

      // fill next_hop with dst ip if localhost is in the network
      if ( !next_hop.has_value() ) {
        next_hop = std::make_optional( Address::from_ipv4_numeric( dgram.header.dst ) );
      }

      _interfaces[curr_match]->send_datagram( dgram, next_hop.value() );
      ni->datagrams_received().pop();
    }
  }

}

bool Router::longest_prefix_match( uint32_t dst_ip, uint32_t route_prefix, uint8_t prefix_length ) const {
  // NOTE: cannot shift uint32_t 32 bits
  if ( prefix_length == 0 )
    return true;

  uint32_t mask = UINT32_MAX << ( 8 * sizeof(uint32_t) - prefix_length );
  return ( dst_ip & mask ) == ( route_prefix & mask );
}