#pragma once

#include <array>
#include <cstddef>

namespace gdu {
    // 최근 1초 안의 클릭 수를 세는 고정 크기 링 버퍼.
    //
    // 큐를 쓰면 클릭마다 할당이 생기므로, 좌석이 정해진 회전목마처럼 오래된
    // 기록을 새 기록이 덮어쓰게 한다. 할당도, 삭제도 없다.
    class ClickCounter {
    public:
        static ClickCounter& get();

        void reset();
        void push(double timestamp);

        // now 기준 1초 이내에 눌린 횟수.
        int perSecond(double now) const;

        // 현재 시도의 총 클릭 수.
        int total() const { return m_total; }

        // steady_clock 기반 단조 증가 시각(초).
        static double now();

    private:
        static constexpr std::size_t CAPACITY = 32;

        std::array<double, CAPACITY> m_times{};
        std::size_t m_head = 0;
        std::size_t m_filled = 0;
        int m_total = 0;
    };
}
