// --- Includes and Constants ---

#include <map>
#include <random>
#include <vector>
#include <fstream>
#include <numeric>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <filesystem>

#include <zlib.h>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

#include "ft2_text_render.hpp"


constexpr const char* WIN_NAME = "Puzzle";
constexpr const char* FONT_FILE = "res/NotoSansJP-Regular.ttf";
constexpr const char* PUZZLE_STATE_FILE = "res/puzzle_state.json";
constexpr const char* PUZZLE_DATA_FILE = "res/puzzles.dat";
constexpr const char* PUZZLE_META_FILE = "res/puzzles.json";


// --- Structs ---
struct MouseState
{
    int block_width, block_height, cols, rows;
    int &empty_x, &empty_y;
    cv::Mat &image_altered;
    const cv::Mat &image_original;
    std::vector<cv::Rect> &blocks;
    std::vector<int>* perm = nullptr; // pointer to permutation vector
    bool solved = false;
    nlohmann::json* puzzle_state = nullptr; // pointer to puzzle state JSON
    std::string puzzle_key; // key for the current puzzle
};

struct ClickState
{ 
    bool clicked = false;
};

struct MouseClickParams
{
    int margin, thumb_size, grid, start, end, win_h;
    int* selected;
};

struct PuzzleMeta
{
    std::string name;
    std::string artist;
    std::string difficulty;
    int offset;
    int length;
    int block_size;
};

struct PageClickParams
{
    int img_x, img_y, draw_w, draw_h, btn_w, btn_h, btn_y, left_btn_x, right_btn_x, win_w, win_h, page, total_pages;
    int* selected;
    int* nav_dir;
};


// --- Function Signatures ---

// Utility and puzzle logic
inline bool is_empty(int x, int y, int empty_x, int empty_y);
inline bool is_adjacent(int x1, int y1, int x2, int y2, int bw, int bh);
int permutation_manhattan_distance(const std::vector<int>& perm, int num_blocks_x, int num_blocks_y);
void fill_image_from_permutation(cv::Mat& image_altered, const cv::Mat& image_original, const std::vector<int>& perm, int num_blocks_x, int num_blocks_y, int block_width, int block_height);
cv::Mat pad_image_to_blocks(const cv::Mat& img, int num_blocks_x, int num_blocks_y, int& padded_cols, int& padded_rows, int& block_width, int& block_height);
bool is_solved(const std::vector<int>& perm);
int total_manhattan_distance(const cv::Mat& altered, const cv::Mat& original, int bw, int bh, int cols, int rows, int empty_x, int empty_y);
void swap_block(int x, int y, MouseState &state);

// Mouse and UI
void on_mouse(int event, int x, int y, int, void* userdata);
void show_start_screen(const cv::Mat& image_original, int block_width, int block_height);
bool wait_for_mouse_click(const std::string& winname);

// Landing page and puzzle loading
int show_main_menu(const std::vector<PuzzleMeta>& metas, const std::vector<cv::Mat>& previews, int page, const std::map<std::string, bool>& solved_map);
std::vector<PuzzleMeta> load_puzzle_meta(const std::string& json_path);
cv::Mat load_puzzle_image(const std::string& dat_path, const PuzzleMeta& meta);
void save_puzzle_state(const nlohmann::json& state);

// Callbacks functions
void wait_click_callback(int event, int, int, int, void* userdata);
void landing_page_mouse_callback(int event, int mx, int my, int, void* userdata);


// --- Definitions ---

static FT2TextRenderer ft2(FONT_FILE);


// -- Implementations ---

// Check if a block is the empty block
inline bool is_empty(int x, int y, int empty_x, int empty_y) {
    return x == empty_x && y == empty_y;
}

// Check if two blocks are adjacent
inline bool is_adjacent(int x1, int y1, int x2, int y2, int bw, int bh) {
    return (std::abs(x1 - x2) == bw && y1 == y2) || (std::abs(y1 - y2) == bh && x1 == x2);
}

// Returns the Manhattan distance for the current permutation (excluding the empty block at index 0)
int permutation_manhattan_distance(const std::vector<int>& perm, int num_blocks_x, int num_blocks_y) {
    int total = 0;
    for (int idx = 1; idx < (int)perm.size(); ++idx) { // skip empty block at 0
        int cur = perm[idx];
        if (cur == 0) continue; // skip empty
        int x = idx % num_blocks_x, y = idx / num_blocks_x;
        int sx = cur % num_blocks_x, sy = cur / num_blocks_x;
        total += std::abs(x - sx) + std::abs(y - sy);
    }
    return total;
}

