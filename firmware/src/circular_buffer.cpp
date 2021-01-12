#include "main.h"

int CircularBuffer::write(const uint8_t c) {
    if (available >= SER_BUFFER_SIZE) {
        overflow = 1;
        return 0;
    } else {
        *b_current = c;
        b_current++;
        available++;
        if (b_current >= b_end) {b_current = buffer;}      
        return 1;
    }
}

int CircularBuffer::write(const uint8_t *s) {
    return write(s, strlen((const char *)s));
}

int CircularBuffer::write(const char *s) {
    return write((uint8_t *)s, strlen(s));
}

int CircularBuffer::write(const char *s, int nbytes) {
    return write((uint8_t *)s, nbytes);
}

int CircularBuffer::write(const uint8_t *s, int nbytes) {
    int bytes_written = min(nbytes, SER_BUFFER_SIZE - available);

    int wrap = (b_current + bytes_written) - b_end;
    if (wrap > 0) {
        // In this case the data wraps around, requiring two memcpy's
        int split = bytes_written - wrap;
        memcpy(b_current, s,         split);
        memcpy(buffer,    s + split, wrap);
    } else {
        // The data does not need to wrap around.
        memcpy(b_current, s, bytes_written);
    }

    b_current += bytes_written;
    if (b_current >= b_end) {b_current -= SER_BUFFER_SIZE;}

    available += bytes_written;
    if (bytes_written != nbytes) {overflow = 1;}

    return bytes_written;
}

uint8_t * CircularBuffer::get_buffer(int * max_data) {
    int current_start = start;

    *max_data = min(min(*max_data, available), SER_BUFFER_SIZE - start);

    start += *max_data;
    if (start >= SER_BUFFER_SIZE) {start = 0;}

    available -= *max_data;

    return buffer + current_start;
}

CircularBuffer::CircularBuffer() {
    start = 0;
    available = 0;
    overflow = 0;
    b_current = buffer;
    b_end = buffer + SER_BUFFER_SIZE;
}

int CircularBuffer::from_stream(Stream &stream) {
    int bytes_written = max(min(stream.available(), SER_BUFFER_SIZE - available), 0);

    if (bytes_written) {
        int wrap = (b_current + bytes_written) - b_end;
        if (wrap > 0) {
            // In this case the data wraps around, requiring two memcpy's
            int split = bytes_written - wrap;
            stream.readBytes(b_current, split);
            stream.readBytes(buffer, wrap);
        } else {
            // The data does not need to wrap around.
            stream.readBytes(b_current, bytes_written);
        }

        b_current += bytes_written;
        if (b_current >= b_end) {b_current -= SER_BUFFER_SIZE;}

        available += bytes_written;
    }

    return bytes_written;
}

int CircularBuffer::to_stream(HardwareSerial &stream) {
    int n = min(stream.availableForWrite(), available);

    if (n) {
        int wrap = (start + n) - SER_BUFFER_SIZE;
        if (wrap > 0) {
            int split = n - wrap;
            stream.write(buffer + start, split);
            stream.write(buffer, wrap);
        } else {
            stream.write(buffer + start, n);
        }

        start += n;
        if (start >= SER_BUFFER_SIZE) {start -= SER_BUFFER_SIZE;}

        available -= n;
    }

    return n;
}


#ifdef BLUETOOTH_ENABLED
int CircularBuffer::to_stream(BluetoothSerial &stream) {
    int n = min(64, available);
    int written = 0;
    // Note: the BluetoothSerial object does not have a avaiableForWrite method, and
    //   so there is no way to tell how much data is in the buffer.
    // As a workaround, we can see how much was actually written.

    if (n) {
        int wrap = (start + n) - SER_BUFFER_SIZE;
        if (wrap > 0) {
            int split = n - wrap;
            written = stream.write(buffer + start, split);
            if (written == split) {
                // Only send more data if it got the first bit!
                written += stream.write(buffer, wrap);
            }
        } else {
            written = stream.write(buffer + start, n);
        }

        // Only advance as much as was actually written!
        start += written;
        if (start >= SER_BUFFER_SIZE) {start -= SER_BUFFER_SIZE;}

        available -= written;
    }

    return written;
}
#endif

int CircularBuffer::to_stream(CircularBuffer &buf) {
    int n = min(SER_BUFFER_SIZE - buf.available, available);

    if (n) {
        int wrap = (start + n) - SER_BUFFER_SIZE;
        if (wrap > 0) {
            int split = n - wrap;
            buf.write(buffer + start, split);
            buf.write(buffer, wrap);
        } else {
            buf.write(buffer + start, n);
        }

        start += n;
        if (start >= SER_BUFFER_SIZE) {start -= SER_BUFFER_SIZE;}

        available -= n;
    }

    return n;
}

void CircularBuffer::flush() {
    b_current = buffer;
    start = 0;
    available = 0;
    overflow = 0;
}

// int CircularBuffer::to_stream(CircularBuffer &buf) {
//     int n = min(buf.available, available);

//     if (n) {
//         // This is complicated because the buffers may wrap around in weird ways
//         // Unfortunately we need to consider each of six possible cases!
//         int wrap_s = (start + n) - SER_BUFFER_SIZE;
//         int wrap_d = (buf.b_current + n) - buf.b_end;
//         int split_s = n - wrap_s;
//         int split_d = n - wrap_d;

//         if (wrap_s > 0) {
//             if (wrap_d > 0) {
//                 // Nightmare scenario: both buffers wrap ):
//                 if (split_s == split_d) {
//                     // Fortunate coincidence: both wrap at the same point.
//                     memcpy(buf.b_current, buffer + start, split_s);
//                     memcpy(buf.buffer,    buffer,         split_s);
//                 } else if (split_s < split_d) {
//                     // The source wraps before the destination
//                     memcpy(buf.b_current, buffer + start, )
//                 } else {

//                 }
//             } else {
//                 // Source buffer wraps, destination does not
//                 memcpy(buf.b_current,           buffer + start, split_s);
//                 memcpy(buf.b_current + split_s, buffer,         wrap_s);
//             }
//         } else if (wrap_d > 0) {
//             // Destination wraps around, source does not
//             memcpy(buf.b_current, buffer + start,           split_d);
//             memcpy(buf.buffer,    buffer + start + split_d, wrap_d);
//         } else {
//             // Neither buffer wraps
//             memcpy(buf.b_current, buffer + start, n);
//         }

//         start += n;
//         if (start >= SER_BUFFER_SIZE) {start -= SER_BUFFER_SIZE;}
//         available -= n;

//         buf.b_current += n;
//         if (buf.b_current >= buf.b_end) {buf.b_current -= SER_BUFFER_SIZE;}
//         buf.available += n;

//     }

//     return n;
// }
