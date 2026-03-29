#include "thread_pool.h"
#include <iostream>
#include <random>
#include <chrono>
#include <mutex>

using namespace std;
using namespace std::chrono;

int completed_tasks = 0;
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
    cout << "task " << task_id << " done (" << execute_time << "s)\n";
}

int main()
{
    thread_pool pool;
    pool.initialize(4);

    cout << "pool started (4 threads)\n";
    cout << "adding tasks...\n";

    for (int i = 1; i <= 15; ++i)
    {
        pool.add_task([i]() { execute_mock_task(i); });

        lock_guard<mutex> lock(console_mutex);
        cout << "+ task " << i << "\n";

        this_thread::sleep_for(milliseconds(500));
    }

    cout << "\nall tasks queued. waiting 45s for swap...\n\n";

    this_thread::sleep_for(seconds(60));

    pool.terminate();

    cout << "\ndone. tasks completed: " << completed_tasks << "\n";

    return 0;
}