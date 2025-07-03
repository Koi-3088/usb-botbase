#pragma once

#include <atomic>
#include <memory>
#include <cstddef>

namespace LocklessQueue {
    template<typename T>
    class LockFreeQueue {
    public:
        explicit LockFreeQueue(size_t capacity)
            : m_capacity(capacity + 1),
              m_buffer(new T[m_capacity]),
              m_head(0),
              m_tail(0) {}

        LockFreeQueue(const LockFreeQueue&) = delete;
        LockFreeQueue& operator=(const LockFreeQueue&) = delete;

        bool push(const T& item) {
            size_t tail = m_tail.load(std::memory_order_relaxed);
            size_t next_tail = increment(tail);
            if (next_tail == m_head.load(std::memory_order_acquire)) {
                return false;
            }

            m_buffer[tail] = item;
            m_tail.store(next_tail, std::memory_order_release);
            return true;
        }

        bool push_front(const T& item) {
            size_t head = m_head.load(std::memory_order_relaxed);
            size_t prev_head = decrement(head);
            if (m_tail.load(std::memory_order_acquire) == prev_head) {
                return false;
            }

            m_buffer[prev_head] = item;
            m_head.store(prev_head, std::memory_order_release);
            return true;
        }

        bool pop(T& item) {
            size_t head = m_head.load(std::memory_order_relaxed);
            if (head == m_tail.load(std::memory_order_acquire)) {
                return false;
            }

            item = std::move(m_buffer[head]);
            m_head.store(increment(head), std::memory_order_release);
            return true;
        }

        void clear() {
            m_head.store(0, std::memory_order_release);
            m_tail.store(0, std::memory_order_release);
        }

        bool empty() const {
            return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
        }

        bool full() const {
            return increment(m_tail.load(std::memory_order_acquire)) == m_head.load(std::memory_order_acquire);
        }

    private:
        size_t increment(size_t idx) const { return (idx + 1) % m_capacity; }
        size_t decrement(size_t idx) const { return (idx + m_capacity - 1) % m_capacity; }

        const size_t m_capacity;
        std::unique_ptr<T[]> m_buffer;
        std::atomic<size_t> m_head;
        std::atomic<size_t> m_tail;
    };
}