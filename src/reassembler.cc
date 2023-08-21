#include "reassembler.hh"
#include <algorithm>
#include <cstdint>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  if ( is_last_substring ) {
    _end = data.length() + first_index;
  }
  if ( _first_unassembled >= _end ) {
    output.close();
  }
  _buffer.resize( output.capacity() );
  _mark.resize( output.capacity() );
  uint64_t l = max( first_index, _first_unassembled );
  uint64_t r = min( first_index + data.length(), output.bytes_pushed() + output.available_capacity() );
  if ( l >= r ) {
    return;
  }
  for ( uint64_t i = _first_unassembled_pos + l - _first_unassembled; l < r; l++, i++ ) {
    i -= i >= output.capacity() ? output.capacity() : 0;
    _buffer[i] = data[l - first_index];
    if ( !_mark[i] ) {
      _mark[i] = 1;
      _bytes_pending += 1;
    }
  }
  if ( first_index <= _first_unassembled ) {
    uint64_t i = _first_unassembled_pos;
    do {
      i -= i >= output.capacity() ? output.capacity() : 0;
      if ( !_mark[i] ) {
        break;
      }
      _mark[i] = 0;
      i++;
    } while ( i != _first_unassembled_pos );
    uint64_t written = 0;
    if ( _first_unassembled_pos < i ) {
      output.push( { _buffer.begin() + _first_unassembled_pos, _buffer.begin() + i } );
      written = i - _first_unassembled_pos;
    } else {
      output.push( { _buffer.begin() + _first_unassembled_pos, _buffer.end() } );
      output.push( { _buffer.begin(), _buffer.begin() + i } );
      written = output.capacity() - _first_unassembled_pos + i;
    }
    _first_unassembled += written;
    _bytes_pending -= written;
    _first_unassembled_pos = i;
    if ( _first_unassembled >= _end ) {
      output.close();
    }
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return _bytes_pending;
}
