// Process server for soft ioc - log buffer
// Ralph Lange 12/14/2007
// GNU Public License applies - see www.gnu.org

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <stdio.h>


#include "logBuffer.h"

// Count occurrences of character c in string s for max. n bytes
size_t memccnt(const void* s, int c, size_t n) {
    size_t i = 0, m = 0;
    char* ms = (char*) s;

    while ( i < n ) {
        i++;
        if(*ms++ == c) m++;
    }
    return m;
}


// Ring buffer class

logBuffer::logBuffer( unsigned int buffer_size )
{
    _buff        = (char*) calloc( buffer_size, 1 );
    assert(_buff != NULL);

    _buff_end    = _buff + buffer_size;
    _write_p     = _read_p = _buff;
    _max_write   = buffer_size; 
    _max_read = _data_in_buffer = _skipped_lines = 0;
    updateState();
}

// Write 'len' bytes to the buffer, deleting oldest buffer lines if necessary.
void logBuffer::write( const char* data, const unsigned int len )
{
    unsigned int to_do = len;
    unsigned int do_now, done = 0;

    while ( to_do ) {
                                // Full? Make room.
        if ( _max_write == 0 ) {
            skipAtLeast( to_do );
            updateState();
        }
                                // Split wrap-around write
        do_now = ( to_do > _max_write ) ? _max_write : to_do;
                                // Copy in
        memcpy( _write_p, data+done, do_now );
        _write_p += do_now;
        _data_in_buffer += do_now;
        to_do -= do_now;
        done += do_now;
        updateState();
    }
}

// Read a maximum of 'len' bytes from the buffer. After the call,
// 'len' contains the amount of bytes actually copied.
void logBuffer::read( char* data, unsigned int& len )
{
    unsigned int to_do = len;
    unsigned int do_now;
    unsigned int do_miss;
    char b[80];
    char *bp = b;

    len = 0;
                                // Print "missing lines" message if necessary
    if ( _skipped_lines ) {
        sprintf( b, "\n[ procServ: %d lines missing due to logBuffer overflow. ]\n\n",
                 _skipped_lines );
        _skipped_lines = 0;
        do_miss = strlen(b);
        if ( to_do > do_miss ) {
            while ( do_miss ) {
                                // Split wrap-around read
                do_now = ( do_miss > _max_read ) ? _max_read : do_miss;
                                // Copy out.
                memcpy( data+len, bp, do_now );
                bp += do_now;
                do_miss -= do_now;
                len += do_now;
            }
        }
    }
                                // Print buffer contents
    while ( to_do && _data_in_buffer ) {
                                // Split wrap-around read
        do_now = ( to_do > _max_read ) ? _max_read : to_do;
                                // Copy out
        memcpy( data+len, _read_p, do_now );
        _read_p += do_now;
        _data_in_buffer -= do_now;
        to_do -= do_now;
        len += do_now;
        updateState();
    }
}

// Skip full lines freeing at least 'len' bytes.
void logBuffer::skipAtLeast( const unsigned int len )
{
    unsigned int to_do = len;
    int do_now;
    char* n_nl;

                                // Nothing to skip? Empty? Return.
    if ( to_do == 0 || _data_in_buffer == 0 ) return;

    while (to_do) {
                                // Split wrap-around skip
        do_now = ( to_do > _max_read ) ? _max_read : to_do;
                                // Skip, counting newlines
        _skipped_lines += memccnt( _read_p, '\n', do_now);
        _read_p += do_now;
        _data_in_buffer -= do_now;
        to_do -= do_now;
        updateState();
    }
                                // Continue to next newline
    to_do = ( _data_in_buffer > 0 ) ? 1 : 0;
    while (to_do) {
        n_nl = (char*) memchr( _read_p, '\n', _max_read );
        if ( n_nl == NULL ) {
            do_now = _max_read;
            to_do = ( (_data_in_buffer - do_now) > 0 ) ? 1 : 0;
        } else {
            do_now = (n_nl - _read_p) + 1;
            _skipped_lines++;
            to_do = 0;
        }
        _read_p += do_now;
        _data_in_buffer -= do_now;
        updateState();
    }
}

// Buffer accounting:
//   recalculate pointers etc., checking for wrap-arounds.
void logBuffer::updateState()
{
    if ( _read_p == _buff_end )
        _read_p = _buff;
    if ( _write_p == _buff_end )
        _write_p = _buff;

    if ( _write_p == _read_p ) {
        if ( _data_in_buffer > 0 ) {
            _max_write = 0;
            _max_read = _buff_end - _read_p;
        } else {
            _max_write = _buff_end - _write_p;
            _max_read = 0;
        }
    }
    else if ( _write_p > _read_p ) {
        _max_write = _buff_end - _write_p;
        _max_read = _write_p - _read_p;
    } else {
        _max_write = _read_p - _write_p;
        _max_read = _buff_end - _read_p;
    }
}
