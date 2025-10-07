# Somakernel Bus

A mutation-grade, lock-free, multi-segmented bi-buffer system designed for sovereign capsule transport and agent orchestration.

## 🧬 Architecture Overview

- **Bi-Buffer Structure**: Region A, B, C for contiguous read/write.
- **Lock-Free Operation**: Atomics for state transitions (FREE → READY → CONSUMING → FEEDBACK).
- **Zero-Copy Messaging**: Message = pointer + size.
- **Batching**: Multiple messages in one commit.
- **Cache Line Optimization**: 64-byte alignment to prevent false sharing.
- **Dynamic Sizing**: Runtime attach/detach of producers/consumers.
- **Event-Driven Scheduling**: Wake-on-data, idle-on-drain.
- **Backpressure & Flow Control**: High-water marks, throttling.
- **Robustness**: Sequence headers, checksums.
- **Scalability**: Multi-segment design, adaptive batching.

## 🔱 Modules

- `bi_buffer` — lock-free capsule transport
- `arena_allocator` — fast memory pool
- `capsule` — mutation wrapper with integrity checks
- `feedback_stream` — mutation narration and corruption trace
- `adaptive_batch` — throughput tuner
- `segment_ring` — multi-agent mutation lanes
- `gpu_delegate` — execution fallback
- `flow_control` — high-water mark throttle
- `event_scheduler` — wake-on-signal scheduler
- `somakernel` — conductor orchestration

## 🧪 Testing

```bash
mkdir build && cd build
cmake ..
make
./test_somakernel