// Fills image_altered from the permutation vector
void fill_image_from_permutation(cv::Mat& image_altered, const cv::Mat& image_original, const std::vector<int>& perm, int num_blocks_x, int num_blocks_y, int block_width, int block_height) {
    int idx = 0;
    for (int by = 0; by < num_blocks_y; ++by) {
        for (int bx = 0; bx < num_blocks_x; ++bx, ++idx) {
            int src_idx = perm[idx];
            int sx = src_idx % num_blocks_x, sy = src_idx / num_blocks_x;
            int x = bx * block_width, y = by * block_height;
            int w = std::min(block_width, image_original.cols - x);
            int h = std::min(block_height, image_original.rows - y);
            cv::Rect dst_rect(x, y, w, h);
            if (src_idx == 0) {
                image_altered(dst_rect).setTo(cv::Scalar(0,0,0));
            } else {
                int src_x = sx * block_width, src_y = sy * block_height;
                int sw = std::min(block_width, image_original.cols - src_x);
                int sh = std::min(block_height, image_original.rows - src_y);

                // Ensure src_rect and dst_rect are the same size
                int copy_w = std::min(sw, w);
                int copy_h = std::min(sh, h);
                if (copy_w > 0 && copy_h > 0) {
                    cv::Rect src_rect(src_x, src_y, copy_w, copy_h);
                    cv::Rect dst_rect_adj(x, y, copy_w, copy_h);
                    image_original(src_rect).copyTo(image_altered(dst_rect_adj));
                }
            }
        }
    }
}

// Pads the image with black borders so that its dimensions are divisible by block size
cv::Mat pad_image_to_blocks(const cv::Mat& img, int num_blocks_x, int num_blocks_y, int& padded_cols, int& padded_rows, int& block_width, int& block_height) {
    block_width = (img.cols + num_blocks_x - 1) / num_blocks_x;
    block_height = (img.rows + num_blocks_y - 1) / num_blocks_y;
    padded_cols = block_width * num_blocks_x;
    padded_rows = block_height * num_blocks_y;
    int right = padded_cols - img.cols;
    int bottom = padded_rows - img.rows;
    cv::Mat padded;
    cv::copyMakeBorder(img, padded, 0, bottom, 0, right, cv::BORDER_CONSTANT, cv::Scalar(0,0,0));
    return padded;
}

// Check if the puzzle is solved (all blocks except the empty one are in their solved positions)
bool is_solved(const std::vector<int>& perm) {
    for (size_t i = 1; i < perm.size(); ++i) {
        if ((int)i != perm[i]) return false;
    }
    return true;
}

// Compute total Manhattan distance of all blocks from their solved positions (excluding the empty block)
int total_manhattan_distance(const cv::Mat& altered, const cv::Mat& original, int bw, int bh, int cols, int rows, int empty_x, int empty_y) {
    int total = 0;
    for (int y = 0; y < rows; y += bh) {
        int h = std::min(bh, rows - y);
        for (int x = 0; x < cols; x += bw) {
            int w = std::min(bw, cols - x);
            if (is_empty(x, y, empty_x, empty_y)) continue;
            cv::Rect rect(x, y, w, h);
            cv::Mat block = altered(rect);
            cv::Vec3b sig = block.at<cv::Vec3b>(0, 0);
            // Find the matching block in the original image
            for (int sy = 0; sy < rows; sy += bh) {
                int sh = std::min(bh, rows - sy);
                for (int sx = 0; sx < cols; sx += bw) {
                    int sw = std::min(bw, cols - sx);
                    cv::Rect orig_rect(sx, sy, sw, sh);
                    cv::Mat orig_block = original(orig_rect);
                    if (orig_block.size() == block.size() && orig_block.at<cv::Vec3b>(0, 0) == sig) {
                        total += std::abs((x - sx) / bw) + std::abs((y - sy) / bh);
                        goto next_block;
                    }
                }
            }
        next_block:;
        }
    }
    return total;
}

// Swap a block with the empty block if adjacent
void swap_block(int x, int y, MouseState &state) {
    if (!is_adjacent(x, y, state.empty_x, state.empty_y, state.block_width, state.block_height)) return;

    int from_w = std::min(state.block_width, state.cols - x);
    int from_h = std::min(state.block_height, state.rows - y);
    int to_w = std::min(state.block_width, state.cols - state.empty_x);
    int to_h = std::min(state.block_height, state.rows - state.empty_y);
    int copy_w = std::min(from_w, to_w), copy_h = std::min(from_h, to_h);

    cv::Rect from_rect(x, y, copy_w, copy_h), to_rect(state.empty_x, state.empty_y, copy_w, copy_h);
    if (from_rect.size() != to_rect.size() || copy_w <= 0 || copy_h <= 0) return;

    cv::Mat temp; state.image_altered(from_rect).copyTo(temp);
    state.image_altered(to_rect).copyTo(state.image_altered(from_rect));
    temp.copyTo(state.image_altered(to_rect));

    // Update permutation if available
    if (state.perm) {
        int num_blocks_x = (state.cols + state.block_width - 1) / state.block_width;
        int num_blocks_y = (state.rows + state.block_height - 1) / state.block_height;
        int from_idx = (y / state.block_height) * num_blocks_x + (x / state.block_width);
        int to_idx = (state.empty_y / state.block_height) * num_blocks_x + (state.empty_x / state.block_width);
        std::swap((*state.perm)[from_idx], (*state.perm)[to_idx]);
    }

    state.empty_x = x; state.empty_y = y;
}


