#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity ), available_capacity_( capacity ), error_( false ), closed_( false )
{
  buffer_.resize( capacity_ );
}

void Writer::push( string data )
{
  uint64_t len = data.length();
  if ( len > available_capacity_ ) {
    len = available_capacity_;
  }
  if ( capacity_ - wpointer_ < len ) {
    data.copy( buffer_.data() + wpointer_, capacity_ - wpointer_ );
    data.copy( buffer_.data(), len - capacity_ + wpointer_, capacity_ - wpointer_ );
  } else {
    data.copy( buffer_.data() + wpointer_, len );
  }
  pushed_ += len;
  wpointer_ += len;
  wpointer_ -= wpointer_ >= capacity_ ? capacity_ : 0;
  available_capacity_ -= len;
}

void Writer::close()
{
  closed_ = true;
}

void Writer::set_error()
{
  error_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return available_capacity_;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return pushed_;
}

string_view Reader::peek() const
{
  static string view;
  if ( rpointer_ > wpointer_ || ( rpointer_ == wpointer_ && available_capacity_ == 0 ) ) {
    view.assign( buffer_, rpointer_, capacity_ - rpointer_ );
    view += buffer_.substr( 0, wpointer_ );
    return view;
  }
  string_view v = buffer_;
  return v.substr( rpointer_, wpointer_ - rpointer_ );
}

bool Reader::is_finished() const
{
  return closed_ && available_capacity_ == capacity_;
}

bool Reader::has_error() const
{
  // Your code here.
  return error_;
}

void Reader::pop( uint64_t len )
{
  if ( len > capacity_ - available_capacity_ ) {
    len = capacity_ - available_capacity_;
  }
  rpointer_ += len;
  available_capacity_ += len;
  if ( rpointer_ >= capacity_ ) {
    rpointer_ -= capacity_;
  }
}

uint64_t Reader::bytes_buffered() const
{
  return capacity_ - available_capacity_;
}

uint64_t Reader::bytes_popped() const
{
  return pushed_ - capacity_ + available_capacity_;
}
