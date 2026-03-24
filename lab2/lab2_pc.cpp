#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>

using namespace std;
using namespace std::chrono;

void solve_sequential(const vector<int>& arr, int& count, int& max_even) {
    for (size_t i = 0; i < arr.size(); ++i) {
        int val = arr[i];
        if (val % 2 == 0) {
            count++;
            if (val > max_even) {
                max_even = val;
            }
        }
    }
}

mutex mtx;

void worker_mutex(const vector<int>& arr, int start, int end, int& count, int& max_even) {
    for (int i = start; i < end; ++i) {
        int val = arr[i];
        if (val % 2 == 0) {
            lock_guard<mutex> lock(mtx);
            count++;
            if (val > max_even) {
                max_even = val;
            }
        }
    }
}

void solve_with_mutex(const vector<int>& arr, int num_threads, int& count, int& max_even) {
    vector<thread> threads;
    int chunk_size = arr.size() / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        int start = i * chunk_size;
        int end = (i == num_threads - 1) ? arr.size() : start + chunk_size;
        threads.emplace_back(worker_mutex, cref(arr), start, end, ref(count), ref(max_even));
    }

    for (auto& t : threads) {
        t.join();
    }
}

void worker_cas(const vector<int>& arr, int start, int end, atomic<int>& count, atomic<int>& max_even) {
    for (int i = start; i < end; ++i) {
        int val = arr[i];
        if (val % 2 == 0) {
            int expected_c = count.load();
            while (!count.compare_exchange_weak(expected_c, expected_c + 1)) {}

            int expected_m = max_even.load();
            while (val > expected_m && !max_even.compare_exchange_weak(expected_m, val)) {}
        }
    }
}

void solve_with_cas(const vector<int>& arr, int num_threads, atomic<int>& count, atomic<int>& max_even) {
    vector<thread> threads;
    int chunk_size = arr.size() / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        int start = i * chunk_size;
        int end = (i == num_threads - 1) ? arr.size() : start + chunk_size;
        threads.emplace_back(worker_cas, cref(arr), start, end, ref(count), ref(max_even));
    }

    for (auto& t : threads) {
        t.join();
    }
}

int main() {
    const vector<int> DATA_SIZES = { 1000000, 5000000, 10000000, 50000000, 100000000 };
    const int NUM_THREADS = 16;

    ofstream csv_file("results.csv");
    csv_file << "data_size,sequential_ms,mutex_ms,cas_ms\n";

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 1000000);

    for (int current_size : DATA_SIZES) {
        cout << "========================================\n";
        cout << "run test for DATA_SIZE: " << current_size << "\n";
        cout << "========================================\n";

        vector<int> arr(current_size);
        for (int i = 0; i < current_size; ++i) {
            arr[i] = dis(gen);
        }

        int count_seq = 0;
        int max_even_seq = -1;

        auto start_seq = high_resolution_clock::now();
        solve_sequential(arr, count_seq, max_even_seq);
        auto end_seq = high_resolution_clock::now();
        auto duration_seq = duration_cast<milliseconds>(end_seq - start_seq).count();

        cout << "--- seq result ---\n";
        cout << "even numbers found: " << count_seq << "\n";
        cout << "max even number: " << max_even_seq << "\n";
        cout << "exec time: " << duration_seq << " ms\n\n";

        int count_mtx = 0;
        int max_even_mtx = -1;

        auto start_mtx = high_resolution_clock::now();
        solve_with_mutex(arr, NUM_THREADS, count_mtx, max_even_mtx);
        auto end_mtx = high_resolution_clock::now();
        auto duration_mtx = duration_cast<milliseconds>(end_mtx - start_mtx).count();

        cout << "--- mtx result (" << NUM_THREADS << " threads) ---\n";
        cout << "even numbers found: " << count_mtx << "\n";
        cout << "max even number: " << max_even_mtx << "\n";
        cout << "exec time: " << duration_mtx << " ms\n\n";

        atomic<int> count_cas(0);
        atomic<int> max_even_cas(-1);

        auto start_cas = high_resolution_clock::now();
        solve_with_cas(arr, NUM_THREADS, count_cas, max_even_cas);
        auto end_cas = high_resolution_clock::now();
        auto duration_cas = duration_cast<milliseconds>(end_cas - start_cas).count();

        cout << "--- CAS res (" << NUM_THREADS << " threads) ---\n";
        cout << "even numbers found: " << count_cas.load() << "\n";
        cout << "max even number: " << max_even_cas.load() << "\n";
        cout << "exec time: " << duration_cas << " ms\n\n";

        csv_file << current_size << "," << duration_seq << "," << duration_mtx << "," << duration_cas << "\n";
    }

    csv_file.close();

    return 0;
}