#pragma once

#include "address.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <sys/types.h>
#include <unordered_map>

// A wrapper for NetworkInterface that makes the host-side
// interface asynchronous: instead of returning received datagrams
// immediately (from the `recv_frame` method), it stores them for
// later retrieval. Otherwise, behaves identically to the underlying
// implementation of NetworkInterface.
class AsyncNetworkInterface : public NetworkInterface
{
  std::queue<InternetDatagram> datagrams_in_ {};

public:
  using NetworkInterface::NetworkInterface;

  // Construct from a NetworkInterface
  explicit AsyncNetworkInterface( NetworkInterface&& interface ) : NetworkInterface( interface ) {}

  // \brief Receives and Ethernet frame and responds appropriately.

  // - If type is IPv4, pushes to the `datagrams_out` queue for later retrieval by the owner.
  // - If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // - If type is ARP reply, learn a mapping from the "target" fields.
  //
  // \param[in] frame the incoming Ethernet frame
  void recv_frame( const EthernetFrame& frame )
  {
    std::cerr << "received datagram\n";
    auto optional_dgram = NetworkInterface::recv_frame( frame );
    if ( optional_dgram.has_value() ) {
      datagrams_in_.push( std::move( optional_dgram.value() ) );
    }
  };

  // Access queue of Internet datagrams that have been received
  std::optional<InternetDatagram> maybe_receive()
  {
    if ( datagrams_in_.empty() ) {
      return {};
    }

    InternetDatagram datagram = std::move( datagrams_in_.front() );
    datagrams_in_.pop();

    return datagram;
  }
};

// A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{

  struct Route
  {
    uint32_t route_prefix;
    uint8_t prefix_length;
    std::optional<Address> next_hop; // Gateway
    size_t interface_num;

    Route( uint32_t a, uint8_t b, std::optional<Address> c, size_t d )
      : route_prefix( a ), prefix_length( b ), next_hop( c ), interface_num( d )
    {}
  };
  // The router's collection of network interfaces
  std::vector<AsyncNetworkInterface> interfaces_ {};
  std::vector<Route> routes_ {};

  void route_datagram( InternetDatagram dgram );

public:
  // Add an interface to the router
  // interface: an already-constructed network interface
  // returns the index of the interface after it has been added to the router
  size_t add_interface( AsyncNetworkInterface&& interface )
  {
    interfaces_.push_back( std::move( interface ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  AsyncNetworkInterface& interface( size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces. For each interface, use the
  // maybe_receive() method to consume every incoming datagram and
  // send it on one of interfaces to the correct next hop. The router
  // chooses the outbound interface and next-hop as specified by the
  // route with the longest prefix_length that matches the datagram's
  // destination address.
  void route();
};
