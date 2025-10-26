#include "app.hpp"

#include "ft2.hpp"
#include "main.hpp"
#include "menu.hpp"
#include "util.hpp"
#include "state.hpp"
#include "puzzle.hpp"

#include <map>
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <numeric>
#include <iostream>
#include <memory>

#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>


// Static wrappers for OpenCV callbacks
void App::on_mouse(int event, int x, int y, int flags, void* userdata) {
    if (userdata) {
        static_cast<App*>(userdata)->on_mouse_impl(event, x, y, flags, userdata);
    }
}
void App::wait_click_callback(int event, int x, int y, int flags, void* userdata) {
    if (userdata) {
        static_cast<App*>(userdata)->wait_click_callback_impl(event, x, y, flags, userdata);
    }
}
void App::landing_page_mouse_callback(int event, int x, int y, int flags, void* userdata) {
    if (userdata) {
        static_cast<App*>(userdata)->landing_page_mouse_callback_impl(event, x, y, flags, userdata);
    }
}
void App::main_menu_mouse_callback(int event, int x, int y, int flags, void* userdata) {
    if (userdata) static_cast<App*>(userdata)->main_menu_mouse_callback_impl(event, x, y, flags, userdata);
}


App::App() : menu(std::make_unique<Menu>()), state(std::make_unique<State>()), puzzle(nullptr) { 
}

App::~App() {
    cv::destroyAllWindows();
}

// Member implementations of logic and callbacks
void App::handle_puzzle_solved(MouseState& mouse_state, std::map<std::string, bool>& solved_map, int& last_page) {
    if (!mouse_state.solved) {
        mouse_state.solved = true;
        solved_map[mouse_state.puzzle_key] = true;

        mouse_state.image_altered = mouse_state.image_original.clone();
        draw_text_overlay(mouse_state.image_altered, "Finito!", "Press Escape to return", 56, 36);
        cv::imshow(WIN_NAME, mouse_state.image_altered);
    }
}

void App::show_start_screen(const cv::Mat& image_original, int block_width, int block_height) {
    cv::Mat display = image_original.clone();
    // Dim the top-left block to indicate the empty space
    cv::rectangle(display, cv::Rect(0, 0, block_width, block_height), cv::Scalar(0, 0, 0), cv::FILLED);

    // Overlay centered text
    std::string line1 = "Click to play";
    int font_height = 48;
    int font_height2 = 28;
    draw_text_overlay(display, line1, "", font_height, font_height2);

    cv::namedWindow(WIN_NAME, cv::WINDOW_AUTOSIZE);
    cv::resizeWindow(WIN_NAME, display.cols, display.rows);
    cv::imshow(WIN_NAME, display);
}

bool App::wait_for_mouse_click(const std::string& winname) {
    ClickState state;
    cv::setMouseCallback(winname, wait_click_callback, &state);

    while (!state.clicked) {
        if (cv::waitKey(1) == 27) {
            break;
        }
    }

    cv::setMouseCallback(winname, nullptr, nullptr);
    return state.clicked;
}

void App::draw_text_overlay(cv::Mat& mat, const std::string& line1, const std::string& line2, int font_height1, int font_height2) {
    int thickness = 2;
    int font = cv::FONT_HERSHEY_SIMPLEX;
    int baseline = 0;
    double scale1 = font_height1 / 32.0, scale2 = font_height2 / 32.0;

    // Calculate text sizes
    cv::Size sz1 = cv::getTextSize(line1, font, scale1, thickness, &baseline);
    cv::Size sz2 = cv::getTextSize(line2, font, scale2, thickness, &baseline);

    // Centering calculations
    int cx = mat.cols / 2;
    int cy = mat.rows / 2 - (sz1.height + sz2.height) / 2;
    int box_w = std::max(sz1.width, sz2.width) + 60;
    int box_h = sz1.height + sz2.height + 60;
    int box_x = cx - box_w / 2;
    int box_y = cy - 30;

    // Draw semi-transparent background box
    cv::Rect box_rect(box_x, box_y, box_w, box_h);
    cv::Mat overlay = mat.clone();
    cv::rectangle(overlay, box_rect, cv::Scalar(0, 0, 0, 180), cv::FILLED);
    cv::addWeighted(overlay, 0.6, mat, 0.4, 0, mat);

    // Render text using FT2TextRenderer
    FT2TextRenderer ft2(FONT_FILE);
    int text1_y = cy + sz1.height;
    int text2_y = text1_y + sz2.height + 10;

    if (!line1.empty()) { 
        ft2.draw_text(mat, line1, cv::Point(cx, text1_y), cv::Scalar(255, 255,  80), 2, true);
    }
    if (!line2.empty()) { 
        ft2.draw_text(mat, line2, cv::Point(cx, text2_y), cv::Scalar(255, 255, 255), 2, true);
    }
}

