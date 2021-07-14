#ifndef _INFLATE_H_
#define _INFLATE_H_

#include <stddef.h>

struct inflate;

typedef size_t (*inflate_reader) (void* in, size_t in_size, void* ctx);
typedef size_t (*inflate_writer) (const void* out, size_t out_size, void* ctx);

extern const size_t inflate_size;
extern const size_t inflate_align;

int inflate_init(struct inflate* ip, inflate_reader read, void* read_ctx, inflate_writer write, void* write_ctx);

#endif
