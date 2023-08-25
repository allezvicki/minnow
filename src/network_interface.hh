#pragma once

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"

#include <cstdint>
#include <iostream>
#include <optional>
#include <queue>
#include <unordered_map>

const uint64_t ARPWaitingTime = 5000;
const uint64_t MappingExpiryTime = 30000;

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
private:
  class Status
  {
    uint64_t ms_ {};
    bool waiting_reply_ { true };
    bool address_valid_ { false };

  public:
    std::queue<InternetDatagram> queue_ {};
    bool waiting_for_reply() const { return waiting_reply_; }
    bool address_valid() const { return address_valid_; }
    bool check_valid() { return address_valid_ = ms_ <= MappingExpiryTime; }
    bool check_waiting() const { return ms_ > ARPWaitingTime; }
    void add( uint64_t ms_to_add ) { ms_ += ms_to_add; }
    void reset() { ms_ = 0; }
    void set_waiting( bool x ) { waiting_reply_ = x; }
    void set_valid( bool x ) { address_valid_ = x; }
  };

  struct EthernetAddressWithStatus
  {
    EthernetAddress ethernet_address_ {};
    Status status_ {};
  };

  // Ethernet (known as hardware, network-access, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as Internet-layer or network-layer) address of the interface
  Address ip_address_;

  std::queue<EthernetFrame> ethernet_frames_ {};
  std::unordered_map<uint32_t, EthernetAddressWithStatus> mapping_ {};

  void send_arp_request( uint32_t next_hop );

  void add_mapping( uint32_t ip_address, EthernetAddress ethernet_address );

public:
  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address );

  // Access queue of Ethernet frames awaiting transmission
  std::optional<EthernetFrame> maybe_send();

  // Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address
  // for the next hop.
  // ("Sending" is accomplished by making sure maybe_send() will release the frame when next called,
  // but please consider the frame sent as soon as it is generated.)
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, returns the datagram.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  std::optional<InternetDatagram> recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );
};