// Callback implementations
void App::on_mouse_impl(int event, int x, int y, int, void* userdata) {
    auto &state = *reinterpret_cast<MouseState*>(userdata);

    if (state.solved && event == cv::EVENT_LBUTTONDOWN) {
        return;
    }

    if (event != cv::EVENT_LBUTTONDOWN) {
        return;
    }

    int bx = (x / state.block_width) * state.block_width;
    int by = (y / state.block_height) * state.block_height;

    if (Util::is_empty(bx, by, state.empty_x, state.empty_y)) {
        return;
    }

    if (Util::is_adjacent(bx, by, state.empty_x, state.empty_y, state.block_width, state.block_height)) {
        Puzzle::swap_block(bx, by, state);
        auto& mat = state.image_altered;

        if (state.perm && Puzzle::is_solved(*state.perm)) {
            state.solved = true;
            mat = state.image_original.clone();
            draw_text_overlay(mat, "Finito!", "Press Escape to return", 56, 36);
        }

        cv::imshow(WIN_NAME, mat);
    }
}

void App::wait_click_callback_impl(int event, int, int, int, void* userdata) {
    if (event == cv::EVENT_LBUTTONDOWN) {
        static_cast<ClickState*>(userdata)->clicked = true;
    }
}

void App::landing_page_mouse_callback_impl(int event, int mx, int my, int, void* userdata) {
    if (event != cv::EVENT_LBUTTONDOWN) {
        return;
    }

    auto* p = static_cast<MouseClickParams*>(userdata);
    int grid = p->grid;
    int thumb_total = p->thumb_size + p->margin;

    // Calculate grid coordinates
    int gx = (mx - p->margin) / thumb_total;
    int gy = (my - p->margin) / thumb_total;

    if (gx < 0 || gx >= grid || gy < 0 || gy >= grid) {
        return;
    }

    int idx = gy * grid + gx;
    int pick = p->start + idx;
    if (pick >= p->end) {
        return;
    }

    int x = p->margin + gx * thumb_total;
    int y = p->margin + gy * thumb_total;
    int btn_y = y + p->thumb_size + 8;

    // Check if click is within the button area below the thumbnail
    if (mx >= x && mx < x + p->thumb_size && my >= btn_y && my < btn_y + 32) {
        *(p->selected) = pick;
    }
}

void App::main_menu_mouse_callback_impl(int event, int x, int y, int flags, void* userdata) {
    auto* params = static_cast<PageClickParams*>(userdata);
    auto* hover = *(std::string**)(params + 1);

    // Determine hover state
    std::string new_hover = "none";
    const bool over_left  = (x >= params->left_btn_x && x < params->left_btn_x + params->btn_w &&
                             y >= params->btn_y && y < params->btn_y + params->btn_h);
    const bool over_right = (x >= params->right_btn_x && x < params->right_btn_x + params->btn_w &&
                             y >= params->btn_y && y < params->btn_y + params->btn_h);
    const bool over_image = (x >= params->img_x && x < params->img_x + params->draw_w &&
                             y >= params->img_y && y < params->img_y + params->draw_h);

    if (over_left)      new_hover = "left";
    else if (over_right) new_hover = "right";
    else if (over_image) new_hover = "image";

    if (event == cv::EVENT_MOUSEMOVE) {
        if (*hover != new_hover) {
            *hover = new_hover;
        }
        return;
    }

    if (event != cv::EVENT_LBUTTONDOWN) {
        return;
    }

    if (new_hover == "left" && params->page > 0) {
        *params->nav_dir = -1;
    }
    else if (new_hover == "right" && params->page < params->total_pages - 1) {
        *params->nav_dir = 1;
    }
    else if (new_hover == "image") {
        *params->selected = params->page;
    }
}

