#pragma once

#include <atomic>
#include <memory>
#include <cstddef>

namespace LocklessQueue {
    template<typename T>
    class LockFreeQueue {
    private:
        struct Node {
            std::unique_ptr<T> data;
            std::atomic<Node*> next;

            Node() : next(nullptr) {}
            explicit Node(const T& value) : data(new T(value)), next(nullptr) {}
            explicit Node(T&& value) : data(new T(std::move(value))), next(nullptr) {}
        };

        std::atomic<Node*> m_head;
        std::atomic<Node*> m_tail;
        std::atomic<size_t> m_size;
        const size_t m_capacity;

    public:
        explicit LockFreeQueue(size_t capacity) : m_size(0), m_capacity(capacity) {
              Node* dummy = new Node();
              m_head.store(dummy, std::memory_order_relaxed);
              m_tail.store(dummy, std::memory_order_relaxed);
        }

        LockFreeQueue(const LockFreeQueue&) = delete;
        LockFreeQueue& operator=(const LockFreeQueue&) = delete;

        ~LockFreeQueue() {
            Node* node = m_head.load(std::memory_order_relaxed);
            while (node) {
                Node* next = node->next.load(std::memory_order_relaxed);
                delete node;
                node = next;
            }
        }

        bool push(const T& item) {
            if (m_size.load(std::memory_order_acquire) >= m_capacity) {
                return false;
            }

            Node* new_node = new Node(item);
            Node* tail;
            while (true) {
                tail = m_tail.load(std::memory_order_acquire);
                Node* next = tail->next.load(std::memory_order_acquire);
                if (next == nullptr) {
                    if (tail->next.compare_exchange_weak(next, new_node)) {
                        break;
                    }
                } else {
                    m_tail.compare_exchange_weak(tail, next);
                }
            }

            m_tail.compare_exchange_weak(tail, new_node);
            m_size.fetch_add(1, std::memory_order_release);
            return true;
        }

        bool pop(T& item) {
            Node* head;
            while (true) {
                head = m_head.load(std::memory_order_acquire);
                Node* next = head->next.load(std::memory_order_acquire);
                if (next == nullptr) {
                    return false;
                }

                if (m_head.compare_exchange_weak(head, next)) {
                    item = std::move(*next->data);
                    delete head;
                    m_size.fetch_sub(1, std::memory_order_release);
                    return true;
                }
            }
        }

        void clear() {
            T temp;
            while (pop(temp)) {}
        }

        bool empty() const {
            return m_size.load(std::memory_order_acquire) == 0;
        }

        bool full() const {
            return m_size.load(std::memory_order_acquire) >= m_capacity;
        }
    };
}
