#pragma once

#include <string>
#include <memory>

#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>

constexpr const char* WIN_NAME = "ReVision Sliding Puzzle";
constexpr const char* FONT_FILE = "res/NotoSansJP-Regular.ttf";
constexpr const char* PUZZLE_STATE_FILE = "res/puzzle_state";
constexpr const char* PUZZLE_DATA_FILE = "res/puzzles.dat";
constexpr const char* PUZZLE_META_FILE = "res/puzzles.json";

struct MouseState {
    int block_width, block_height, cols, rows;
    int &empty_x, &empty_y;

    cv::Mat &image_altered;
    const cv::Mat &image_original;

    std::vector<cv::Rect> &blocks;
    std::vector<int>* perm = nullptr;

    bool solved = false;
    std::string puzzle_key;
};

struct ClickState {
    bool clicked = false;
};

struct MouseClickParams {
    int margin, thumb_size, grid, start, end, win_h;
    int* selected;
};

struct PuzzleMeta {
    std::string name;
    std::string artist;
    std::string difficulty;
    
    int offset;
    int length;
    int block_size;
};

struct PageClickParams {
    int img_x, img_y;
    int draw_w, draw_h;
    int btn_w, btn_h, btn_y;
    int left_btn_x, right_btn_x;
    int win_w, win_h;
    int page, total_pages;
    int* selected;
    int* nav_dir;
};

struct PuzzleLayout {
    int cols, rows, block_width, block_height;
    cv::Mat padded;
};

struct PuzzleSession {
    PuzzleMeta meta;
    std::string puzzle_key;

    bool solved;
    PuzzleLayout layout;

    std::vector<cv::Rect> blocks;
    std::vector<int> perm;

    int empty_idx;
    cv::Mat image_original;
};

struct MainMenuCallbackData {
    PageClickParams params;
    std::string* hover;
};