// --- Mouse Callback ---
void on_mouse(int event, int x, int y, int, void* userdata) {
    auto &state = *reinterpret_cast<MouseState*>(userdata);

    if (state.solved && event == cv::EVENT_LBUTTONDOWN) {
        // Handle this in main loop
        return;
    }

    if (event != cv::EVENT_LBUTTONDOWN) {
        return;
    }

    int bx = (x / state.block_width) * state.block_width;
    int by = (y / state.block_height) * state.block_height;
    if (is_empty(bx, by, state.empty_x, state.empty_y)) return;

    if (is_adjacent(bx, by, state.empty_x, state.empty_y, state.block_width, state.block_height)) {
        swap_block(bx, by, state);

        auto& mat = state.image_altered;

        // Use permutation-based solved check if available
        if (state.perm && is_solved(*state.perm)) {
            if (state.puzzle_state) {
                (*state.puzzle_state)[state.puzzle_key] = true;
                save_puzzle_state(*state.puzzle_state);
            }
            // Restore color
            mat = state.image_original.clone();
            std::string line1 = "Finito!";
            std::string line2 = "Press Escape to return";
            int font_height1 = 56, font_height2 = 36;
            int thickness = 2;

            // Calculate text size for background box
            int font = cv::FONT_HERSHEY_SIMPLEX;
            int baseline = 0;
            double scale1 = font_height1 / 32.0, scale2 = font_height2 / 32.0;
            cv::Size sz1 = cv::getTextSize(line1, font, scale1, thickness, &baseline);
            cv::Size sz2 = cv::getTextSize(line2, font, scale2, thickness, &baseline);
            int cx = mat.cols / 2;
            int cy = mat.rows / 2 - sz1.height;
            int box_w = std::max(sz1.width, sz2.width) + 60;
            int box_h = sz1.height + sz2.height + 80;
            int box_x = cx - box_w/2;
            int box_y = cy - 50;

            // Draw semi-transparent background
            cv::Mat roi = mat(cv::Rect(box_x, box_y, box_w, box_h));
            cv::Mat overlay;
            roi.copyTo(overlay);
            cv::rectangle(overlay, cv::Rect(0,0,box_w,box_h), cv::Scalar(0,0,0,180), -1);
            cv::addWeighted(overlay, 0.7, roi, 0.3, 0, roi);

            // Draw text
            ft2.putText(mat, line1, cv::Point(cx, cy), cv::Scalar(255,255,80), 2, true);
            ft2.putText(mat, line2, cv::Point(cx, cy + sz1.height + 40), cv::Scalar(255,255,255), 2, true);
        }

        cv::imshow(WIN_NAME, mat);
    }
}

// Show the original image (minus top-left block) with overlay and wait for click to start
void show_start_screen(const cv::Mat& image_original, int block_width, int block_height) {
    cv::Mat display = image_original.clone();
    cv::Rect top_left_rect(0, 0, block_width, block_height);
    display(top_left_rect).setTo(cv::Scalar(0,0,0));

    // Overlay text
    int font = cv::FONT_HERSHEY_SIMPLEX, thickness = 3, baseline = 0;
    double scale = 2.0;
    std::string line1 = "Click to play";
    cv::Size text1 = cv::getTextSize(line1, font, scale, thickness, &baseline);
    int cx = (display.cols - text1.width) / 2, cy = display.rows / 2 - text1.height;
    cv::putText(display, line1, {cx, cy}, font, scale, cv::Scalar(80, 140, 220), thickness, cv::LINE_AA);
    cv::imshow(WIN_NAME, display);
}

// Wait for a mouse click (returns true if left button is clicked)
bool wait_for_mouse_click(const std::string& winname) {
    ClickState state;
    cv::setMouseCallback(winname, wait_click_callback, &state);

    while (!state.clicked) {
        if (cv::waitKey(30) == 27) break; // allow ESC to exit
    }

    cv::setMouseCallback(winname, nullptr, nullptr);
    return state.clicked;
}

