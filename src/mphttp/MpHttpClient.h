#ifndef MPHTTPCLIENT_H
#define MPHTTPCLIENT_H

#include <queue>

#include "HttpClient.h"
#include "MpTask.h"
#include "common.h"
#include "helper.hpp"

// forward declaration
struct EventLoop;
struct HttpClient;

struct MpHttpClient {
    MpHttpClient(struct EventLoop *loop, struct sockaddr_in *addr_in)
        : loop_(loop), tasks_({nullptr, nullptr}), addr_in_(*addr_in) {}

    void SetMpTask(struct MpTask *task) { mp_task_ = task; }

    bool IsComplete() {
        if (tasks_.running == nullptr && tasks_.next == nullptr) {
            return true;
        } else {
            UpdateTask();
            if (tasks_.running != nullptr && tasks_.running->is_close) {
                MPHTTP_LOG(debug, "holly shit\n");
                return tasks_.next == nullptr;
            }
            return false;
        }
    }

    void UpdateBandwidth(size_t bw) {
        if (bw > 0) {
            if (bandwidth_ == 0) {
                bandwidth_ = bw;
            } else {
                bandwidth_ = 0.85 * bandwidth_ + 0.15 * bw;
            }
        }
    }

    void UpdateTask() {
        if (!tasks_.running) {
            tasks_.running = tasks_.next;
            tasks_.next = nullptr;
        }
    }

    void UpdateRTT(timestamp_t send_time, timestamp_t recv_time) {
        size_t sample_rtt =
            std::chrono::duration_cast<std::chrono::milliseconds>(recv_time -
                                                                  send_time)
                .count();
        if (rtt_ == 0) {
            rtt_ = sample_rtt;
        } else {
            rtt_ = 0.9 * rtt_ + 0.1 * sample_rtt;
        }
    }

    size_t GetBandwidth() { return bandwidth_; }

    size_t GetRTT() const { return rtt_; }

    size_t GetRemaining() {
        if (tasks_.running) {
            return tasks_.running->GetRemaining();
        } else if (tasks_.next) {
            return tasks_.next->GetRemaining();
        } else {
            return 0;
        }
    }

    void run(size_t start, size_t end);

    // @brief : get another task
    struct HttpClient *getAnotherTask(struct HttpClient *client) {
        if (client == tasks_.running) {
            return tasks_.next;
        } else if (client == tasks_.next) {
            return tasks_.running;
        } else {
            MPHTTP_FATAL("MpHttpClient getAnotherTask fail.");
            return nullptr;
        }
    }

    // @brief : reschedule the task when almost complete.
    void reschedule();

    // @brief : reset Task queue with the client pointer, need to call update
    // after reset
    void resetTaskQueue(struct HttpClient *client) {
        if (tasks_.running == client) {
            tasks_.running = nullptr;
        } else if (tasks_.next == client) {
            tasks_.next = nullptr;
        }
        UpdateTask();
    }

    Request *request_header;
    std::string *request_body;

    struct sockaddr_in addr_in_;

    // current bandwidth
    size_t bandwidth_{0};
    // current rtt
    size_t rtt_{0};
    // the main event loop
    struct EventLoop *loop_;
    // the mp task
    struct MpTask *mp_task_;
    // the rival
    struct MpHttpClient *rival_;
    // the task queue of MpHttpClient
    struct Tasks {
        struct HttpClient *running;
        struct HttpClient *next;
    };
    struct Tasks tasks_;
};

#endif  // ! MPHTTPCLIENT_H