PuzzleSession App::create_puzzle_session(const PuzzleMeta& meta, const std::map<std::string, bool>& solved_map) {
    PuzzleSession session{meta};
    session.puzzle_key = meta.name + "|" + meta.artist;
    session.solved = solved_map.contains(session.puzzle_key) && solved_map.at(session.puzzle_key);

    session.image_original = Puzzle::load_image(PUZZLE_DATA_FILE, meta);
    if (session.image_original.empty()) {
        throw std::runtime_error("Failed to load image for puzzle: " + session.puzzle_key);
    }

    int n = meta.block_size;
    session.layout = Puzzle::make_puzzle_layout(session.image_original, n, n);
    session.blocks = Puzzle::make_blocks(session.layout.cols, session.layout.rows, session.layout.block_width, session.layout.block_height);

    int total = n * n;
    session.perm.resize(total);
    Puzzle::shuffle_permutation(session.perm, n, n, session.empty_idx, std::max(6, 2 * (total - 1)));

    return session;
}

void App::run() {
    // Load puzzle metadata and previews
    auto metas = Puzzle::load_meta(PUZZLE_META_FILE);
    std::vector<cv::Mat> previews;

    for (const auto& meta : metas) {
        cv::Mat img = Puzzle::load_image(PUZZLE_DATA_FILE, meta);

        if (img.empty()) {
            std::cerr << "Failed to load preview for: " << meta.name << std::endl;
            previews.push_back(cv::Mat(128,128,CV_8UC3,cv::Scalar(50,50,50)));
        }
        else {
            previews.push_back(img);
        }
    }

    if (previews.empty()) {
        std::cerr << "No puzzles found in " << PUZZLE_DATA_FILE << std::endl;
        return;
    }

    // Load persistent state (binary)
    std::map<std::string, bool> solved_map;
    
    int last_page = 0;
    std::vector<int> solved_indices;
    State::load(solved_indices, last_page);

    for (int idx : solved_indices) {
        if (idx >= 0 && idx < (int)metas.size()) {
            const auto& meta = metas[idx];
            std::string key = meta.name + "|" + meta.artist;
            solved_map[key] = true;
        }
    }

    while (true) {
        // Show main menu and get puzzle selection
        int pick = menu ? menu->show(metas, previews, last_page, solved_map) : last_page;

        if (pick < 0 || pick >= static_cast<int>(metas.size())) {
            break;
        }

        last_page = pick;

        // Save progress after menu selection (including last_page)
        std::vector<int> save_indices;
        for (size_t i = 0; i < metas.size(); ++i) {
            std::string key = metas[i].name + "|" + metas[i].artist;
            if (solved_map.count(key) && solved_map[key]) {
                save_indices.push_back((int)i);
            }
        }
        State::save(save_indices, last_page);

        // Play the selected puzzle
        const auto& meta = metas[pick];
        Puzzle puzzle(meta, solved_map);
        puzzle.play(solved_map, last_page, this);

        // Save progress after each puzzle (in case solved_map changed)
        save_indices.clear();
        for (size_t i = 0; i < metas.size(); ++i) {
            std::string key = metas[i].name + "|" + metas[i].artist;
            if (solved_map.count(key) && solved_map[key]) {
                save_indices.push_back((int)i);
            }
        }

        State::save(save_indices, last_page);
    }

    cv::destroyAllWindows();
}