// Show the main menu with puzzle previews and navigation
int show_main_menu(const std::vector<PuzzleMeta>& metas, const std::vector<cv::Mat>& previews, int page, const std::map<std::string, bool>& solved_map) {
    int total_pages = static_cast<int>(metas.size());
    int win_w = 900, win_h = 700;
    int margin = 20;
    int thumb_w = win_w - 2 * margin - 2 * 60; // 60 for each arrow box
    int thumb_h = win_h - 220;
    int selected = -1;
    int idx = page;
    std::string hover = "none";
    std::string last_hover = "none";

    while (true) {
        cv::Mat canvas(win_h, win_w, CV_8UC3, cv::Scalar(30,30,30));

        // Draw page indicator at the top, centered (above the image, not covering it)
        int nav_font_height = 36; // smaller font
        int nav_y = margin + nav_font_height/2; // set to margin from the top
        std::string nav = std::to_string(idx+1) + "/" + std::to_string(total_pages);
        ft2.putText(canvas, nav, cv::Point(win_w/2, nav_y), cv::Scalar(255,255,255), 2, true);
        
        // Move everything else down to fit well
        int y_offset = nav_y + nav_font_height/2;
        
        // Calculate preview size to fit in the available area, preserving aspect ratio
        cv::Mat thumb_src = previews[idx];
        double aspect = static_cast<double>(thumb_src.cols) / thumb_src.rows;
        int max_w = thumb_w, max_h = thumb_h;
        int min_w = thumb_w / 4; // don't reduce width by more than 75%
        int draw_w = max_w, draw_h = max_h;

        // Wide image
        if (aspect > 1.0) {
            draw_w = std::min(max_w, std::max(min_w, static_cast<int>(max_h * aspect)));
            draw_h = static_cast<int>(draw_w / aspect);
            if (draw_h > max_h) {
                draw_h = max_h;
                draw_w = static_cast<int>(draw_h * aspect);
            }
        }
        // Tall or square image
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
        
        // Center the preview in the available area, but ensure a margin between image border and arrow boxes
        int arrow_img_gap = 8; // minimal gap in pixels between image border and arrow boxes
        int max_draw_w = thumb_w - 2 * arrow_img_gap;
        if (draw_w > max_draw_w) {
            draw_w = max_draw_w;
            draw_h = static_cast<int>(draw_w / aspect);
        }
        
        // Center the preview in the available area (centered in window), but clamp so it never touches the arrow boxes
        int img_x = (win_w - draw_w) / 2;
        int img_y = y_offset + (thumb_h - draw_h) / 2;
        cv::Mat thumb;
        cv::resize(thumb_src, thumb, cv::Size(draw_w, draw_h));
        thumb.copyTo(canvas(cv::Rect(img_x, img_y, draw_w, draw_h)));
        
        // Border
        cv::Scalar border_color(80, 140, 220); // friendly color
        cv::rectangle(canvas, cv::Rect(img_x, img_y, draw_w, draw_h), border_color, 4);
        
        // Draw navigation arrows (vertically centered in preview area, fixed distance from window edges)
        int btn_w = 60, btn_h = 120;
        int preview_area_top = y_offset;
        int preview_area_h = thumb_h;
        int btn_y = preview_area_top + (preview_area_h - btn_h) / 2;
        int left_btn_x = margin; // fixed distance from left edge
        int right_btn_x = win_w - margin - btn_w; // fixed distance from right edge

        // Highlight effect for hover
        cv::Scalar hover_color(180, 220, 255); // highlight color
        int border_thick = 4;
        int hover_thick = 8;

        // Preview image border (drawn first, so arrow boxes are on top)
        cv::Scalar img_border = (hover == "image") ? hover_color : border_color;
        int img_thick = (hover == "image") ? hover_thick : border_thick;
        cv::rectangle(canvas, cv::Rect(img_x, img_y, draw_w, draw_h), img_border, img_thick);

        // Left arrow (drawn OVER preview image and border)
        cv::Scalar left_color = (hover == "left") ? hover_color : border_color;
        int left_thick = (hover == "left") ? hover_thick : border_thick;
        cv::rectangle(canvas, cv::Rect(left_btn_x, btn_y, btn_w, btn_h), left_color, -1);
        cv::rectangle(canvas, cv::Rect(left_btn_x, btn_y, btn_w, btn_h), left_color, left_thick);
        int left_arrow_cx = left_btn_x + btn_w / 2;
        int left_arrow_cy = btn_y + btn_h / 2 + 20;
        ft2.putText(canvas, "←", cv::Point(left_arrow_cx, left_arrow_cy), cv::Scalar(255,255,255), 3, true);

        // Right arrow (drawn OVER preview image and border)
        cv::Scalar right_color = (hover == "right") ? hover_color : border_color;
        int right_thick = (hover == "right") ? hover_thick : border_thick;
        cv::rectangle(canvas, cv::Rect(right_btn_x, btn_y, btn_w, btn_h), right_color, -1);
        cv::rectangle(canvas, cv::Rect(right_btn_x, btn_y, btn_w, btn_h), right_color, right_thick);
        int right_arrow_cx = right_btn_x + btn_w / 2;
        int right_arrow_cy = btn_y + btn_h / 2 + 20;
        ft2.putText(canvas, "→", cv::Point(right_arrow_cx, right_arrow_cy), cv::Scalar(255,255,255), 3, true);
        
        // Draw puzzle metadata (name, artist) centered below preview, always at the same vertical position
        int info_y = y_offset + thumb_h + 60; // fixed vertical position below preview area
        int info_center_x = win_w / 2;
        ft2.putText(canvas, metas[idx].name, cv::Point(info_center_x, info_y), cv::Scalar(255,255,255), 2, true);
        ft2.putText(canvas, metas[idx].artist, cv::Point(info_center_x, info_y + 50), cv::Scalar(200,200,200), 1, true);
        
        // Solved/Unsolved indicator in bottom-left
        std::string puzzle_key = metas[idx].name + "|" + metas[idx].artist;
        bool solved = false;
        auto it = solved_map.find(puzzle_key);
        if (it != solved_map.end()) solved = it->second;
        std::string solved_text = solved ? "Solved" : "Unsolved";
        cv::Scalar solved_color = solved ? cv::Scalar(0,255,0) : cv::Scalar(0,0,255);
        int solved_font_height = 32;
        int solved_x = 30, solved_y = win_h - 30;
        ft2.putText(canvas, solved_text, cv::Point(solved_x, solved_y), solved_color, 2);
        
        // Difficulty color and position (bottom-right, ensure it fits)
        cv::Scalar diff_color(0,255,0); // Easy: green
        std::string diff = metas[idx].difficulty;
        if (diff == "Medium" || diff == "medium") diff_color = cv::Scalar(0,255,255); // yellow
        else if (diff == "Hard" || diff == "hard") diff_color = cv::Scalar(0,0,255); // red
        int diff_font_height = 32;
        int diff_margin_x = 40, diff_margin_y = 30;
        int baseline = 0;
        int font = cv::FONT_HERSHEY_SIMPLEX;
        double scale = diff_font_height / 32.0;
        cv::Size diff_sz = cv::getTextSize(diff, font, scale, 2, &baseline);
        int diff_x = win_w - diff_sz.width - diff_margin_x;
        int diff_y = win_h - diff_margin_y;
        ft2.putText(canvas, diff, cv::Point(diff_x, diff_y), diff_color, 2);
        cv::imshow(WIN_NAME, canvas);
        selected = -1;
        int nav_dir = 0;
        last_hover = hover;

        // Mouse callback to handle clicks and hover
        auto mouse_cb = [](int event, int x, int y, int flags, void* userdata) {
            PageClickParams* p = static_cast<PageClickParams*>(userdata);
            std::string* hover = (std::string*)(p+1);
            std::string new_hover = "none";
            if (x >= p->left_btn_x && x < p->left_btn_x + p->btn_w && y >= p->btn_y && y < p->btn_y + p->btn_h) {
                new_hover = "left";
            } else if (x >= p->right_btn_x && x < p->right_btn_x + p->btn_w && y >= p->btn_y && y < p->btn_y + p->btn_h) {
                new_hover = "right";
            } else if (x >= p->img_x && x < p->img_x + p->draw_w && y >= p->img_y && y < p->img_y + p->draw_h) {
                new_hover = "image";
            }
            if (event == cv::EVENT_MOUSEMOVE) {
                if (*hover != new_hover) *hover = new_hover;
            }
            if (event != cv::EVENT_LBUTTONDOWN) return;
            if (new_hover == "left") {
                if (p->page > 0) *p->nav_dir = -1;
                return;
            }
            if (new_hover == "right") {
                if (p->page < p->total_pages-1) *p->nav_dir = 1;
                return;
            }
            if (new_hover == "image") {
                *p->selected = p->page;
            }
        };

        // Use a shared hover pointer for both main loop and callback
        alignas(std::max_align_t) char cb_data[sizeof(PageClickParams) + sizeof(std::string*)];
        PageClickParams cb_params{img_x, img_y, draw_w, draw_h, btn_w, btn_h, btn_y, left_btn_x, right_btn_x, win_w, win_h, idx, total_pages, &selected, &nav_dir};
        memcpy(cb_data, &cb_params, sizeof(PageClickParams));
        std::string** hover_ptr_ptr = reinterpret_cast<std::string**>(cb_data + sizeof(PageClickParams));
        *hover_ptr_ptr = &hover;
        
        cv::setMouseCallback(WIN_NAME, [](int event, int x, int y, int flags, void* userdata) {
            PageClickParams* p = static_cast<PageClickParams*>(userdata);
            std::string* hover = *(std::string**)(p+1);
            std::string new_hover = "none";
            if (x >= p->left_btn_x && x < p->left_btn_x + p->btn_w && y >= p->btn_y && y < p->btn_y + p->btn_h) {
                new_hover = "left";
            } else if (x >= p->right_btn_x && x < p->right_btn_x + p->btn_w && y >= p->btn_y && y < p->btn_y + p->btn_h) {
                new_hover = "right";
            } else if (x >= p->img_x && x < p->img_x + p->draw_w && y >= p->img_y && y < p->img_y + p->draw_h) {
                new_hover = "image";
            }
            if (event == cv::EVENT_MOUSEMOVE) {
                if (*hover != new_hover) *hover = new_hover;
            }
            if (event != cv::EVENT_LBUTTONDOWN) return;
            if (new_hover == "left") {
                if (p->page > 0) *p->nav_dir = -1;
                return;
            }
            if (new_hover == "right") {
                if (p->page < p->total_pages-1) *p->nav_dir = 1;
                return;
            }
            if (new_hover == "image") {
                *p->selected = p->page;
            }
        }, cb_data);

        // Wait for input or hover change
        while (selected == -1 && nav_dir == 0) {
            int key = cv::waitKey(30);
            
            // Exit if window was closed
            if (cv::getWindowProperty(WIN_NAME, cv::WND_PROP_VISIBLE) < 1 || key == 27) {
                return -1;
            }
            if (hover != last_hover) break;
        }

        cv::setMouseCallback(WIN_NAME, nullptr, nullptr);
        
        if (selected != -1) return selected;
        if (nav_dir == -1 && idx > 0) idx--;
        if (nav_dir == 1 && idx < total_pages-1) idx++;
    }
}


