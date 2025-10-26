#include "menu.hpp"

#include "app.hpp"
#include "state.hpp"
#include "puzzle.hpp"

#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include <opencv2/opencv.hpp>


Menu::Menu() : ft2(FONT_FILE), hover("none"), current_page(0) {}

void Menu::calc_preview_layout(int thumb_w, int thumb_h, int win_w, int win_h, const cv::Mat& thumb_src, int& draw_w, int& draw_h, int& img_x, int& img_y) {
    double aspect = static_cast<double>(thumb_src.cols) / thumb_src.rows;
    int max_w = thumb_w, max_h = thumb_h;
    int min_w = thumb_w / 4;
    draw_w = max_w; draw_h = max_h;

    if (aspect > 1.0) {
        draw_w = std::min(max_w, std::max(min_w, static_cast<int>(max_h * aspect)));
        draw_h = static_cast<int>(draw_w / aspect);

        if (draw_h > max_h) {
            draw_h = max_h;
            draw_w = static_cast<int>(draw_h * aspect);
        }
    }
    else {
        draw_h = max_h;
        draw_w = static_cast<int>(draw_h * aspect);
        
        if (draw_w < min_w) {
            draw_w = min_w;
            draw_h = static_cast<int>(draw_w / aspect);
        }

        if (draw_w > max_w) {
            draw_w = max_w;
            draw_h = static_cast<int>(draw_w / aspect);
        }
    }

    int arrow_img_gap = 8;
    int max_draw_w = thumb_w - 2 * arrow_img_gap;

    if (draw_w > max_draw_w) {
        draw_w = max_draw_w;
        draw_h = static_cast<int>(draw_w / aspect);
    }

    img_x = (win_w - draw_w) / 2;
    img_y = ((win_h - thumb_h) / 2) + (thumb_h - draw_h) / 2;
}

// Helper to draw an arrow button (left/right)
void Menu::draw_arrow_btn(cv::Mat& canvas, int x, int y, int w, int h, bool hover, const std::string& arrow, const cv::Scalar& border_color, const cv::Scalar& hover_color, int border_thick, int hover_thick) {
    cv::Scalar color = hover ? hover_color : border_color;
    int thick = hover ? hover_thick : border_thick;

    cv::rectangle(canvas, cv::Rect(x, y, w, h), color, -1);
    cv::rectangle(canvas, cv::Rect(x, y, w, h), color, thick);

    int arrow_cx = x + w / 2;
    int arrow_cy = y + h / 2 + 20;
    ft2.draw_text(canvas, arrow, cv::Point(arrow_cx, arrow_cy), cv::Scalar(255,255,255), 3, true);
}

// Helper to draw puzzle info (name, artist, solved, difficulty)
void Menu::draw_puzzle_info(cv::Mat& canvas, const PuzzleMeta& meta, int idx, int win_w, int win_h, int y_offset, int thumb_h, const std::map<std::string, bool>& solved_map) {
    int info_center_x = win_w / 2;
    int info_y = y_offset + thumb_h + 60;
    
    ft2.draw_text(canvas, meta.name, cv::Point(info_center_x, info_y), cv::Scalar(255,255,255), 2, true);
    ft2.draw_text(canvas, meta.artist, cv::Point(info_center_x, info_y + 50), cv::Scalar(200,200,200), 1, true);

    bool solved = false;
    auto it = solved_map.find(meta.name + "|" + meta.artist);
    if (it != solved_map.end()) {
        solved = it->second;
    }

    ft2.draw_text(canvas, solved ? "Solved" : "Unsolved", cv::Point(30, win_h - 30), solved ? cv::Scalar(0,255,0) : cv::Scalar(0,0,255), 2);

    cv::Scalar diff_color(0,255,0);
    if (meta.difficulty == "Medium" || meta.difficulty == "medium") {
        diff_color = cv::Scalar(0,255,255);
    }
    else if (meta.difficulty == "Hard" || meta.difficulty == "hard") {
        diff_color = cv::Scalar(0,0,255);
    }

    int baseline = 0;
    cv::Size diff_sz = cv::getTextSize(meta.difficulty, cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline);
    ft2.draw_text(canvas, meta.difficulty, cv::Point(win_w - diff_sz.width - 40, win_h - 30), diff_color, 2);
}

