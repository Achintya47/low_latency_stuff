#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <new>

constexpr std::uint64_t ITERATIONS = 50'000'000;

struct AdjacentCounters {
    std::uint64_t counter1 = 0;
    std::uint64_t counter2 = 0;
};

struct alignas(std::hardware_destructive_interference_size)
PaddedCounter {
    std::uint64_t value = 0;
};

struct PaddedCounters {
    PaddedCounter counter1;
    PaddedCounter counter2;
};

template <typename Counters>
double benchmark(Counters& counters, bool padded) {
    auto start = std::chrono::steady_clock::now();

    std::thread t1([&]() {
        for (std::uint64_t i = 0; i < ITERATIONS; ++i) {
            if (padded) {
                counters.counter1.value++;
            }
        }
    });

    std::thread t2([&]() {
        for (std::uint64_t i = 0; i < ITERATIONS; ++i) {
            if (padded) {
                counters.counter2.value++;
            }
        }
    });

    t1.join();
    t2.join();

    auto end = std::chrono::steady_clock::now();

    return std::chrono::duration<double>(end - start).count();
}

int main() {
    std::cout
        << "Destructive interference size: "
        << std::hardware_destructive_interference_size
        << " bytes\n";

    std::cout
        << "Padded counter size: "
        << sizeof(PaddedCounter)
        << " bytes\n";

    AdjacentCounters adjacent;

    auto start = std::chrono::steady_clock::now();

    std::thread a([&]() {
        for (std::uint64_t i = 0; i < ITERATIONS; ++i) {
            adjacent.counter1++;
        }
    });

    std::thread b([&]() {
        for (std::uint64_t i = 0; i < ITERATIONS; ++i) {
            adjacent.counter2++;
        }
    });

    a.join();
    b.join();

    auto end = std::chrono::steady_clock::now();

    double adjacentTime =
        std::chrono::duration<double>(end - start).count();

    PaddedCounters padded;

    start = std::chrono::steady_clock::now();

    std::thread c([&]() {
        for (std::uint64_t i = 0; i < ITERATIONS; ++i) {
            padded.counter1.value++;
        }
    });

    std::thread d([&]() {
        for (std::uint64_t i = 0; i < ITERATIONS; ++i) {
            padded.counter2.value++;
        }
    });

    c.join();
    d.join();

    end = std::chrono::steady_clock::now();

    double paddedTime =
        std::chrono::duration<double>(end - start).count();

    std::cout << "\nAdjacent counters:\n";
    std::cout << "  Size: "
              << sizeof(AdjacentCounters) << " bytes\n";
    std::cout << "  Time: "
              << adjacentTime << " seconds\n";

    std::cout << "\nPadded counters:\n";
    std::cout << "  Size: "
              << sizeof(PaddedCounters) << " bytes\n";
    std::cout << "  Time: "
              << paddedTime << " seconds\n";

    std::cout << "\nSpeedup: "
              << adjacentTime / paddedTime
              << "x\n";

    std::cout << "\nResults:\n";
    std::cout << adjacent.counter1 << " "
              << adjacent.counter2 << '\n';

    std::cout << padded.counter1.value << " "
              << padded.counter2.value << '\n';
}