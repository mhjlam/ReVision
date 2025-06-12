#pragma once

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class State {
public:
    static void save(const std::vector<int>& solved_indices, int last_page);
    static void load(std::vector<int>& solved_indices, int& last_page);
};
