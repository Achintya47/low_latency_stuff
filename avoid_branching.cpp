#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

enum class Side : int16_t {
    BUY = 1,
    SELL = -1
};

struct Fill {
    Side side;
    int qty;
};

struct Result {
    int64_t position;
    int64_t last_buy_qty;
    int64_t last_sell_qty;
};

Result run_branching(const std::vector<Fill>& fills) {
    int64_t last_buy_qty = 0;
    int64_t last_sell_qty = 0;
    int64_t position = 0;

    for (const auto& fill : fills) {
        if (fill.side == Side::BUY) {
            position += fill.qty;
            last_buy_qty = fill.qty;
        } else if (fill.side == Side::SELL) {
            position -= fill.qty;
            last_sell_qty = fill.qty;
        }
    }

    return {position, last_buy_qty, last_sell_qty};
}

Result run_branchless(const std::vector<Fill>& fills) {
    int64_t last_qty[3] = {0, 0, 0};
    int64_t position = 0;

    for (const auto& fill : fills) {
        const int side = static_cast<int>(fill.side);

        position += static_cast<int64_t>(side) * fill.qty;
        last_qty[side + 1] = fill.qty;
    }

    return {position, last_qty[2], last_qty[0]};
}

volatile int64_t benchmark_sink = 0;

template <typename Function>
double benchmark_ms(Function function, int repetitions) {
    // Warm up caches and code paths.
    benchmark_sink = function().position;

    std::vector<double> times;
    times.reserve(repetitions);

    for (int i = 0; i < repetitions; ++i) {
        const auto start = std::chrono::steady_clock::now();

        const Result result = function();

        const auto end = std::chrono::steady_clock::now();

        // Make the computed result observable.
        benchmark_sink =
            result.position +
            result.last_buy_qty +
            result.last_sell_qty;

        times.push_back(
            std::chrono::duration<double, std::milli>(
                end - start
            ).count()
        );
    }

    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}

void run_test(
    const std::string& workload,
    const std::vector<Fill>& fills
) {
    constexpr int REPETITIONS = 9;

    // Verify that both implementations produce identical results.
    const Result a = run_branching(fills);
    const Result b = run_branchless(fills);

    if (a.position != b.position ||
        a.last_buy_qty != b.last_buy_qty ||
        a.last_sell_qty != b.last_sell_qty) {
        std::cerr << "ERROR: Results do not match!\n";
        return;
    }

    // Alternate benchmark order to reduce systematic ordering bias.
    double branching_ms = 0.0;
    double branchless_ms = 0.0;

    for (int i = 0; i < REPETITIONS; ++i) {
        if (i % 2 == 0) {
            branching_ms += benchmark_ms(
                [&] { return run_branching(fills); }, 1
            );

            branchless_ms += benchmark_ms(
                [&] { return run_branchless(fills); }, 1
            );
        } else {
            branchless_ms += benchmark_ms(
                [&] { return run_branchless(fills); }, 1
            );

            branching_ms += benchmark_ms(
                [&] { return run_branching(fills); }, 1
            );
        }
    }

    branching_ms /= REPETITIONS;
    branchless_ms /= REPETITIONS;

    const double count_millions =
        static_cast<double>(fills.size()) / 1'000'000.0;

    const double branching_mops =
        fills.size() / (branching_ms * 1000.0);

    const double branchless_mops =
        fills.size() / (branchless_ms * 1000.0);

    std::cout << "\nWorkload: " << workload << '\n';
    std::cout << "Fills: " << fills.size() << '\n';

    std::cout << std::fixed << std::setprecision(3);

    std::cout << "Branching:  "
              << branching_ms << " ms, "
              << branching_mops << " million fills/sec\n";

    std::cout << "Branchless: "
              << branchless_ms << " ms, "
              << branchless_mops << " million fills/sec\n";

    if (branchless_ms < branching_ms) {
        std::cout << "Branchless faster by "
                  << (branching_ms / branchless_ms - 1.0) * 100.0
                  << "%\n";
    } else {
        std::cout << "Branching faster by "
                  << (branchless_ms / branching_ms - 1.0) * 100.0
                  << "%\n";
    }

    std::cout << "Final position: " << a.position << '\n';
}

int main() {
    constexpr std::size_t N = 20'000'000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> qty_dist(1, 100);

    std::vector<Fill> predictable;
    std::vector<Fill> random_mixed;

    predictable.reserve(N);
    random_mixed.reserve(N);

    // Workload 1: 90% BUY, 10% SELL.
    // This gives the branch predictor a relatively predictable pattern.
    for (std::size_t i = 0; i < N; ++i) {
        const Side side = (i % 10 == 0)
            ? Side::SELL
            : Side::BUY;

        predictable.push_back({side, qty_dist(rng)});
    }

    // Workload 2: Random BUY/SELL directions.
    // This can be harder for a branch predictor.
    std::bernoulli_distribution side_dist(0.5);

    for (std::size_t i = 0; i < N; ++i) {
        const Side side = side_dist(rng)
            ? Side::BUY
            : Side::SELL;

        random_mixed.push_back({side, qty_dist(rng)});
    }

    std::cout << "Benchmarking 20 million fills per workload...\n";

    run_test("90% BUY / 10% SELL", predictable);
    run_test("50% random BUY / 50% random SELL", random_mixed);

    return 0;
}

