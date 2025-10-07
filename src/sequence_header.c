/* sequence_header.c */
#include "../include/sequence_header.h"
#include <stdint.h>

uint32_t seq_compute_checksum(const SequenceHeader* h){ uint32_t c = (uint32_t)(h->seq ^ (h->seq>>32)); c ^= h->checksum; return c; }
