#include "openstaller_pack_compress.h"

#include <string.h>

#if defined(OPENSTALLER_HAVE_ZLIB)
#include <zlib.h>

#define OS_PACK_COMPRESS_BUFFER_SIZE 65536u

int os_pack_deflate_stream(FILE *in, FILE *out, uint64_t *written_out)
{
    unsigned char in_buffer[OS_PACK_COMPRESS_BUFFER_SIZE];
    unsigned char out_buffer[OS_PACK_COMPRESS_BUFFER_SIZE];
    z_stream stream;
    int flush;
    int status;

    memset(&stream, 0, sizeof(stream));
    if (deflateInit(&stream, Z_BEST_COMPRESSION) != Z_OK) {
        return -1;
    }

    *written_out = 0;
    do {
        stream.avail_in = (uInt)fread(in_buffer, 1, sizeof(in_buffer), in);
        if (ferror(in) != 0) {
            deflateEnd(&stream);
            return -1;
        }

        flush = feof(in) ? Z_FINISH : Z_NO_FLUSH;
        stream.next_in = in_buffer;

        do {
            size_t produced;

            stream.avail_out = sizeof(out_buffer);
            stream.next_out = out_buffer;
            status = deflate(&stream, flush);
            if (status == Z_STREAM_ERROR) {
                deflateEnd(&stream);
                return -1;
            }

            produced = sizeof(out_buffer) - stream.avail_out;
            if (produced > 0 && fwrite(out_buffer, 1, produced, out) != produced) {
                deflateEnd(&stream);
                return -1;
            }
            *written_out += (uint64_t)produced;
        } while (stream.avail_out == 0);
    } while (flush != Z_FINISH);

    deflateEnd(&stream);
    return status == Z_STREAM_END ? 0 : -1;
}

int os_pack_inflate_stream(FILE *in, FILE *out, uint64_t packed_size)
{
    unsigned char in_buffer[OS_PACK_COMPRESS_BUFFER_SIZE];
    unsigned char out_buffer[OS_PACK_COMPRESS_BUFFER_SIZE];
    z_stream stream;
    int status = Z_OK;
    uint64_t remaining = packed_size;

    memset(&stream, 0, sizeof(stream));
    if (inflateInit(&stream) != Z_OK) {
        return -1;
    }

    while (remaining > 0 && status != Z_STREAM_END) {
        size_t chunk = remaining > sizeof(in_buffer) ? sizeof(in_buffer) : (size_t)remaining;
        if (fread(in_buffer, 1, chunk, in) != chunk) {
            inflateEnd(&stream);
            return -1;
        }
        remaining -= (uint64_t)chunk;

        stream.next_in = in_buffer;
        stream.avail_in = (uInt)chunk;
        while (stream.avail_in > 0 && status != Z_STREAM_END) {
            size_t produced;

            stream.next_out = out_buffer;
            stream.avail_out = sizeof(out_buffer);
            status = inflate(&stream, remaining == 0 ? Z_FINISH : Z_NO_FLUSH);
            if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR) {
                inflateEnd(&stream);
                return -1;
            }

            produced = sizeof(out_buffer) - stream.avail_out;
            if (produced > 0 && fwrite(out_buffer, 1, produced, out) != produced) {
                inflateEnd(&stream);
                return -1;
            }
            if (status == Z_BUF_ERROR && produced == 0) {
                inflateEnd(&stream);
                return -1;
            }
        }
    }

    inflateEnd(&stream);
    if (remaining > 0) {
        return -1;
    }
    return status == Z_STREAM_END ? 0 : -1;
}
#endif
