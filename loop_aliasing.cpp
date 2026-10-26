#include <iostream>
#include <chrono>
#include <memory>


using Clock = std::chrono::high_resolution_clock;

constexpr int N = 100'000'000;

void foo(int* __restrict a, int* __restrict b, int n)
{
    for (int i = 0; i < n; ++i)
    {
        a[i] = *b;
    }
}

int main()
{
    auto a = std::make_unique<int[]>(N);

    int value = 42;

    auto start = Clock::now();

    foo(a.get(), &value, N);

    auto end = Clock::now();

    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Time: " << elapsed.count() << " s\n";

    // Prevent optimization away
    long long sum = 0;
    for (int i = 0; i < N; i += 1000)
        sum += a[i];

    std::cout << "Checksum: " << sum << '\n';
}