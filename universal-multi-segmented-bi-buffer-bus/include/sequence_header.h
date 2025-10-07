/* sequence_header.h - atomic sequence + checksum (C API) */
#ifndef SEQUENCE_HEADER_H
#define SEQUENCE_HEADER_H

#include <stdint.h>

typedef struct SequenceHeader {
    uint64_t seq;
    uint32_t checksum;
} SequenceHeader;

uint32_t seq_compute_checksum(const SequenceHeader* h);

#endif /* SEQUENCE_HEADER_H */
