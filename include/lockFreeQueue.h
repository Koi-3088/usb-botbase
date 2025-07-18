#pragma once

#include <atomic>

namespace LocklessQueue {
    template<typename T, size_t Capacity = 128>
    class LockFreeQueue {
        static_assert(Capacity > 0, "Capacity must be greater than 0.");

    private:
        struct Cell {
            std::atomic<size_t> sequence;
            T data;
        };

        alignas(64) Cell m_buffer[Capacity];
        alignas(64) std::atomic<size_t> m_enqueuePos;
        alignas(64) std::atomic<size_t> m_dequeuePos;

    public:
        LockFreeQueue() : m_enqueuePos(0), m_dequeuePos(0) {
            for (size_t i = 0; i < Capacity; ++i) {
                m_buffer[i].sequence.store(i, std::memory_order_relaxed);
            }
        }

        LockFreeQueue(const LockFreeQueue&) = delete;
        LockFreeQueue& operator=(const LockFreeQueue&) = delete;

        bool push(const T& item) {
            return emplace(item);
        }

        bool push(T&& item) {
            return emplace(std::move(item));
        }

        template<typename U>
        bool emplace(U&& item) {
            size_t pos = m_enqueuePos.load(std::memory_order_relaxed);
            for (;;) {
                Cell& cell = m_buffer[pos % Capacity];
                size_t seq = cell.sequence.load(std::memory_order_acquire);
                intptr_t diff = (intptr_t)seq - (intptr_t)pos;
                if (diff == 0) {
                    if (m_enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        cell.data = std::forward<U>(item);
                        cell.sequence.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                } else if (diff < 0) {
                    return false;
                } else {
                    pos = m_enqueuePos.load(std::memory_order_relaxed);
                }
            }
        }

        bool pop(T& item) {
            size_t pos = m_dequeuePos.load(std::memory_order_relaxed);
            for (;;) {
                Cell& cell = m_buffer[pos % Capacity];
                size_t seq = cell.sequence.load(std::memory_order_acquire);
                intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
                if (diff == 0) {
                    if (m_dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        item = std::move(cell.data);
                        cell.sequence.store(pos + Capacity, std::memory_order_release);
                        return true;
                    }
                } else if (diff < 0) {
                    return false;
                } else {
                    pos = m_dequeuePos.load(std::memory_order_relaxed);
                }
            }
        }

        void clear() {
            T temp;
            while (pop(temp)) {}
        }

        bool empty() const {
            size_t pos = m_dequeuePos.load(std::memory_order_acquire);
            const Cell& cell = m_buffer[pos % Capacity];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            return ((intptr_t)seq - (intptr_t)(pos + 1)) < 0;
        }

        bool full() const {
            size_t pos = m_enqueuePos.load(std::memory_order_acquire);
            const Cell& cell = m_buffer[pos % Capacity];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            return ((intptr_t)seq - (intptr_t)pos) < 0;
        }
    };
}
