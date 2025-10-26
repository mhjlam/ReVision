#pragma once

#include <algorithm>

class Util {
public:
    template<typename T>
    static constexpr T clamp(const T& v, const T& lo, const T& hi) {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }

    static bool is_empty(int x, int y, int empty_x, int empty_y) {
        return x == empty_x && y == empty_y;
    }
    
    static bool is_adjacent(int x1, int y1, int x2, int y2, int bw, int bh) {
        return (std::abs(x1 - x2) == bw && y1 == y2) || (std::abs(y1 - y2) == bh && x1 == x2);
    }
};
