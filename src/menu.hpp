#pragma once

#include "ft2.hpp"
#include "main.hpp"

#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

// Layout constants
constexpr int WIN_W = 900;
constexpr int WIN_H = 700;
constexpr int MARGIN = 20;
constexpr int BTN_W = 60;
constexpr int BTN_H = 120;
constexpr int NAV_FONT_HEIGHT = 36;

struct MenuLayout {
    int win_w, win_h;
    int margin;
    int thumb_w, thumb_h;
    int nav_y, y_offset;
    int draw_w, draw_h, img_x, img_y;
    int btn_w, btn_h, btn_y;
    int left_btn_x, right_btn_x;
};

struct MenuCallbackState {
    int selected;
    int nav_dir;
    std::string* hover;
};

class Menu {
public:
    Menu();
    int show(const std::vector<PuzzleMeta>& metas, const std::vector<cv::Mat>& previews, int page, const std::map<std::string, bool>& solved_map);
    
private:
    FT2TextRenderer ft2;
    std::string hover;
    int current_page;

private:
    void calc_preview_layout(int thumb_w, int thumb_h, int win_w, int win_h, const cv::Mat& thumb_src, int& draw_w, int& draw_h, int& img_x, int& img_y);

    void draw_arrow_btn(cv::Mat& canvas, int x, int y, int w, int h, bool hover, const std::string& arrow, const cv::Scalar& border_color, const cv::Scalar& hover_color, int border_thick, int hover_thick);

    void draw_puzzle_info(cv::Mat& canvas, const PuzzleMeta& meta, int idx, int win_w, int win_h, int y_offset, int thumb_h, const std::map<std::string, bool>& solved_map);

    MenuLayout compute_menu_layout(const cv::Mat& preview);

    cv::Mat draw_menu(const MenuLayout& menu_layout, int idx, int total_pages, const std::string& hover, const std::vector<PuzzleMeta>& metas, const std::vector<cv::Mat>& previews, const std::map<std::string, bool>& solved_map);

    char* setup_main_menu_mouse_callback(const MenuLayout& menu_layout, int idx, int total_pages, MenuCallbackState& state);
};
