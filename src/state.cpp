#include "state.hpp"

#include "main.hpp"
#include "puzzle.hpp"

#include <map>
#include <string>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>


void State::save(const std::vector<int>& solved_indices, int last_page) {
    std::ofstream f(PUZZLE_STATE_FILE, std::ios::binary | std::ios::trunc);
    if (!f) {
        return;
    }

    int32_t lp = static_cast<int32_t>(last_page);
    uint32_t n = static_cast<uint32_t>(solved_indices.size());
    f.write(reinterpret_cast<const char*>(&lp), sizeof(lp));
    f.write(reinterpret_cast<const char*>(&n), sizeof(n));

    for (uint32_t i = 0; i < n; ++i) {
        int32_t idx = static_cast<int32_t>(solved_indices[i]);
        f.write(reinterpret_cast<const char*>(&idx), sizeof(idx));
    }
}

void State::load(std::vector<int>& solved_indices, int& last_page) {
    last_page = 0;
    solved_indices.clear();
    
    std::ifstream f(PUZZLE_STATE_FILE, std::ios::binary);
    if (!f) {
        return;
    }

    int32_t lp = 0;
    uint32_t n = 0;
    f.read(reinterpret_cast<char*>(&lp), sizeof(lp));
    f.read(reinterpret_cast<char*>(&n), sizeof(n));
    last_page = lp;
    
    for (uint32_t i = 0; i < n; ++i) {
        int32_t idx = 0;
        f.read(reinterpret_cast<char*>(&idx), sizeof(idx));
        solved_indices.push_back(idx);
    }
}
