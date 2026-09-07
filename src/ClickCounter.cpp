#include "ClickCounter.hpp"

#include <chrono>

namespace gdu {
    ClickCounter& ClickCounter::get() {
        static ClickCounter instance;
        return instance;
    }

    double ClickCounter::now() {
        using namespace std::chrono;
        static const auto start = steady_clock::now();
        return duration<double>(steady_clock::now() - start).count();
    }

    void ClickCounter::reset() {
        m_head = 0;
        m_filled = 0;
        m_total = 0;
    }

    void ClickCounter::push(double timestamp) {
        m_times[m_head] = timestamp;
        m_head = (m_head + 1) % CAPACITY;
        if (m_filled < CAPACITY) {
            ++m_filled;
        }
        ++m_total;
    }

    int ClickCounter::perSecond(double now) const {
        int count = 0;
        for (std::size_t i = 0; i < m_filled; ++i) {
            if (now - m_times[i] <= 1.0) {
                ++count;
            }
        }
        return count;
    }
}
