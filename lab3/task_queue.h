#pragma once

#include <vector>
#include <shared_mutex>
#include <mutex>

using read_write_lock = std::shared_mutex;
using read_lock = std::shared_lock<read_write_lock>;
using write_lock = std::unique_lock<read_write_lock>;

template <typename task_type_t>
class task_queue
{
public:
    inline task_queue() = default;
    inline ~task_queue() { clear(); }

    inline bool empty() const;
    inline size_t size() const;

public:
    inline void clear();
    inline void push(task_type_t task);
    inline void swap_with(std::vector<task_type_t>& external_buffer);

public:
    task_queue(const task_queue& other) = delete;
    task_queue(task_queue&& other) = delete;
    task_queue& operator=(const task_queue& rhs) = delete;
    task_queue& operator=(task_queue&& rhs) = delete;

private:
    mutable read_write_lock m_rw_lock;
    std::vector<task_type_t> m_tasks;
};

template <typename task_type_t>
inline bool task_queue<task_type_t>::empty() const
{
    read_lock _(m_rw_lock);
    return m_tasks.empty();
}

template <typename task_type_t>
inline size_t task_queue<task_type_t>::size() const
{
    read_lock _(m_rw_lock);
    return m_tasks.size();
}

template <typename task_type_t>
inline void task_queue<task_type_t>::clear()
{
    write_lock _(m_rw_lock);
    m_tasks.clear();
}

template <typename task_type_t>
inline void task_queue<task_type_t>::push(task_type_t task)
{
    write_lock _(m_rw_lock);
    m_tasks.push_back(task);
}

template <typename task_type_t>
inline void task_queue<task_type_t>::swap_with(std::vector<task_type_t>& external_buffer)
{
    write_lock _(m_rw_lock);
    m_tasks.swap(external_buffer);
}