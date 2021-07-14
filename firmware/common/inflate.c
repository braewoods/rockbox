#include "inflate.h"
#include <stdbool.h>
#include <stdint.h>
#include "system.h"
#include "rbendian.h"

enum {
    INFLATE_BUFFER_SIZE = 0x8000,
};

struct inflate_reader {
    inflate_reader read;
    void* ctx;
    size_t offset;
    size_t length;
    size_t trailing_bits;
    size_t total_bits;
    size_t read_bytes;
    uint32_t data32[INFLATE_BUFFER_SIZE / sizeof(uint32_t)];
    uint32_t bits;
};

struct inflate_writer {
    inflate_writer write;
    void* ctx;
    size_t wrote_bytes;
    uint16_t offset;
    uint16_t flush_offset;
    uint8_t data8[INFLATE_BUFFER_SIZE * 2];
};

struct inflate {
    struct inflate_reader reader;
    struct inflate_writer writer;
};

static void inflate_reader_init(struct inflate_reader* r, inflate_reader read, void* ctx) {
    r->read = read;
    r->ctx = ctx;
    r->total_bits = 0;
    r->read_bytes = 0;
    r->bits = 0;
}

static int inflate_reader_fill_data(struct inflate_reader* r) {
    size_t size = r->read(r->data32, sizeof(r->data32), r->ctx);

    if (size == 0)
        return -1;

    r->offset = 0;
    r->length = (size / sizeof(uint32_t));
    r->trailing_bits = (size % sizeof(uint32_t)) * 8;
    r->read_bytes += size;

    return 0;
}

static int inflate_reader_fill_bits(struct inflate_reader* r) {
    int rv;

    if (r->offset == r->length) {
        if (r->trailing_bits != 0) {
            r->bits = (letoh32(r->data32[r->offset]) & ((1 << r->trailing_bits) - 1));
            r->total_bits = r->trailing_bits;
            r->trailing_bits = 0;
            return 0;
        }

        if ((rv = inflate_reader_fill_data(r)) != 0)
            return rv;
    }

    r->total_bits = (sizeof(uint32_t) * 8);
    r->bits = letoh32(r->data32[r->offset++]);

    return 0;
}

static int inflate_reader_get_bits(struct inflate_reader* r, size_t read_bits, uint32_t* out_bits) {
    size_t shift_bits;
    int rv;

    if (read_bits <= r->total_bits) {
        out_bits[0] = (r->bits & ((1 << read_bits) - 1));
        r->total_bits -= read_bits;
        r->bits >>= read_bits;
        return 0;
    }

    read_bits -= r->total_bits;
    shift_bits = r->total_bits;
    out_bits[0] = r->bits;

    if ((rv = inflate_reader_fill_bits(r)) != 0)
        return rv;

    if (read_bits > r->total_bits)
        return -1;

    out_bits[0] |= ((r->bits & ((1 << read_bits) - 1)) << shift_bits);
    r->total_bits -= read_bits;
    r->bits >>= read_bits;

    return 0;
}

static int inflate_reader_get_byte(struct inflate_reader* r, uint8_t* byte) {
    uint32_t bits;
    int rv;

    if ((rv = inflate_reader_get_bits(r, 8, &bits)) != 0)
        return rv;

    byte[0] = bits;

    return 0;
}

static int inflate_reader_get_word(struct inflate_reader* r, uint16_t* word) {
    uint32_t bits;
    int rv;

    if ((rv = inflate_reader_get_bits(r, 16, &bits)) != 0)
        return rv;

    word[0] = bits;

    return 0;
}

static void inflate_reader_discard_byte(struct inflate_reader* r) {
    size_t discard_bits = (r->total_bits % 8);

    r->total_bits -= discard_bits;
    r->bits >>= discard_bits;
}

static void inflate_writer_init(struct inflate_writer* w, inflate_writer write, void* ctx) {
    w->write = write;
    w->ctx = ctx;
    w->wrote_bytes = 0;
    w->offset = 0;
    w->flush_offset = (sizeof(w->data8) - 1);
}

static int inflate_writer_flush_loop(struct inflate_writer* w) {
    size_t size = (sizeof(w->data8) / 2);
    uint16_t offset = (w->flush_offset + 1);

    if (w->write(w->data8 + offset, size, w->ctx) != size)
        return -1;

    w->wrote_bytes += size;
    w->flush_offset = (offset + (size - 1));

    return 0;
}

static int inflate_writer_flush_last(struct inflate_writer* w) {
    int rv;
    uint16_t offset;
    size_t size;

    if (w->wrote_bytes == 0) {
        offset = 0;
        size = w->offset;
        goto last_write;
    }

    if ((rv = inflate_writer_flush_loop(w)) != 0)
        return rv;

    offset = (w->flush_offset + 1);
    size = (w->offset - offset);

last_write:

    if (w->write(w->data8 + offset, size, w->ctx) != size)
        return -1;

    w->wrote_bytes += size;

    return 0;
}

static int inflate_writer_put_byte(struct inflate_writer* w, uint8_t byte) {
    int rv;

    w->data8[w->offset] = byte;

    if (w->offset == w->flush_offset && (rv = inflate_writer_flush_loop(w)) != 0)
        return rv;

    ++w->offset;

    return 0;
}

static int inflate_uncompressed_block(struct inflate_reader* r, struct inflate_writer* w) {
    int rv;
    uint16_t length;
    uint16_t length_complement;

    inflate_reader_discard_byte(r);

    if ((rv = inflate_reader_get_word(r, &length)) != 0)
        return rv;

    if ((rv = inflate_reader_get_word(r, &length_complement)) != 0)
        return rv;

    if (length != (length_complement ^ 0xffff))
        return -1;

    for (; length > 0; --length) {
        uint8_t byte;

        if ((rv = inflate_reader_get_byte(r, &byte)) != 0)
            return rv;

        if ((rv = inflate_writer_put_byte(w, byte)) != 0)
            return rv;
    }

    return 0;
}

const size_t inflate_size = sizeof(struct inflate);
const size_t inflate_align = _Alignof(struct inflate);

int inflate_init(struct inflate* ip, inflate_reader read, void* read_ctx, inflate_writer write, void* write_ctx) {
    if (ip == NULL || read == NULL || write == NULL)
        return -1;

    inflate_reader_init(&ip->reader, read, read_ctx);
    inflate_writer_init(&ip->writer, write, write_ctx);

    return 0;
}
