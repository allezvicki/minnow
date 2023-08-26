#include "router.hh"
#include "address.hh"
#include "ipv4_datagram.hh"

#include <algorithm>
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
  routes_.push_back( { route_prefix, prefix_length, next_hop, interface_num } ); // emplace back?
}

void Router::route_datagram( InternetDatagram dgram )
{
  if ( dgram.header.ttl <= 1 ) {
    return;
  }
  auto ip_address = dgram.header.dst; // Is endianness correct?
  cerr << "dst ip address is " << Address::from_ipv4_numeric( ip_address ).to_string() << endl;
  int match_length { -1 };
  optional<Address> next_hop;
  size_t interface_num {};
  for ( auto& route : routes_ ) {
    int prefix_length = route.prefix_length;
    if ( match_length < prefix_length ) {
      uint32_t mask = UINT32_MAX - ( ( 1UL << ( 32UL - route.prefix_length ) ) - 1UL );
      if ( ( route.route_prefix & mask ) == ( ip_address & mask ) ) {
        cerr << "match success\n";
        next_hop = route.next_hop;
        interface_num = route.interface_num;
        match_length = prefix_length;
      }
    }
  }
  if ( match_length >= 0 ) {
    cerr << "sent datagram\n";
    dgram.header.ttl--; // this made dgram bad,,, why???
    cerr << dgram.header.to_string() << " ttl is " << (int)dgram.header.ttl << endl;
    Router::interface( interface_num )
      .send_datagram( dgram, next_hop.value_or( Address::from_ipv4_numeric( ip_address ) ) );
  }
}

void Router::route()
{
  for ( auto& interface : interfaces_ ) {
    optional<InternetDatagram> dgram;
    while ( ( dgram = interface.maybe_receive() ).has_value() ) {
      route_datagram( dgram.value() );
    }
  }
}
