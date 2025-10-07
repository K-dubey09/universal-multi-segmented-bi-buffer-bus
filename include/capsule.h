#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t sequence;
    uint64_t timestamp;
    uint32_t checksum;
} CapsuleHeader;

typedef struct {
    uint32_t endMarker;
    uint32_t checksum;
} CapsuleFooter;

typedef struct {
    CapsuleHeader header;
    char* payload;
    size_t size;
    CapsuleFooter footer;
} MessageCapsule;

void capsule_wrap(MessageCapsule* cap, uint32_t seq, const char* msg, size_t size);
int capsule_validate(const MessageCapsule* cap);
uint32_t capsule_checksum(const char* data, size_t size);