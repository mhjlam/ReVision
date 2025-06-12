#pragma once

#include "main.hpp"
#include "puzzle.hpp"

#include <opencv2/opencv.hpp>


// Forward declarations
class Menu;
class State;


class App {
public:
    App();
    ~App();

    void run();

    // OpenCV callbacks as static wrappers
    static void on_mouse(int event, int x, int y, int, void* userdata);
    static void wait_click_callback(int event, int, int, int, void* userdata);
    static void landing_page_mouse_callback(int event, int mx, int my, int, void* userdata);
    static void main_menu_mouse_callback(int event, int x, int y, int flags, void* userdata);

    // Core logic as member functions
    void handle_puzzle_solved(MouseState& mouse_state, std::map<std::string, bool>& solved_map, int& last_page);
    void show_start_screen(const cv::Mat& image_original, int block_width, int block_height);
    bool wait_for_mouse_click(const std::string& winname);
    void draw_text_overlay(cv::Mat& mat, const std::string& line1, const std::string& line2, int font_height1, int font_height2);

    // --- Add declarations for private helpers ---
private:
    void on_mouse_impl(int event, int x, int y, int, void* userdata);
    void wait_click_callback_impl(int event, int, int, int, void* userdata);
    void landing_page_mouse_callback_impl(int event, int mx, int my, int, void* userdata);
    void main_menu_mouse_callback_impl(int event, int x, int y, int flags, void* userdata);
    static PuzzleLayout make_puzzle_layout(const cv::Mat& image, int num_blocks_x, int num_blocks_y);
    static std::vector<cv::Rect> make_blocks(int cols, int rows, int block_width, int block_height);
    static std::vector<int> get_empty_neighbors(int empty_idx, int num_blocks_x, int num_blocks_y, const std::vector<int>& perm, bool avoid_zero);
    PuzzleSession create_puzzle_session(const PuzzleMeta& meta, const std::map<std::string, bool>& solved_map);

    // Members
    std::unique_ptr<Menu> menu;
    std::unique_ptr<State> state;
    std::unique_ptr<Puzzle> puzzle;
};