MenuLayout Menu::compute_menu_layout(const cv::Mat& preview) {
    MenuLayout menu_layout {
        .win_w = WIN_W,
        .win_h = WIN_H,
        .margin = MARGIN,
        .thumb_w = WIN_W - 2 * MARGIN - 2 * BTN_W,
        .thumb_h = WIN_H - 220,
        .nav_y = MARGIN + NAV_FONT_HEIGHT/2,
        .y_offset = MARGIN + NAV_FONT_HEIGHT,
    };

    int preview_area_x = menu_layout.margin + BTN_W;
    int preview_area_y = menu_layout.y_offset;
    int preview_area_w = menu_layout.thumb_w;
    int preview_area_h = menu_layout.thumb_h;
    double aspect = static_cast<double>(preview.cols) / preview.rows;
    int draw_w = preview_area_w, draw_h = preview_area_h;
    
    if (aspect > 1.0) {
        draw_w = preview_area_w;
        draw_h = static_cast<int>(draw_w / aspect);

        if (draw_h > preview_area_h) {
            draw_h = preview_area_h;
            draw_w = static_cast<int>(draw_h * aspect);
        }
    }
    else {
        draw_h = preview_area_h;
        draw_w = static_cast<int>(draw_h * aspect);

        if (draw_w > preview_area_w) {
            draw_w = preview_area_w;
            draw_h = static_cast<int>(draw_w / aspect);
        }
    }

    menu_layout.draw_w = draw_w;
    menu_layout.draw_h = draw_h;
    menu_layout.img_x = preview_area_x + (preview_area_w - draw_w) / 2;
    menu_layout.img_y = preview_area_y + (preview_area_h - draw_h) / 2;
    menu_layout.btn_w = BTN_W;
    menu_layout.btn_h = BTN_H;
    menu_layout.btn_y = preview_area_y + (preview_area_h - menu_layout.btn_h) / 2;
    menu_layout.left_btn_x = menu_layout.margin;
    menu_layout.right_btn_x = menu_layout.win_w - menu_layout.margin - menu_layout.btn_w;
    return menu_layout;
}

// Draws the main menu UI
cv::Mat Menu::draw_menu(const MenuLayout& menu_layout, int idx, int total_pages, const std::string& hover, const std::vector<PuzzleMeta>& metas, const std::vector<cv::Mat>& previews, const std::map<std::string, bool>& solved_map) {

    // Ensure window is created and resized for the menu
    cv::namedWindow(WIN_NAME, cv::WINDOW_AUTOSIZE);
    cv::resizeWindow(WIN_NAME, menu_layout.win_w, menu_layout.win_h);
    cv::Mat canvas = cv::Mat(menu_layout.win_h, menu_layout.win_w, CV_8UC3, cv::Scalar(30,30,30));
    std::string nav = std::to_string(idx+1) + "/" + std::to_string(total_pages);
    ft2.draw_text(canvas, nav, cv::Point(menu_layout.win_w/2, menu_layout.nav_y), cv::Scalar(255,255,255), 2, true);

    // Preview image
    cv::Mat thumb;
    cv::resize(previews[idx], thumb, cv::Size(menu_layout.draw_w, menu_layout.draw_h));
    thumb.copyTo(canvas(cv::Rect(menu_layout.img_x, menu_layout.img_y, menu_layout.draw_w, menu_layout.draw_h)));
    cv::Scalar border_color(80,140,220);
    cv::Scalar hover_color(180,220,255);
    int border_thick = 4, hover_thick = 8;

    // Highlight preview if hovered
    cv::Scalar img_border = (hover == "image") ? hover_color : border_color;
    int img_thick = (hover == "image") ? hover_thick : border_thick;
    cv::rectangle(canvas, cv::Rect(menu_layout.img_x, menu_layout.img_y, menu_layout.draw_w, menu_layout.draw_h), img_border, img_thick);

    // Draw left/right arrow buttons
    draw_arrow_btn(canvas, menu_layout.left_btn_x, menu_layout.btn_y, menu_layout.btn_w, menu_layout.btn_h, hover == "left", "←", border_color, hover_color, border_thick, hover_thick);
    draw_arrow_btn(canvas, menu_layout.right_btn_x, menu_layout.btn_y, menu_layout.btn_w, menu_layout.btn_h, hover == "right", "→", border_color, hover_color, border_thick, hover_thick);

    // Draw puzzle info
    draw_puzzle_info(canvas, metas[idx], idx, menu_layout.win_w, menu_layout.win_h, menu_layout.y_offset, menu_layout.thumb_h, solved_map);
    cv::imshow(WIN_NAME, canvas);

    return canvas;
}

