#include "LayoutLog.hpp"

#include <algorithm>
#include <vector>

namespace {
    constexpr std::size_t LIMIT = 80;

    std::vector<std::string>& lines() {
        static std::vector<std::string> list;
        return list;
    }
}

namespace kopatch::layoutlog {
    void record(std::string line) {
        auto& list = lines();
        if (list.size() >= LIMIT) return;
        if (std::ranges::find(list, line) != list.end()) return;
        list.push_back(std::move(line));
    }

    std::string dump() {
        std::string out;
        for (auto const& line : lines()) {
            out += line;
            out += '\n';
        }
        return out;
    }
}