// --- Puzzle metadata and image loading ---
std::vector<PuzzleMeta> load_puzzle_meta(const std::string& json_path) {
    std::ifstream f(json_path);
    nlohmann::json j;
    f >> j;
    std::vector<PuzzleMeta> puzzles;
    for (const auto& entry : j["puzzles"]) {
        puzzles.push_back({
            entry["name"],
            entry["artist"],
            entry.value("difficulty", "medium"),
            entry["offset"],
            entry["length"],
            entry.value("block_size", 3)
        });
    }
    return puzzles;
}

// Load puzzle state from file (returns empty json if not found)
nlohmann::json load_puzzle_state() {
    std::ifstream f(PUZZLE_STATE_FILE);
    if (!f) return nlohmann::json::object();

    nlohmann::json j;
    f >> j;
    return j;
}

// Save puzzle state to file
void save_puzzle_state(const nlohmann::json& state) {
    std::ofstream f(PUZZLE_STATE_FILE);
    f << state.dump(4);
}

// Load and decompress a puzzle image from puzzles.dat given meta info
cv::Mat load_puzzle_image(const std::string& dat_path, const PuzzleMeta& meta) {
    std::ifstream dat(dat_path, std::ios::binary);
    dat.seekg(meta.offset);
    std::vector<uchar> compressed(meta.length);
    dat.read(reinterpret_cast<char*>(compressed.data()), meta.length);

    // Decompress with zlib
    uLongf uncompressed_size = meta.length * 20; // guess, will resize if needed
    std::vector<uchar> uncompressed(uncompressed_size);
    int z_result = uncompress(uncompressed.data(), &uncompressed_size, compressed.data(), meta.length);

    // Try a bigger buffer
    if (z_result == Z_BUF_ERROR) {
        uncompressed_size = meta.length * 100;
        uncompressed.resize(uncompressed_size);
        z_result = uncompress(uncompressed.data(), &uncompressed_size, compressed.data(), meta.length);
    }

    if (z_result != Z_OK) {
        std::cerr << "Decompression failed for puzzle: " << meta.name << std::endl;
        return cv::Mat();
    }

    uncompressed.resize(uncompressed_size);
    return cv::imdecode(uncompressed, cv::IMREAD_COLOR);
}