// Helper to set up mouse callback and buffer
char* Menu::setup_main_menu_mouse_callback(const MenuLayout& menu_layout, int idx, int total_pages, MenuCallbackState& state) {
    size_t buf_size = sizeof(PageClickParams) + sizeof(std::string*);
    char* cb_data = new char[buf_size];

    PageClickParams cb_params{
        menu_layout.img_x, menu_layout.img_y, menu_layout.draw_w, menu_layout.draw_h,
        menu_layout.btn_w, menu_layout.btn_h, menu_layout.btn_y, menu_layout.left_btn_x, menu_layout.right_btn_x,
        menu_layout.win_w, menu_layout.win_h, idx, total_pages,
        &state.selected, &state.nav_dir
    };

    memcpy(cb_data, &cb_params, sizeof(PageClickParams));
    std::string** hover_ptr_ptr = reinterpret_cast<std::string**>(cb_data + sizeof(PageClickParams));
    *hover_ptr_ptr = state.hover;
    cv::setMouseCallback(WIN_NAME, App::main_menu_mouse_callback, cb_data);
    return cb_data;
}

// Show the main menu with puzzle previews and navigation
int Menu::show(const std::vector<PuzzleMeta>& metas, const std::vector<cv::Mat>& previews, int page, const std::map<std::string, bool>& solved_map) {
    current_page = page;
    int total_pages = static_cast<int>(metas.size());
    std::string last_hover = hover;

    while (true) {
        MenuLayout menu_layout = compute_menu_layout(previews[current_page]);
        MenuCallbackState cb_state{ -1, 0, &hover };
        last_hover = hover;

        cv::Mat canvas = draw_menu(menu_layout, current_page, total_pages, hover, metas, previews, solved_map);
        char* cb_data = setup_main_menu_mouse_callback(menu_layout, current_page, total_pages, cb_state);

        while (cb_state.selected == -1 && cb_state.nav_dir == 0) {
            int key = cv::waitKey(1);
            if (cv::getWindowProperty(WIN_NAME, cv::WND_PROP_VISIBLE) < 1 || key == 27) {
                delete[] cb_data;
                return -1;
            }

            if (hover != last_hover) {
                last_hover = hover;
                canvas = draw_menu(menu_layout, current_page, total_pages, hover, metas, previews, solved_map);
            }

            // Save last selected preview on every highlight change
            static int last_saved_page = -1;
            if (current_page != last_saved_page) {

                // Build solved_indices for saving
                std::vector<int> solved_indices;
                for (size_t i = 0; i < metas.size(); ++i) {
                    std::string key = metas[i].name + "|" + metas[i].artist;
                    if (solved_map.count(key) && solved_map.at(key)) {
                        solved_indices.push_back((int)i);
                    }
                }
                State::save(solved_indices, current_page);
                last_saved_page = current_page;
            }
        }
        
        cv::setMouseCallback(WIN_NAME, nullptr, nullptr);

        delete[] cb_data;
        if (cb_state.selected != -1) {
            return cb_state.selected;
        }

        current_page += cb_state.nav_dir;
    }
}
