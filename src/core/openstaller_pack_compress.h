#ifndef OPENSTALLER_PACK_COMPRESS_H
#define OPENSTALLER_PACK_COMPRESS_H

#include <stdint.h>
#include <stdio.h>

#if defined(OPENSTALLER_HAVE_ZLIB)
int os_pack_deflate_stream(FILE *in, FILE *out, uint64_t *written_out);
int os_pack_inflate_stream(FILE *in, FILE *out, uint64_t packed_size);
#endif

#endif
