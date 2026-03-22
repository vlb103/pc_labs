#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <limits>

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

int main() {
    const int DATA_SIZE = 10000000;

    vector<int> arr(DATA_SIZE);

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 1000000);

    for (int i = 0; i < DATA_SIZE; ++i) {
        arr[i] = dis(gen);
    }

    int count = 0;
    int max_even = numeric_limits<int>::min();

    auto start_seq = high_resolution_clock::now();
    solve_sequential(arr, count, max_even);
    auto end_seq = high_resolution_clock::now();
    auto duration_seq = duration_cast<milliseconds>(end_seq - start_seq).count();

    cout << "--- Sequential Search Results ---\n";
    cout << "Total even numbers found: " << count << "\n";
    cout << "Max even number: " << max_even << "\n";
    cout << "Execution time: " << duration_seq << " ms\n";

    return 0;
}