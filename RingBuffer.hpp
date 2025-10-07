#pragma once
#include <atomic>
#include <vector>
#include <string>
#include <cstdint>
#include <mutex>
#include <memory>

// =============
// Message Format
// =============
struct Message {
    uint64_t msg_id;
    uint32_t producer_id;
    uint32_t consumer_id;
    uint32_t meta_type;
    std::string meta;
    std::string payload;
};

struct Feedback {
    uint8_t status; // 0=none, 1=ack, 2=nack...
    std::string detail;
};

// =============
// Slot State
// =============
enum SlotState : uint8_t { FREE=0, READY, CONSUMING, FEEDBACK };

struct Slot {
    std::atomic<uint8_t> state{FREE};
    Message msg;
    Feedback fb;
};

// =============
// Producer Segment
// =============
struct ProducerSegment {
    uint32_t id;
    uint32_t slots_count;
    std::atomic<uint64_t> head{0};
    std::vector<std::unique_ptr<Slot>> slots;
    bool active{true};

    ProducerSegment(uint32_t pid, uint32_t count)
        : id(pid), slots_count(count) {
        slots.reserve(count);
        for (uint32_t i=0; i<count; i++) {
            slots.emplace_back(std::make_unique<Slot>());
        }
    }
};

// =============
// RingBuffer Manager (Dynamic)
// =============
struct RingBuffer {
    std::vector<std::unique_ptr<ProducerSegment>> producers;
    std::vector<bool> consumers;
    std::mutex lock;  // protects attach/detach

    // Attach producer dynamically
    uint32_t attachProducer(uint32_t slots_per_producer) {
        std::lock_guard<std::mutex> g(lock);
        uint32_t pid = producers.size();
        producers.push_back(std::make_unique<ProducerSegment>(pid, slots_per_producer));
        return pid;
    }

    // Detach producer dynamically (delete memory)
    void detachProducer(uint32_t pid) {
        std::lock_guard<std::mutex> g(lock);
        if (pid < producers.size() && producers[pid]) {
            producers[pid].reset(); // free memory
        }
    }

    // Attach consumer dynamically
    uint32_t attachConsumer() {
        std::lock_guard<std::mutex> g(lock);
        uint32_t cid = consumers.size();
        consumers.push_back(true);
        return cid;
    }

    // Detach consumer dynamically
    void detachConsumer(uint32_t cid) {
        std::lock_guard<std::mutex> g(lock);
        if (cid < consumers.size()) consumers[cid] = false;
    }

    // Producer writes message
    bool produce(uint32_t pid, const Message& m) {
        if (pid >= producers.size() || !producers[pid]) return false;
        ProducerSegment* seg = producers[pid].get();
        if (!seg->active) return false;

        uint64_t idx = seg->head.fetch_add(1) % seg->slots_count;
        Slot* s = seg->slots[idx].get();

        uint8_t expected = FREE;
        if (!s->state.compare_exchange_strong(expected, READY)) return false;

        s->msg = m;
        s->msg.producer_id = pid;
        s->state.store(READY);
        return true;
    }

    // Consumer fetches any message for itself
    bool consume(uint32_t cid, Message& out_msg, uint32_t& out_pid, uint32_t& out_idx) {
        if (cid >= consumers.size() || !consumers[cid]) return false;

        for (size_t p=0; p<producers.size(); p++) {
            if (!producers[p]) continue;
            ProducerSegment* seg = producers[p].get();
            for (uint32_t i=0; i<seg->slots_count; i++) {
                Slot* s = seg->slots[i].get();
                if (s->state.load() == READY && s->msg.consumer_id == cid) {
                    uint8_t expected = READY;
                    if (s->state.compare_exchange_strong(expected, CONSUMING)) {
                        out_msg = s->msg;
                        out_pid = seg->id;
                        out_idx = i;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Consumer writes feedback
    void writeFeedback(uint32_t pid, uint32_t idx, uint8_t status, const std::string& detail) {
        if (pid >= producers.size() || !producers[pid]) return;
        ProducerSegment* seg = producers[pid].get();
        if (!seg) return;

        Slot* s = seg->slots[idx].get();
        s->fb.status = status;
        s->fb.detail = detail;
        s->state.store(FEEDBACK);
    }

    // Producer collects feedback
    bool collectFeedback(uint32_t pid, Feedback& out_fb, uint64_t& msg_id) {
        if (pid >= producers.size() || !producers[pid]) return false;
        ProducerSegment* seg = producers[pid].get();
        if (!seg) return false;

        for (uint32_t i=0; i<seg->slots_count; i++) {
            Slot* s = seg->slots[i].get();
            if (s->state.load() == FEEDBACK) {
                out_fb = s->fb;
                msg_id = s->msg.msg_id;
                s->state.store(FREE);
                return true;
            }
        }
        return false;
    }
};