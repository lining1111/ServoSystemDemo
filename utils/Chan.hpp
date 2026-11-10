//
// Created by lining on 2026/4/10.
//

#ifndef CHAN_H
#define CHAN_H

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <variant>
#include <thread>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <utility>

/**
 * 仿 Go 语言的 channel 实现
 * @tparam T 通道中传输的数据类型
 */
template<typename T>
class Chan {
public:
    /**
     * 构造 channel
     * @param capacity 缓冲区大小，0 表示无缓冲（同步），>0 表示有缓冲
     * @param name 通道名称，用于监控打印标识
     */
    explicit Chan(size_t capacity = 0, std::string name = "chan")
        : capacity_(capacity), closed_(false), name_(std::move(name)),
          storage_(capacity > 0
                       ? std::variant<QueueStorage, SlotStorage>(std::in_place_type<QueueStorage>, capacity)
                       : std::variant<QueueStorage, SlotStorage>(std::in_place_type<SlotStorage>)) {
    }

    ~Chan() {
        //析构时停止监控线程，避免线程悬空
        SetMonitor(false);
    }

    // 禁止拷贝和赋值
    Chan(const Chan &) = delete;

    Chan &operator=(const Chan &) = delete;

    /**
     * 发送数据到 channel
     * @param value 要发送的值
     * @return true 表示发送成功，false 表示 channel 已关闭
     */
    bool send(T value) {
        std::unique_lock<std::mutex> lock(mtx_);
        // 等待条件：未关闭且（有缓冲时有空位，或无缓冲时槽为空）
        cv_.wait(lock, [this] {
            if (closed_) return true;
            if (capacity_ > 0) {
                auto &qs = std::get<QueueStorage>(storage_);
                return qs.queue.size() < qs.capacity;
            } else {
                auto &ss = std::get<SlotStorage>(storage_);
                return !ss.slot.has_value();
            }
        });

        if (closed_) {
            // 向已关闭的 channel 发送数据，按 Go 语义应 panic，这里返回 false 表示失败
            return false;
        }

        if (capacity_ > 0) {
            auto &qs = std::get<QueueStorage>(storage_);
            qs.queue.push(std::move(value));
        } else {
            auto &ss = std::get<SlotStorage>(storage_);
            ss.slot = std::move(value);
        }
        cv_.notify_one(); // 唤醒一个等待的接收者
        return true;
    }

    /**
     * 从 channel 接收数据
     * @param out 用于存储接收到的值
     * @return true 表示成功接收到数据，false 表示 channel 已关闭且无数据
     */
    bool receive(T &out) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] {
            if (closed_) return true;
            if (capacity_ > 0) {
                auto &qs = std::get<QueueStorage>(storage_);
                return !qs.queue.empty();
            } else {
                auto &ss = std::get<SlotStorage>(storage_);
                return ss.slot.has_value();
            }
        });

        if (closed_) {
            // 如果 channel 已关闭，先尝试取出剩余数据
            if (capacity_ > 0) {
                auto &qs = std::get<QueueStorage>(storage_);
                if (!qs.queue.empty()) {
                    out = std::move(qs.queue.front());
                    qs.queue.pop();
                    cv_.notify_one(); // 唤醒可能等待的发送者
                    return true;
                }
            } else {
                auto &ss = std::get<SlotStorage>(storage_);
                if (ss.slot.has_value()) {
                    out = std::move(*ss.slot);
                    ss.slot = std::nullopt;
                    cv_.notify_one();
                    return true;
                }
            }
            return false;
        }

        // 正常接收
        if (capacity_ > 0) {
            auto &qs = std::get<QueueStorage>(storage_);
            out = std::move(qs.queue.front());
            qs.queue.pop();
        } else {
            auto &ss = std::get<SlotStorage>(storage_);
            out = std::move(*ss.slot);
            ss.slot = std::nullopt;
        }
        cv_.notify_one(); // 唤醒可能等待的发送者
        return true;
    }

    /**
     * 关闭 channel
     * 关闭后不能再发送，但可以继续接收剩余数据
     */
    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (closed_) return;
        closed_ = true;
        cv_.notify_all(); // 唤醒所有等待的线程
    }

    /** 判断 channel 是否已关闭 */
    bool is_closed() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return closed_;
    }

    /** 当前通道内的元素个数（无缓冲通道为 0 或 1） */
    size_t size() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (capacity_ > 0) {
            return std::get<QueueStorage>(storage_).queue.size();
        }
        return std::get<SlotStorage>(storage_).slot.has_value() ? 1 : 0;
    }

    /**
     * 开启/关闭容量监控
     * 开启后启动后台线程，周期性检查通道内元素个数，发生变化时以覆盖方式打印到控制台：
     *   [chanName] 3/10
     * @param enable true 开启 false 关闭
     * @param intervalMs 检查间隔，默认 500ms
     */
    void SetMonitor(bool enable, int intervalMs = 500) {
        std::lock_guard<std::mutex> lock(monitorMtx_);
        if (enable == monitorEnabled_) {
            return;
        }
        monitorEnabled_ = enable;
        if (enable) {
            monitorThread_ = std::thread([this, intervalMs] {
                MonitorLoop(intervalMs);
            });
        } else {
            if (monitorThread_.joinable()) {
                monitorThread_.join();
            }
        }
    }

    /** 通道名称 */
    std::string name() const {
        return name_;
    }

