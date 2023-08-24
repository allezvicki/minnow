#include "tcp_sender.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <random>
#include <string_view>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , ackno_( isn_ )
  , next_seqno_( isn_ )
  , timer_( initial_RTO_ms_ )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return sequence_numbers_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  optional<TCPSenderMessage> mesg {};
  if ( !sender_messages_.empty() ) {
    mesg = std::move( sender_messages_.front() );
    sender_messages_.pop_front();
    // Only new mesgs will be pushed back. Retxs will stay in queue.
    if ( outstanding_messages_.empty()
         || mesg->seqno.unwrap( isn_, checkpoint_ )
              > outstanding_messages_.back().seqno.unwrap( isn_, checkpoint_ ) ) {
      outstanding_messages_.push( mesg.value() );
      /* not what the tests expect. in flight as soon as pushed */
      /* sequence_numbers_in_flight_ += mesg->sequence_length(); */
    }
    if ( !timer_.started() ) {
      timer_.start();
    }
  }
  return mesg;
}

void TCPSender::push( Reader& outbound_stream )
{
  TCPSenderMessage mesg;
  if ( !window_size_ && ackno_ == next_seqno_ ) {
    window_size_ = 1;
  }
  uint16_t payload_size_tot = static_cast<uint16_t>( window_size_ - !syn_ ) < outbound_stream.bytes_buffered()
                                ? static_cast<uint16_t>( window_size_ - !syn_ )
                                : outbound_stream.bytes_buffered();
  while ( payload_size_tot > 0 || !syn_ ) {
    auto sv = outbound_stream.peek();
    uint16_t payload_size
      = payload_size_tot < TCPConfig::MAX_PAYLOAD_SIZE ? payload_size_tot : TCPConfig::MAX_PAYLOAD_SIZE;
    std::string payload { sv.begin(), sv.begin() + payload_size };
    outbound_stream.pop( payload_size );
    if ( outbound_stream.is_finished() && window_size_ > payload_size + !syn_ ) {
      fin_ = true;
    }
    if ( !syn_ ) {
      mesg = { isn_, true, payload, fin_ };
      syn_ = true;
    } else {
      mesg = { next_seqno_, false, payload, fin_ };
    }
    next_seqno_ = next_seqno_ + mesg.sequence_length();
    sequence_numbers_in_flight_ += mesg.sequence_length();
    window_size_ -= mesg.sequence_length();
    sender_messages_.push_back( mesg );
    payload_size_tot -= payload_size;
  }
  if ( outbound_stream.is_finished() && window_size_ && !fin_ ) {
    mesg = { next_seqno_, !syn_, {}, true };
    sender_messages_.push_back( mesg );
    next_seqno_ = next_seqno_ + mesg.sequence_length();
    fin_ = true;
    sequence_numbers_in_flight_ += mesg.sequence_length();
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage { Wrap32( next_seqno_ ), false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.ackno.has_value() && ackno_.unwrap( isn_, checkpoint_ ) <= msg.ackno->unwrap( isn_, checkpoint_ )
       && msg.ackno->unwrap( isn_, checkpoint_ ) <= next_seqno_.unwrap( isn_, checkpoint_ ) ) {
    ackno_ = msg.ackno.value();
    window_size_ = msg.window_size;
    window_size_ -= next_seqno_.unwrap( isn_, checkpoint_ ) - ackno_.unwrap( isn_, checkpoint_ );
    if ( window_size_ < 1 ) {
      window_size_ = 0;
      nonzero_window_size_ = false;
    } else {
      nonzero_window_size_ = true;
    }
    bool popped {};
    while ( !outstanding_messages_.empty() ) {
      if ( ackno_.unwrap( isn_, checkpoint_ ) >= checkpoint_ + outstanding_messages_.front().sequence_length() ) {
        checkpoint_ += outstanding_messages_.front().sequence_length();
        sequence_numbers_in_flight_ -= outstanding_messages_.front().sequence_length();
        outstanding_messages_.pop();
        popped = true;
      } else {
        break;
      }
    }
    timer_.set_RTO( initial_RTO_ms_ );
    if ( !outstanding_messages_.empty() && popped ) { // ack new data, sending data
      timer_.start();                                 // that is, restart
      consecutive_retransmissions_ = 0;
    } else if ( outstanding_messages_.empty() ) { // all data sent
      timer_.reset();
      consecutive_retransmissions_ = 0;
    } // do nothing when no data is newly acked
  } else if ( !msg.ackno.has_value() && !syn_ ) {
    window_size_ = msg.window_size;
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  if ( timer_.started() ) {
    timer_.add( ms_since_last_tick );
    if ( timer_.expired() ) {
      sender_messages_.push_front( outstanding_messages_.front() );
      if ( nonzero_window_size_ ) {
        consecutive_retransmissions_++;
        timer_.double_RTO();
      }
      timer_.reset();
    }
  }
}
