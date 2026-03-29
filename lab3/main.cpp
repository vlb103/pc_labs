#include "thread_pool.h"
#include <iostream>
#include <random>
#include <chrono>
#include <mutex>
#include <vector>
#include <iomanip>

using namespace std;
using namespace std::chrono;

int completed_tasks = 0;
int total_execution_time = 0;
mutex console_mutex;

void execute_mock_task(int task_id)
{
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(4, 10);

    int execute_time = dist(gen);
    this_thread::sleep_for(seconds(execute_time));

    lock_guard<mutex> lock(console_mutex);
    completed_tasks++;
    total_execution_time += execute_time;
    cout << "task " << task_id << " done (" << execute_time << "s)\n";
}

void task_generator(thread_pool& pool, int generator_id, time_point<steady_clock> deadline)
{
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(3000, 6000);

    int i = 1;
    while (steady_clock::now() < deadline)
    {
        int task_id = generator_id * 1000 + i++;

        pool.add_task([task_id]() { execute_mock_task(task_id); });

        {
            lock_guard<mutex> lock(console_mutex);
            cout << "[gen " << generator_id << "] + task " << task_id << "\n";
        }

        this_thread::sleep_for(milliseconds(dist(gen)));
    }
}

int main()
{
    thread_pool pool;
    pool.initialize(4);

    cout << "pool started (4 threads)\n";
    cout << "starting 3 task generators. test duration: 50 seconds...\n";

    auto test_start_time = steady_clock::now();
    auto deadline = test_start_time + seconds(50);

    vector<thread> generators;
    for (int i = 1; i <= 3; ++i)
    {
        generators.emplace_back(task_generator, ref(pool), i, deadline);
    }

    for (auto& gen : generators)
    {
        if (gen.joinable()) gen.join();
    }

    cout << "\nall generators finished. terminating pool and waiting for pending tasks...\n\n";

    pool.terminate(true);

    auto test_end_time = steady_clock::now();

    cout << "\n--- statistics ---\n";

    double total_test_seconds = duration<double>(test_end_time - test_start_time).count();
    cout << "total test execution time: " << fixed << setprecision(2) << total_test_seconds << " s\n";

    cout << "threads created: 4\n";

    long long wait_time = pool.get_total_wait_time_ms();
    long long wait_count = pool.get_wait_count();
    double avg_wait = wait_count > 0 ? (double)wait_time / wait_count : 0.0;
    cout << "avg thread wait time: " << fixed << setprecision(2) << avg_wait << " ms\n";

    cout << "avg pending queue length: " << pool.get_avg_pending_size() << " tasks\n";
    cout << "avg active queue length: " << pool.get_avg_active_size() << " tasks\n";

    double avg_exec = completed_tasks > 0 ? (double)total_execution_time / completed_tasks : 0.0;
    cout << "avg task execution time: " << avg_exec << " s\n";
    cout << "total tasks completed: " << completed_tasks << "\n";

    return 0;
}