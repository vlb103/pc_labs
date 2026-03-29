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

                {
                    read_lock lock(m_rw_lock);
                    m_sum_pending_size += m_pending_tasks.size();
                    m_sum_active_size += m_active_tasks.size();
                    m_sample_count++;
                }
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

    std::vector<std::function<void()>> temp;
    m_pending_tasks.swap_with(temp);

    for (auto& t : temp) {
        m_active_tasks.push_back(std::move(t));
    }

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

            auto start_wait = high_resolution_clock::now();
            m_task_waiter.wait(_, [this] {
                return m_terminated || (!m_active_tasks.empty() && !m_paused);
                });
            auto end_wait = high_resolution_clock::now();

            m_total_wait_time_ms += duration_cast<milliseconds>(end_wait - start_wait).count();
            m_wait_count++;

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
            else {
                std::vector<std::function<void()>> temp;
                m_pending_tasks.swap_with(temp);
                for (auto& t : temp) {
                    m_active_tasks.push_back(std::move(t));
                }
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

long long thread_pool::get_total_wait_time_ms() const { read_lock _(m_rw_lock); return m_total_wait_time_ms; }
long long thread_pool::get_wait_count() const { read_lock _(m_rw_lock); return m_wait_count; }

double thread_pool::get_avg_pending_size() const {
    read_lock _(m_rw_lock);
    return m_sample_count > 0 ? (double)m_sum_pending_size / m_sample_count : 0.0;
}

double thread_pool::get_avg_active_size() const {
    read_lock _(m_rw_lock);
    return m_sample_count > 0 ? (double)m_sum_active_size / m_sample_count : 0.0;
}