private:
    //监控循环：元素个数变化时覆盖打印
    void MonitorLoop(int intervalMs) {
        size_t lastSize = SIZE_MAX;
        while (monitorEnabled_) {
            size_t cur = size();
            if (cur != lastSize) {
                //\r 回到行首，\033[K 清除到行尾，实现同行覆盖打印
                std::cout << "\r\033[K[" << name_ << "] " << cur << "/" << capacity_ << std::flush;
                lastSize = cur;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    }

    size_t capacity_; // 缓冲区大小，0 表示无缓冲
    bool closed_; // 关闭标志
    std::string name_; // 通道名称
    mutable std::mutex mtx_; // 互斥锁
    std::condition_variable cv_; // 条件变量
    std::mutex monitorMtx_; // 监控线程启停互斥锁
    std::atomic<bool> monitorEnabled_{false}; // 监控开关
    std::thread monitorThread_; // 监控线程

    // 有缓冲时的存储结构
    struct QueueStorage {
        std::queue<T> queue;
        size_t capacity;

        QueueStorage(size_t cap) : capacity(cap) {
        }
    };

    // 无缓冲时的存储结构（单个槽）
    struct SlotStorage {
        std::optional<T> slot;
    };

    std::variant<QueueStorage, SlotStorage> storage_;
};


class Uint8Channel {
public:
    explicit Uint8Channel(size_t capacity)
        : capacity_(capacity), closed_(false) {}

    // 禁止拷贝
    Uint8Channel(const Uint8Channel&) = delete;
    Uint8Channel& operator=(const Uint8Channel&) = delete;

    // ---------- 批量发送 ----------
    // 从 vector 发送
    bool send_batch(const std::vector<uint8_t>& data) {
        return send_batch(data.data(), data.size());
    }

    // 从原始数组发送，返回 true 表示全部成功发送，false 表示通道已关闭
    bool send_batch(const uint8_t* data, size_t len) {
        if (len == 0) return true;
        std::unique_lock<std::mutex> lock(mtx_);
        // 等待有足够空间容纳整个批次，或者通道关闭
        cv_.wait(lock, [this, len] {
            return closed_ || (buffer_.size() + len <= capacity_);
        });
        if (closed_) return false;
        // 批量存入
        for (size_t i = 0; i < len; ++i) {
            buffer_.push_back(data[i]);
        }
        // 只通知一次消费者
        cv_.notify_one();
        return true;
    }

    // ---------- 批量接收 ----------
    // 将数据写入用户提供的缓冲区，返回实际读取的字节数
    // 若通道关闭且无数据则返回 0
    size_t receive_batch(uint8_t* out, size_t max_len) {
        if (max_len == 0) return 0;
        std::unique_lock<std::mutex> lock(mtx_);
        // 等待至少有一个数据，或通道关闭
        cv_.wait(lock, [this] {
            return closed_ || !buffer_.empty();
        });
        if (buffer_.empty()) return 0;  // 关闭且无数据
        // 批量取出（最多 max_len 个）
        size_t to_read = std::min(max_len, buffer_.size());
        for (size_t i = 0; i < to_read; ++i) {
            out[i] = buffer_.front();
            buffer_.pop_front();
        }
        // 只通知一次生产者
        cv_.notify_one();
        return to_read;
    }

    // 返回 vector 的便捷版本
    std::vector<uint8_t> receive_batch(size_t max_len) {
        std::vector<uint8_t> result(max_len);
        size_t n = receive_batch(result.data(), max_len);
        result.resize(n);
        return result;
    }

    // 关闭通道
    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_.notify_all();   // 唤醒所有等待的线程
    }

    bool is_closed() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return closed_;
    }

private:
    size_t capacity_;
    bool closed_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<uint8_t> buffer_;   // 可换成环形缓冲区以提高性能
};


#endif //CHAN_H
