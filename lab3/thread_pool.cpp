#include "thread_pool.h"
#include <chrono>

using namespace std;
using namespace std::chrono;

bool thread_pool::working() const
{
    read_lock _(m_rw_lock);
    return working_unsafe();
}

bool thread_pool::working_unsafe() const
{
    return m_initialized && !m_terminated;
}

void thread_pool::initialize(const size_t worker_count)
{
    write_lock _(m_rw_lock);
    if (m_initialized || m_terminated) return;

    m_workers.reserve(worker_count);
    for (size_t id = 0; id < worker_count; id++)
    {
        m_workers.emplace_back(&thread_pool::routine, this);
    }

    m_timer_thread = thread([this]() {
        while (true) {
            for (int i = 0; i < 450; ++i) {
                this_thread::sleep_for(milliseconds(100));
                if (!working()) return;
            }
            update_buffers();
        }
        });

    m_initialized = !m_workers.empty();
}

void thread_pool::pause()
{
    write_lock _(m_rw_lock);
    m_paused = true;
}

void thread_pool::resume()
{
    write_lock _(m_rw_lock);
    m_paused = false;
    m_task_waiter.notify_all();
}

void thread_pool::update_buffers()
{
    write_lock _(m_rw_lock);
    m_pending_tasks.swap_with(m_active_tasks);
    if (!m_paused) {
        m_task_waiter.notify_all();
    }
}

void thread_pool::routine()
{
    while (true)
    {
        function<void()> task;
        {
            write_lock _(m_rw_lock);
            m_task_waiter.wait(_, [this] {
                return m_terminated || (!m_active_tasks.empty() && !m_paused);
                });

            if (m_terminated && m_active_tasks.empty()) return;

            if (m_active_tasks.empty()) continue;

            task = move(m_active_tasks.front());
            m_active_tasks.erase(m_active_tasks.begin());
        }

        if (task) task();
    }
}

void thread_pool::add_task(function<void()> task)
{
    {
        read_lock _(m_rw_lock);
        if (!working_unsafe()) return;
    }
    m_pending_tasks.push(task);
}

void thread_pool::terminate(bool wait)
{
    {
        write_lock _(m_rw_lock);
        if (working_unsafe()) {
            m_terminated = true;
            if (!wait) {
                m_pending_tasks.clear();
                m_active_tasks.clear();
            }
        }
        else {
            return;
        }
    }

    m_task_waiter.notify_all();

    for (thread& worker : m_workers)
    {
        if (worker.joinable()) worker.join();
    }

    if (m_timer_thread.joinable()) m_timer_thread.join();

    m_workers.clear();
    m_terminated = false;
    m_initialized = false;
    m_paused = false;
}