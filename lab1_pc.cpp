#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <thread>

using namespace std;
using namespace std::chrono;

void printMatrix(const vector<int>& matrix, size_t N, size_t M) {
    if (N > 15 || M > 15) {
        return;
    }

    cout << "matrix" << endl;
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < M; ++j) {
            cout << matrix[i * M + j] << "\t";
        }
        cout << endl;
    }
    cout << endl;
}

long long sumSequential(const vector<int>& matrix, size_t N, size_t M) {
    long long total_sum = 0;
    size_t total_elements = N * M;

    for (size_t k = 0; k < total_elements; ++k) {
        total_sum += matrix[k];
    }

    return total_sum;
}

void sumRange(const vector<int>& matrix, size_t start_idx, size_t end_idx, long long& result_ref) {
    long long local_sum = 0;

    for (size_t i = start_idx; i < end_idx; ++i) {
        local_sum += matrix[i];
    }

    result_ref = local_sum;
}

long long sumParallel(const vector<int>& matrix, size_t N, size_t M, int num_threads) {
    size_t total_elements = N * M;

    vector<long long> partial_sums(num_threads, 0);
    vector<thread> threads;

    size_t chunk_size = total_elements / num_threads;
    size_t remainder = total_elements % num_threads;

    size_t current_start = 0;

    for (int i = 0; i < num_threads; ++i) {
        size_t current_end = current_start + chunk_size + (i < remainder ? 1 : 0);

        threads.emplace_back(sumRange, cref(matrix), current_start, current_end, ref(partial_sums[i]));

        current_start = current_end;
    }

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    long long total_sum = 0;
    for (size_t i = 0; i < partial_sums.size(); ++i) {
        total_sum += partial_sums[i];
    }

    return total_sum;
}

int main() {
    const size_t N = 10000;
    const size_t M = 10000;

    int num_threads = 16;

    cout << "creating matrix " << N << "x" << M << endl;

    vector<int> matrix(N * M);

    srand(static_cast<unsigned int>(time(nullptr)));

    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < M; ++j) {
            matrix[i * M + j] = rand() % 10 + 1;
        }
    }

    printMatrix(matrix, N, M);

    volatile long long warmup_result = sumSequential(matrix, N, M);

    auto start_time_seq = high_resolution_clock::now();
    long long result_seq = sumSequential(matrix, N, M);
    auto end_time_seq = high_resolution_clock::now();
    auto elapsed_seq = duration_cast<milliseconds>(end_time_seq - start_time_seq).count();

    auto start_time_par = high_resolution_clock::now();
    long long result_par = sumParallel(matrix, N, M, num_threads);
    auto end_time_par = high_resolution_clock::now();
    auto elapsed_par = duration_cast<milliseconds>(end_time_par - start_time_par).count();

    cout << endl << "sequential results:" << endl;
    cout << "sum: " << result_seq << endl;
    cout << "time: " << elapsed_seq << " ms" << endl;

    cout << endl << "parallel results (" << num_threads << " threads):" << endl;
    cout << "sum: " << result_par << endl;
    cout << "time: " << elapsed_par << " ms" << endl << endl;

    if (result_seq == result_par) {
        cout << "status: success (sums match)" << endl;
    }
    else {
        cout << "status: error (sums do not match)" << endl;
    }

    return 0;
}