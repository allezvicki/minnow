#include "tcp_receiver.hh"
#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  Wrap32 seqno( message.seqno );
  if ( message.SYN ) {
    _zero_point = seqno;
  }
  if ( _zero_point.has_value() ) {
    uint64_t first_index = seqno.unwrap( _zero_point.value(), reassembler.first_unassembled() );
    if ( !message.SYN ) {
      first_index--;
    }
    reassembler.insert( first_index, message.payload, message.FIN, inbound_stream );
    _ackno = Wrap32::wrap( reassembler.first_unassembled() + 1, _zero_point.value() );
    if ( message.FIN ) {
      _fin_aseqno = first_index + message.payload.size();
    }
    if ( _fin_aseqno == reassembler.first_unassembled() ) {
      _ackno = _ackno.value() + 1;
    }
  }
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  uint16_t window_size = UINT16_MAX;
  if ( inbound_stream.available_capacity() < UINT16_MAX ) {
    window_size = inbound_stream.available_capacity();
  }
  return TCPReceiverMessage { _ackno, window_size };
}