// --- Binary puzzle state persistence ---
// Format: [int32_t last_page][uint32_t num_entries][entries...]
// Each entry: [uint16_t key_len][key bytes][uint8_t solved]
void save_puzzle_state_bin(const std::map<std::string, bool>& solved_map, int last_page) {
    std::ofstream f("res/puzzle_state.bin", std::ios::binary | std::ios::trunc);
    if (!f) return;

    int32_t lp = static_cast<int32_t>(last_page);
    uint32_t n = static_cast<uint32_t>(solved_map.size());
    f.write(reinterpret_cast<const char*>(&lp), sizeof(lp));
    f.write(reinterpret_cast<const char*>(&n), sizeof(n));

    for (const auto& [key, solved] : solved_map) {
        uint16_t klen = static_cast<uint16_t>(key.size());
        f.write(reinterpret_cast<const char*>(&klen), sizeof(klen));
        f.write(key.data(), klen);
        uint8_t sval = solved ? 1 : 0;
        f.write(reinterpret_cast<const char*>(&sval), sizeof(sval));
    }
}

void load_puzzle_state_bin(std::map<std::string, bool>& solved_map, int& last_page) {
    std::ifstream f("res/puzzle_state.bin", std::ios::binary);
    solved_map.clear();
    last_page = 0;
    if (!f) return;

    int32_t lp = 0;
    uint32_t n = 0;
    f.read(reinterpret_cast<char*>(&lp), sizeof(lp));
    f.read(reinterpret_cast<char*>(&n), sizeof(n));
    last_page = lp;

    for (uint32_t i = 0; i < n; ++i) {
        uint16_t klen = 0;
        f.read(reinterpret_cast<char*>(&klen), sizeof(klen));
        std::string key(klen, '\0');
        f.read(&key[0], klen);
        uint8_t sval = 0;
        f.read(reinterpret_cast<char*>(&sval), sizeof(sval));
        solved_map[key] = (sval != 0);
    }
}


