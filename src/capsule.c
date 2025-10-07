#include "capsule.h"
#include <string.h>
#include <time.h>

uint32_t capsule_checksum(const char* data, size_t size) {
    uint32_t sum = 0;
    for (size_t i = 0; i < size; ++i) sum += (uint8_t)data[i];
    return sum;
}

void capsule_wrap(MessageCapsule* cap, uint32_t seq, const char* msg, size_t size) {
    cap->header.sequence = seq;
    cap->header.timestamp = (uint64_t)time(NULL);
    cap->header.checksum = capsule_checksum(msg, size);

    cap->payload = (char*)msg;
    cap->size = size;

    cap->footer.endMarker = 0xDEADBEEF;
    cap->footer.checksum = cap->header.checksum;
}

int capsule_validate(const MessageCapsule* cap) {
    if (cap->footer.endMarker != 0xDEADBEEF) return 0;
    return capsule_checksum(cap->payload, cap->size) == cap->footer.checksum;
}