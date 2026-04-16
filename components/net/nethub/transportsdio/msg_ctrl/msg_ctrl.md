# nh_msg_ctrl Message Controller

## Overview

`nh_msg_ctrl` is a bidirectional SDIO message router and manager. It provides a
tag-based dispatch model for device-side message handling. The module receives
messages from multiple upper-layer services, classifies them by tag, schedules
them through queues, and dispatches callbacks when upload or download work is
completed.

## Design Goals

- Tag-based message routing
- Asynchronous upload and download processing
- ISR-safe fast paths
- Queue-based task-context dispatch for expensive work
- Reuse of frame buffer pools for download buffers

## Processing Model

### Upload Path

1. Upper layer allocates a frame buffer
2. Message header is populated with tag/sub-tag/length
3. Message is queued for upload
4. SDIO transmission starts
5. Upload completion interrupt fires
6. Upload callback is executed immediately in ISR context

### Download Path

1. SDIO download completes in interrupt context
2. Basic validation is performed
3. The received frame is queued
4. The message-processing task dequeues the frame
5. Tag-based callback dispatch happens in task context
6. Buffer is released after processing

This split keeps ISR latency small while allowing complex application logic to
run in normal task context.

## Main Components

- `upld_queue`
  Queue of pending upload frames

- `dnld_queue`
  Queue of downloaded frames waiting for dispatch

- `dnld_queue_ctrl`
  Buffer-pool controller for download frames

- `msg_proc_task`
  Main processing task that advances upload, download, and dispatch stages

- Per-tag callback tables
  Upload and download callbacks are indexed directly by message tag

## Message Format

```c
struct nh_msg_packt {
    uint8_t tag;
    uint8_t sub_tag;
    uint16_t len;
    uint8_t data[0];
};
```

## Key Behaviors

### Tag Routing

- Each message carries a tag
- The controller uses the tag as a direct index into the callback table
- Upload and download callbacks can be registered independently

### Queueing Strategy

- Upload queue absorbs burst traffic from producers
- Download queue decouples ISR reception from application processing
- Pool queue manages reusable frame buffers

### Memory Management

- Static frame pools are used where possible
- Buffers are reused instead of allocating dynamically per message
- Alignment constraints are preserved for DMA efficiency

## Thread Safety

- ISR-facing APIs are designed to be interrupt-safe
- Queue operations are used to hand work over to task context
- Callback registration is protected from concurrent access

## Recommended Usage

1. Initialize the message controller
2. Register upload/download callbacks for the required tags
3. Start the processing task
4. Allocate frame buffers from the provided pool
5. Send messages through the upload APIs
6. Handle incoming messages in the registered callbacks

## Typical Responsibilities by Context

### ISR Context

- Hardware completion handling
- Quick validation
- Queue push/pop
- Fast upload-complete callbacks

### Task Context

- Download dispatch
- Protocol parsing
- Error recovery
- Higher-level business logic

## Notes

- Keep ISR callbacks small and non-blocking
- Use task-context callbacks for parsing and heavy processing
- Prefer reusing buffers from the pool instead of allocating ad hoc
- Preserve tag-table consistency when adding new message types
