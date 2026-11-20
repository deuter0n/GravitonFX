// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <vector>

namespace gfx {

// The read and write positions sit on separate cache lines on purpose, so
// MSVC's "structure was padded" warning is expected here.
#pragma warning(push)
#pragma warning(disable : 4324)

// Lock-free ring buffer of mono float samples for exactly one producer thread
// and one consumer thread. All memory is allocated in the constructor, so
// write, read, peek and skip are real-time safe.
//
// The read and write positions only ever increase; the capacity is a power of
// two, so a position maps to a slot with a bit mask.
class SpscRing {
public:
    // Rounds the capacity up to the next power of two.
    explicit SpscRing(size_t minCapacity)
    {
        size_t capacity = 1;
        while (capacity < minCapacity) {
            capacity <<= 1;
        }
        buffer_.assign(capacity, 0.0f);
        mask_ = capacity - 1;
    }

    size_t capacity() const { return buffer_.size(); }

    // Samples available to read. Exact on the consumer thread, a lower bound
    // on the producer thread.
    size_t size() const
    {
        return writePos_.load(std::memory_order_acquire) - readPos_.load(std::memory_order_acquire);
    }

    // Producer: appends up to `count` samples and returns how many fit.
    size_t write(const float* data, size_t count)
    {
        const size_t write = writePos_.load(std::memory_order_relaxed);
        const size_t read = readPos_.load(std::memory_order_acquire);
        const size_t n = std::min(count, capacity() - (write - read));
        for (size_t i = 0; i < n; ++i) {
            buffer_[(write + i) & mask_] = data[i];
        }
        writePos_.store(write + n, std::memory_order_release);
        return n;
    }

    // Producer: appends up to `count` zero samples and returns how many fit.
    size_t writeSilence(size_t count)
    {
        const size_t write = writePos_.load(std::memory_order_relaxed);
        const size_t read = readPos_.load(std::memory_order_acquire);
        const size_t n = std::min(count, capacity() - (write - read));
        for (size_t i = 0; i < n; ++i) {
            buffer_[(write + i) & mask_] = 0.0f;
        }
        writePos_.store(write + n, std::memory_order_release);
        return n;
    }

    // Consumer: copies up to `count` samples without removing them.
    size_t peek(float* data, size_t count) const
    {
        const size_t read = readPos_.load(std::memory_order_relaxed);
        const size_t n = std::min(count, size());
        for (size_t i = 0; i < n; ++i) {
            data[i] = buffer_[(read + i) & mask_];
        }
        return n;
    }

    // Consumer: removes up to `count` samples and returns how many were removed.
    size_t skip(size_t count)
    {
        const size_t n = std::min(count, size());
        readPos_.store(readPos_.load(std::memory_order_relaxed) + n, std::memory_order_release);
        return n;
    }

    // Consumer: copies and removes up to `count` samples.
    size_t read(float* data, size_t count) { return skip(peek(data, count)); }

    // Consumer: empties the ring.
    void clear() { skip(size()); }

private:
    std::vector<float> buffer_;
    size_t mask_ = 0;
    alignas(64) std::atomic<size_t> writePos_{0};
    alignas(64) std::atomic<size_t> readPos_{0};
};

#pragma warning(pop)

}  // namespace gfx
