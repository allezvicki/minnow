#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"
#include <cstdint>
#include <optional>

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

void NetworkInterface::send_arp_request( uint32_t next_hop )
{
  ARPMessage arp {};
  arp.opcode = ARPMessage::OPCODE_REQUEST;
  arp.sender_ethernet_address = ethernet_address_;
  arp.sender_ip_address = ip_address_.ipv4_numeric();
  arp.target_ip_address = next_hop;
  ethernet_frames_.push(
    { EthernetHeader { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP }, serialize( arp ) } );
}

void NetworkInterface::add_mapping( uint32_t ip_address, EthernetAddress ethernet_address )
{
  auto it = mapping_.find( ip_address );
  if ( it == mapping_.end() ) {
    it = mapping_.insert( { ip_address, {} } ).first;
  }
  it->second.ethernet_address_ = ethernet_address;
  auto& sts = it->second.status_;
  sts.set_valid( true );
  sts.set_waiting( false );
  sts.reset();
  // Empty pending mesgs on this address
  while ( !sts.queue_.empty() ) {
    send_datagram( sts.queue_.front(), Address::from_ipv4_numeric( ip_address ) );
    sts.queue_.pop();
  }
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  cerr << "sending datagram, next hop is  " << next_hop.to_string() << endl;
  auto it = mapping_.find( next_hop.ipv4_numeric() );
  if ( it != mapping_.end() ) { // If not the first time knowing about this address
    auto& sts = it->second.status_;
    if ( sts.address_valid() ) {
      auto dst = it->second.ethernet_address_;
      ethernet_frames_.push(
        { EthernetHeader { dst, ethernet_address_, EthernetHeader::TYPE_IPv4 }, serialize( dgram ) } );
    } else if ( !sts.waiting_for_reply() ) { // if either vailid nor waiting for reply
      // send arp request
      // when arp requet received, redo send_datagram
      send_arp_request( next_hop.ipv4_numeric() );

      // mark as waiting for reply
      sts.set_waiting( true );
      sts.reset();
      sts.queue_.push( dgram );
    } else { // If waiting for reply
      sts.queue_.push( dgram );
    }
  } else { // if hit an address for the first time
    send_arp_request( next_hop.ipv4_numeric() );
    auto pair = mapping_.insert( { next_hop.ipv4_numeric(), {} } );
    pair.first->second.status_.queue_.push( dgram );
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  optional<InternetDatagram> ret {};
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp;
    if ( !parse( arp, frame.payload )
         || ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST )
         || arp.target_ip_address != ip_address_.ipv4_numeric() ) {
      return ret;
    }
    if ( arp.opcode == ARPMessage::OPCODE_REQUEST ) {
      ARPMessage send_arp;
      send_arp.opcode = ARPMessage::OPCODE_REPLY;
      send_arp.sender_ethernet_address = ethernet_address_;
      send_arp.sender_ip_address = ip_address_.ipv4_numeric();
      send_arp.target_ethernet_address = frame.header.src;
      send_arp.target_ip_address = arp.sender_ip_address;
      ethernet_frames_.push( { EthernetHeader { frame.header.src, ethernet_address_, EthernetHeader::TYPE_ARP },
                               serialize( send_arp ) } );

      // learn from the request as well
      add_mapping( arp.sender_ip_address, arp.sender_ethernet_address );
    } else if ( arp.opcode == ARPMessage::OPCODE_REPLY ) {
      add_mapping( arp.sender_ip_address, arp.sender_ethernet_address );
    }
  }
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    // Only make sure the ethernet dst is correct.
    // dst may not be our ip address (maybe this is next_hop)
    if ( frame.header.dst != ethernet_address_ ) {
      return ret;
    }
    InternetDatagram dgram;
    if ( !parse( dgram, frame.payload ) ) {
      return ret;
    }
    ret = dgram;
  }
  return ret;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // NOTE! auto& !!! Don't forget &!!!
  for ( auto& it : mapping_ ) {
    auto& sts = it.second.status_;
    if ( sts.address_valid() ) {
      sts.add( ms_since_last_tick );
      if ( !sts.check_valid() ) {
        // mapping expired
        sts.reset();
      }
    } else if ( sts.waiting_for_reply() ) {
      sts.add( ms_since_last_tick );
      if ( sts.check_waiting() ) {
        // waiting expired, resend arp request
        sts.reset();
        send_arp_request( it.first );
      }
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  optional<EthernetFrame> ef {};
  if ( !ethernet_frames_.empty() ) {
    /* ef = std::move( ethernet_frames_.front() ); */
    ef = ethernet_frames_.front();
    ethernet_frames_.pop();
  }
  return ef;
}
