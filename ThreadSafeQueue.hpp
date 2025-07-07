#pragma once // Impede que este arquivo seja incluído múltiplas vezes

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> shutdown_{false};
    size_t max_size_;

public:
    ThreadSafeQueue(size_t max_size = 0) : max_size_(max_size) {}

    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) return;
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            queue_.pop(); 
        }
        queue_.push(std::move(item));
        cond_.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]() { return !queue_.empty() || shutdown_; });
        if (shutdown_ && queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty() || shutdown_) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cond_.notify_all();
    }
};