#pragma once

#include <vector>
#include <thread>
#include <functional>
#include <condition_variable>
#include "task_queue.h"

class thread_pool
{
public:
    inline thread_pool() = default;
    inline ~thread_pool() { terminate(); }

    void initialize(const size_t worker_count);
    void terminate();
    void routine();

    bool working() const;
    bool working_unsafe() const;

    void add_task(std::function<void()> task);
    void update_buffers();

public:
    thread_pool(const thread_pool& other) = delete;
    thread_pool(thread_pool&& other) = delete;
    thread_pool& operator=(const thread_pool& rhs) = delete;
    thread_pool& operator=(thread_pool&& rhs) = delete;

private:
    mutable read_write_lock m_rw_lock;
    mutable std::condition_variable_any m_task_waiter;

    std::vector<std::thread> m_workers;

    task_queue<std::function<void()>> m_pending_tasks;
    std::vector<std::function<void()>> m_active_tasks;

    bool m_initialized = false;
    bool m_terminated = false;

    std::thread m_timer_thread;
};