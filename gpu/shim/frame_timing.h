// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace BbTiming {
struct Summary {
    double mean{}, p99{}, maximum{}, deviation{};
    std::size_t count{};
};
// A bounded history of unsmoothed intervals; callers provide synchronization.
template <std::size_t Capacity = 240>
class History {
public:
    void Reset() { size = next = 0; previous = -1; }
    double Mark(double milliseconds) {
        const double delta = previous < 0 ? 0 : milliseconds - previous;
        previous = milliseconds;
        if (delta > 0 && std::isfinite(delta)) {
            values[next] = float(delta);
            next = (next + 1) % Capacity;
            size = std::min(size + 1, Capacity);
        }
        return delta;
    }
    Summary Get() const {
        Summary result{}; result.count = size;
        if (!size) return result;
        auto sorted = values;
        std::sort(sorted.begin(), sorted.begin() + size);
        double sum = 0, square = 0;
        for (std::size_t i = 0; i < size; ++i) { sum += sorted[i]; square += double(sorted[i]) * sorted[i]; }
        result.mean = sum / size;
        result.p99 = sorted[std::min(size - 1, (size * 99 + 99) / 100 - 1)];
        result.maximum = sorted[size - 1];
        result.deviation = std::sqrt(std::max(0.0, square / size - result.mean * result.mean));
        return result;
    }
    std::size_t Count() const { return size; }
    const float* Data() const { return values.data(); }
    int Offset() const { return size == Capacity ? int(next) : 0; }
private:
    std::array<float, Capacity> values{};
    std::size_t size{}, next{};
    double previous{-1};
};
} // namespace BbTiming
