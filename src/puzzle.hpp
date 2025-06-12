#pragma once

#include "main.hpp" // Ensure full type definitions are available

class App; // Forward declaration only

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>


class Puzzle {
public:
    static std::vector<PuzzleMeta> load_meta(const std::string& json_path);
    static cv::Mat load_image(const std::string& dat_path, const PuzzleMeta& meta);
    
public:
    PuzzleSession session;

public:
    explicit Puzzle(const PuzzleMeta& meta, const std::map<std::string, bool>& solved_map);

public:
    void play(std::map<std::string, bool>& solved_map, int& last_page, class App* app_cb_userdata);

    // Utility methods for App integration
    static cv::Mat pad_image_to_blocks(const cv::Mat& img, int num_blocks_x, int num_blocks_y, int& padded_cols, int& padded_rows, int& block_width, int& block_height);

    static void fill_image_from_permutation(cv::Mat& image_altered, const cv::Mat& image_original, const std::vector<int>& perm, int num_blocks_x, int num_blocks_y, int block_width, int block_height);

    static int permutation_manhattan_distance(const std::vector<int>& perm, int num_blocks_x, int num_blocks_y);
    static void swap_block(int x, int y, MouseState &state);
    static bool is_solved(const std::vector<int>& perm);
    static void on_mouse(int event, int x, int y, int flags, void* userdata);
    static std::vector<int> get_empty_neighbors(int empty_idx, int num_blocks_x, int num_blocks_y, const std::vector<int>& perm, bool avoid_zero);
    static void shuffle_permutation(std::vector<int>& perm, int num_blocks_x, int num_blocks_y, int& empty_idx, int min_challenge);
    static std::vector<cv::Rect> make_blocks(int cols, int rows, int block_width, int block_height);
    static PuzzleLayout make_puzzle_layout(const cv::Mat& image, int num_blocks_x, int num_blocks_y);
};
