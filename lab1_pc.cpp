#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <ctime>

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
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < M; ++j) {
            total_sum += matrix[i * M + j];
        }
    }
    return total_sum;
}

int main() {
    const size_t N = 10000;
    const size_t M = 10000;

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

    auto start_time = high_resolution_clock::now();

    long long result = sumSequential(matrix, N, M);

    auto end_time = high_resolution_clock::now();
    auto elapsed_time = duration_cast<milliseconds>(end_time - start_time).count();

    cout << endl << "sequential results:" << endl;
    cout << "sum: " << result << endl;
    cout << "time: " << elapsed_time << " ms" << endl;

    return 0;
}