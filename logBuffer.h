// Process server for soft ioc - log buffer
// Ralph Lange 12/14/2007
// GNU Public License applies - see www.gnu.org

#ifndef _LOGBUFFER_H_
#define _LOGBUFFER_H_

// Ring buffer class
class logBuffer {
public:
    logBuffer( unsigned int buffer_size );
    
    void write( const char* data, const unsigned int len );
    void read( char* data, unsigned int& len );
    
private:
    void skipAtLeast( const unsigned int len );
    void updateState();

    char *_write_p, *_read_p;
    char *_buff_end;
    char *_buff;
    unsigned int _max_write, _max_read, _data_in_buffer, _skipped_lines;
};

#endif