// -- Callback functions ---

// Mouse callback for wait_for_mouse_click
void wait_click_callback(int event, int, int, int, void* userdata) {
    if (event == cv::EVENT_LBUTTONDOWN) static_cast<ClickState*>(userdata)->clicked = true;
}

// Mouse callback for landing page selection
void landing_page_mouse_callback(int event, int mx, int my, int, void* userdata) {
    if (event != cv::EVENT_LBUTTONDOWN) return;
    MouseClickParams* p = static_cast<MouseClickParams*>(userdata);

    int grid = p->grid;
    int gx = (mx - p->margin) / (p->thumb_size + p->margin);
    int gy = (my - p->margin) / (p->thumb_size + p->margin);
    int x = p->margin + gx * (p->thumb_size + p->margin);
    int y = p->margin + gy * (p->thumb_size + p->margin);

    // Check if click is on the Start button
    if (gx >= 0 && gx < grid && gy >= 0 && gy < grid) {
        int idx = gy * grid + gx;
        int pick = p->start + idx;
        if (pick < p->end) {
            int btn_y = y + p->thumb_size + 8;
            if (mx >= x && mx < x + p->thumb_size && my >= btn_y && my < btn_y + 32) {
                *(p->selected) = pick;
            }
        }
    }
}


// --- Main Program ---

