#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

using Clock = std::chrono::steady_clock;

constexpr std::size_t HOT_SIZE = 1024;
constexpr std::size_t COLD_SIZE = 8 * 1024 * 1024 * 1024;

alignas(64) std::uint64_t hot[HOT_SIZE];
std::vector<std::uint64_t> cold(COLD_SIZE);

volatile std::uint64_t result = 0;

void critical_function()
{
    std::uint64_t sum = 0;

    for (std::size_t i = 0; i < HOT_SIZE; ++i)
        sum += hot[i];

    result = sum;
}

void bulk_processing()
{
    std::uint64_t sum = 0;

    for (std::size_t i = 0; i < COLD_SIZE; i += 8)
        sum += cold[i];

    result = sum;
}

template <typename Function>
double benchmark(Function function, int repetitions)
{
    auto start = Clock::now();

    for (int i = 0; i < repetitions; ++i)
        function();

    auto end = Clock::now();

    return std::chrono::duration<double, std::nano>(
        end - start
    ).count() / repetitions;
}

int main()
{
    for (auto& x : hot)
        x = 1;

    for (auto& x : cold)
        x = 1;

    // Warm up the critical function.
    for (int i = 0; i < 1000; ++i)
        critical_function();

    // Baseline latency.
    double baseline = benchmark(critical_function, 10000);

    // Run the cache-disrupting workload.
    bulk_processing();

    // Latency immediately after that workload.
    double after = benchmark(critical_function, 10000);

    std::cout << "Baseline: " << baseline << " ns\n";
    std::cout << "After:    " << after << " ns\n";
}
