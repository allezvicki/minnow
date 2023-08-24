#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <deque>
#include <sys/types.h>

class Timer
{
private:
  uint64_t RTO_ms_;
  uint64_t ms_ {};
  bool started_ {};

public:
  explicit Timer( uint64_t RTO_ms ) : RTO_ms_( RTO_ms ) {}
  void start()
  {
    ms_ = 0;
    started_ = true;
  }
  bool expired() const { return ms_ >= RTO_ms_; }
  bool started() const { return started_; }
  void reset() { started_ = false; }
  void set_RTO( uint64_t new_RTO ) { RTO_ms_ = new_RTO; }
  void double_RTO() { RTO_ms_ <<= 1; }
  void add( uint64_t ms_to_add ) { ms_ += ms_to_add; }
};

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  std::deque<TCPSenderMessage> sender_messages_ {};
  std::queue<TCPSenderMessage> outstanding_messages_ {};
  Wrap32 ackno_;
  Wrap32 next_seqno_;
  uint64_t checkpoint_ {};
  uint64_t consecutive_retransmissions_ {};
  uint64_t sequence_numbers_in_flight_ {};
  uint16_t window_size_ { 1 };
  bool syn_ {};
  bool fin_ {};
  bool nonzero_window_size_ { true };
  Timer timer_;

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