int main() {
    // Load puzzle metadata and images
    auto metas = load_puzzle_meta(PUZZLE_META_FILE);
    std::vector<cv::Mat> previews;
    for (const auto& meta : metas) {
        cv::Mat img = load_puzzle_image(PUZZLE_DATA_FILE, meta);
        if (img.empty()) {
            std::cerr << "Failed to load preview for: " << meta.name << std::endl;
            previews.push_back(cv::Mat(128,128,CV_8UC3,cv::Scalar(50,50,50)));
        }
        else {
            previews.push_back(img);
        }
    }

    if (previews.empty()) {
        std::cerr << "No puzzles found in " << PUZZLE_DATA_FILE << '\n';
        return 1;
    }

    // Binary puzzle state
    std::map<std::string, bool> solved_map;
    int last_page = 0;
    load_puzzle_state_bin(solved_map, last_page);

    while (true) {
        int pick = show_main_menu(metas, previews, last_page, solved_map);
        if (pick < 0) break;
        last_page = pick;
        save_puzzle_state_bin(solved_map, last_page);
        const auto& meta = metas[pick];
        std::string puzzle_key = meta.name + "|" + meta.artist;
        bool solved = false;
        auto it = solved_map.find(puzzle_key);
        if (it != solved_map.end()) solved = it->second;
        cv::Mat image_original = load_puzzle_image(PUZZLE_DATA_FILE, meta);
        if (image_original.empty()) continue;
        
        int num_blocks_x = meta.block_size, num_blocks_y = meta.block_size;
        int padded_cols, padded_rows, block_width, block_height;
        cv::Mat padded = pad_image_to_blocks(image_original, num_blocks_x, num_blocks_y, padded_cols, padded_rows, block_width, block_height);
        const int cols = padded_cols, rows = padded_rows;

        std::vector<cv::Rect> blocks;
        for (int y = 0; y < rows; y += block_width) {
            for (int x = 0; x < cols; x += block_width) {
                if (!(x == 0 && y == 0)) {
                    blocks.emplace_back(x, y, block_width, block_height);
                }
            }
        }

        // --- Shuffle puzzle on start button click ---
        int total_blocks = num_blocks_x * num_blocks_y;
        std::vector<int> perm(total_blocks);
        std::iota(perm.begin(), perm.end(), 0);
        int empty_idx = 0;
        cv::RNG rng((unsigned)cv::getTickCount());
        int min_challenge = std::max(6, 2 * (total_blocks - 1)), challenge = 0;

        do {
            std::iota(perm.begin(), perm.end(), 0);
            empty_idx = 0;
            int shuffle_moves = rng.uniform(30, 100);
            for (int i = 0; i < shuffle_moves; ++i) {
                std::vector<int> neighbors;
                int ex = empty_idx % num_blocks_x, ey = empty_idx / num_blocks_x;

                if (ex > 0)
                    neighbors.push_back(empty_idx - 1);
                if (ex < num_blocks_x - 1)
                    neighbors.push_back(empty_idx + 1);
                if (ey > 0)
                    neighbors.push_back(empty_idx - num_blocks_x);
                if (ey < num_blocks_y - 1)
                    neighbors.push_back(empty_idx + num_blocks_x);

                if (i > 0 && neighbors.size() > 1) {
                    int prev = perm[empty_idx];
                    neighbors.erase(std::remove_if(neighbors.begin(), neighbors.end(), [&](int n){ 
                        return perm[n] == 0;
                    }), neighbors.end());
                }

                if (neighbors.empty())
                    break;

                int nidx = neighbors[rng.uniform(0, (int)neighbors.size())];
                std::swap(perm[empty_idx], perm[nidx]);
                empty_idx = nidx;
            }
            challenge = permutation_manhattan_distance(perm, num_blocks_x, num_blocks_y);
        }
        while (challenge < min_challenge);

        // Show start screen and wait for click
        show_start_screen(padded, block_width, block_height);
        if (!wait_for_mouse_click(WIN_NAME)) continue;

        // Fill the altered image from the permutation
        cv::Mat image_altered = padded.clone();
        fill_image_from_permutation(image_altered, padded, perm, num_blocks_x, num_blocks_y, block_width, block_height);

        // Define and initialize empty_x and empty_y to the position of the empty block (index 0)
        empty_idx = static_cast<int>(std::find(perm.begin(), perm.end(), 0) - perm.begin());
        int empty_x = (empty_idx % num_blocks_x) * block_width;
        int empty_y = (empty_idx / num_blocks_x) * block_height;

        MouseState mouse_state{block_width, block_height, cols, rows, empty_x, empty_y, image_altered, padded, blocks};
        mouse_state.perm = &perm;
        mouse_state.solved = solved;

        // Use solved_map and puzzle_key for state
        mouse_state.puzzle_state = nullptr; // not used
        mouse_state.puzzle_key = puzzle_key;
        cv::setMouseCallback(WIN_NAME, on_mouse, &mouse_state);
        cv::imshow(WIN_NAME, image_altered);

        // Main loop: check for victory after every move, return to start page on win
        while (true) {
            int key = cv::waitKey(30);
            if (cv::getWindowProperty(WIN_NAME, cv::WND_PROP_VISIBLE) < 1) {
                return 0;
            }
            if (key == 27) break;
            if (mouse_state.solved && key == cv::EVENT_LBUTTONDOWN) break;

            // Check for solved state and update binary file
            if (mouse_state.perm && is_solved(*mouse_state.perm) && !solved) {
                solved = true;
                solved_map[puzzle_key] = true;
                save_puzzle_state_bin(solved_map, last_page);
                mouse_state.solved = true;

                // Show solved overlay (reuse existing code)
                mouse_state.image_altered = mouse_state.image_original.clone();
                std::string line1 = "Finito!";
                std::string line2 = "Press Escape to return";
                int font_height1 = 56, font_height2 = 36;
                int thickness = 2;
                int font = cv::FONT_HERSHEY_SIMPLEX;
                int baseline = 0;
                double scale1 = font_height1 / 32.0, scale2 = font_height2 / 32.0;
                cv::Size sz1 = cv::getTextSize(line1, font, scale1, thickness, &baseline);
                cv::Size sz2 = cv::getTextSize(line2, font, scale2, thickness, &baseline);
                int cx = mouse_state.image_altered.cols / 2;
                int cy = mouse_state.image_altered.rows / 2 - sz1.height;
                int box_w = std::max(sz1.width, sz2.width) + 60;
                int box_h = sz1.height + sz2.height + 80;
                int box_x = cx - box_w/2;
                int box_y = cy - 50;
                cv::Mat roi = mouse_state.image_altered(cv::Rect(box_x, box_y, box_w, box_h));
                cv::Mat overlay;
                roi.copyTo(overlay);
                cv::rectangle(overlay, cv::Rect(0,0,box_w,box_h), cv::Scalar(0,0,0,180), -1);
                cv::addWeighted(overlay, 0.7, roi, 0.3, 0, roi);
                ft2.putText(mouse_state.image_altered, line1, cv::Point(cx, cy), cv::Scalar(255,255,80), 2, true);
                ft2.putText(mouse_state.image_altered, line2, cv::Point(cx, cy + sz1.height + 40), cv::Scalar(255,255,255), 2, true);
                cv::imshow(WIN_NAME, mouse_state.image_altered);
            }
        }
        if (cv::getWindowProperty(WIN_NAME, cv::WND_PROP_VISIBLE) >= 1) {
            cv::destroyWindow(WIN_NAME);
        }
    }
    cv::destroyAllWindows();
    return 0;
}
