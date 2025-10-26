#include "puzzle.hpp"

#include "app.hpp"
#include "ft2.hpp"
#include "main.hpp"
#include "util.hpp"

#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <numeric>
#include <iostream>

#include <zlib.h>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>


std::vector<PuzzleMeta> Puzzle::load_meta(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f) {
        throw std::runtime_error("Failed to open JSON file: " + json_path);
    }

    nlohmann::json j;
    f >> j;

    std::vector<PuzzleMeta> puzzles;
    for (const auto& entry : j.at("puzzles")) {
        puzzles.emplace_back(
            entry.at("name").get<std::string>(),
            entry.at("artist").get<std::string>(),
            entry.value("difficulty", "medium"),
            entry.at("offset").get<int>(),
            entry.at("length").get<int>(),
            entry.value("block_size", 3)
        );
    }
    return puzzles;
}

cv::Mat Puzzle::load_image(const std::string& dat_path, const PuzzleMeta& meta) {
    std::ifstream dat(dat_path, std::ios::binary);
    if (!dat) {
        std::cerr << "Failed to open data file: " << dat_path << std::endl;
        return cv::Mat();
    }

    dat.seekg(meta.offset);
    std::vector<uchar> compressed(meta.length);
    if (!dat.read(reinterpret_cast<char*>(compressed.data()), meta.length)) {
        std::cerr << "Failed to read compressed data for puzzle: " << meta.name << std::endl;
        return cv::Mat();
    }

    // Try decompressing with increasing buffer size if needed
    uLongf uncompressed_size = meta.length * 20;
    std::vector<uchar> uncompressed;

    for (int attempt = 0; attempt < 3; ++attempt) {
        uncompressed.resize(uncompressed_size);
        int z_result = uncompress(uncompressed.data(), &uncompressed_size, compressed.data(), meta.length);

        if (z_result == Z_OK) {
            uncompressed.resize(uncompressed_size);
            return cv::imdecode(uncompressed, cv::IMREAD_COLOR);
        }
        uncompressed_size *= 2;
    }

    std::cerr << "Decompression failed for puzzle: " << meta.name << std::endl;
    return cv::Mat();
}

Puzzle::Puzzle(const PuzzleMeta& meta, const std::map<std::string, bool>& solved_map) : session{} {
    session.meta = meta;
    session.puzzle_key = meta.name + "|" + meta.artist;
    session.solved = solved_map.count(session.puzzle_key) ? solved_map.at(session.puzzle_key) : false;

    cv::Mat image_original = load_image(PUZZLE_DATA_FILE, meta);
    if (image_original.empty()) {
        throw std::runtime_error("Image load failed");
    }
    session.image_original = image_original; // Store in session

    int num_blocks = meta.block_size;
    session.layout = make_puzzle_layout(image_original, num_blocks, num_blocks);
    session.blocks = make_blocks(session.layout.cols, session.layout.rows, session.layout.block_width, session.layout.block_height);

    int total_blocks = num_blocks * num_blocks;
    session.perm.resize(total_blocks);
    shuffle_permutation(session.perm, num_blocks, num_blocks, session.empty_idx, std::max(6, 2 * (total_blocks - 1)));
}

// Add static mouse callback for puzzle sliding
void Puzzle::on_mouse(int event, int x, int y, int /*flags*/, void* userdata) {
    if (event != cv::EVENT_LBUTTONDOWN) { 
        return;
    }

    MouseState* state = static_cast<MouseState*>(userdata);
    if (!state || state->solved) { 
        return;
    }

    int bx = (x / state->block_width) * state->block_width;
    int by = (y / state->block_height) * state->block_height;

    // Only allow clicking on blocks, not the empty block
    if (bx == state->empty_x && by == state->empty_y) {
        return;
    }

    // Try to swap
    Puzzle::swap_block(bx, by, *state);

    // Redraw
    cv::imshow(WIN_NAME, state->image_altered);
}

void Puzzle::play(std::map<std::string, bool>& solved_map, int& last_page, App* app_cb_userdata) {
    int num_blocks = session.meta.block_size;
    app_cb_userdata->show_start_screen(session.layout.padded, session.layout.block_width, session.layout.block_height);

    if (!app_cb_userdata->wait_for_mouse_click(WIN_NAME)) {
        return;
    }

    cv::Mat image_altered = session.layout.padded.clone();
    fill_image_from_permutation(image_altered, session.layout.padded, session.perm, num_blocks, num_blocks, session.layout.block_width, session.layout.block_height);

    session.empty_idx = static_cast<int>(std::distance(session.perm.begin(), std::find(session.perm.begin(), session.perm.end(), 0)));
    int empty_x = (session.empty_idx % num_blocks) * session.layout.block_width;
    int empty_y = (session.empty_idx / num_blocks) * session.layout.block_height;

    MouseState mouse_state{
        session.layout.block_width,
        session.layout.block_height,
        session.layout.cols,
        session.layout.rows,
        empty_x,
        empty_y,
        image_altered,
        session.layout.padded,
        session.blocks,
        &session.perm,
        session.solved,
        session.puzzle_key
    };

    // Use Puzzle's static callback and pass MouseState as userdata
    cv::namedWindow(WIN_NAME, cv::WINDOW_AUTOSIZE);
    cv::resizeWindow(WIN_NAME, image_altered.cols, image_altered.rows);
    cv::setMouseCallback(WIN_NAME, Puzzle::on_mouse, &mouse_state);
    cv::imshow(WIN_NAME, image_altered);

    while (true) {
        int key = cv::waitKey(1);
        if (cv::getWindowProperty(WIN_NAME, cv::WND_PROP_VISIBLE) < 1) {
            return;
        }

        if (key == 27 || (mouse_state.solved && key == cv::EVENT_LBUTTONDOWN)) {
            break;
        }

        if (mouse_state.perm && is_solved(*mouse_state.perm) && !session.solved) {
            app_cb_userdata->handle_puzzle_solved(mouse_state, solved_map, last_page);
            session.solved = true;
            mouse_state.solved = true;
        }
    }

    if (cv::getWindowProperty(WIN_NAME, cv::WND_PROP_VISIBLE) >= 1) {
        cv::destroyWindow(WIN_NAME);
    }
}

cv::Mat Puzzle::pad_image_to_blocks(const cv::Mat& img, int num_blocks_x, int num_blocks_y, int& padded_cols, int& padded_rows, int& block_width, int& block_height) {
    block_width = (img.cols + num_blocks_x - 1) / num_blocks_x;
    block_height = (img.rows + num_blocks_y - 1) / num_blocks_y;
    padded_cols = block_width * num_blocks_x;
    padded_rows = block_height * num_blocks_y;

    cv::Mat padded;
    cv::copyMakeBorder(img, padded, 0, padded_rows - img.rows, 0, padded_cols - img.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0));
    return padded;
}

void Puzzle::fill_image_from_permutation(cv::Mat& image_altered, const cv::Mat& image_original, const std::vector<int>& perm, int num_blocks_x, int num_blocks_y, int block_width, int block_height) {
    int idx = 0;

    for (int by = 0; by < num_blocks_y; ++by) {
        for (int bx = 0; bx < num_blocks_x; ++bx, ++idx) {
            int src_idx = perm[idx];
            int dst_x = bx * block_width, dst_y = by * block_height;
            int dst_w = std::min(block_width, image_altered.cols - dst_x);
            int dst_h = std::min(block_height, image_altered.rows - dst_y);
            cv::Rect dst_rect(dst_x, dst_y, dst_w, dst_h);

            if (src_idx == 0) {
                image_altered(dst_rect).setTo(cv::Scalar(0,0,0));
                continue;
            }

            int src_x = (src_idx % num_blocks_x) * block_width;
            int src_y = (src_idx / num_blocks_x) * block_height;
            int src_w = std::min(block_width, image_original.cols - src_x);
            int src_h = std::min(block_height, image_original.rows - src_y);
            int copy_w = std::min(src_w, dst_w);
            int copy_h = std::min(src_h, dst_h);

            if (copy_w > 0 && copy_h > 0) {
                cv::Rect src_rect(src_x, src_y, copy_w, copy_h);
                cv::Rect dst_rect_adj(dst_x, dst_y, copy_w, copy_h);
                image_original(src_rect).copyTo(image_altered(dst_rect_adj));
            }
        }
    }
}

int Puzzle::permutation_manhattan_distance(const std::vector<int>& perm, int num_blocks_x, int num_blocks_y) {
    int dist = 0;
    for (size_t idx = 0; idx < perm.size(); ++idx) {
        if (perm[idx] == 0) {
            continue;
        }

        dist += std::abs(static_cast<int>(idx % num_blocks_x) - perm[idx] % num_blocks_x);
        dist += std::abs(static_cast<int>(idx / num_blocks_x) - perm[idx] / num_blocks_x);
    }
    return dist;
}

void Puzzle::swap_block(int x, int y, MouseState &state) {
    if (!Util::is_adjacent(x, y, state.empty_x, state.empty_y, state.block_width, state.block_height)) {
        return;
    }

    int copy_w = std::min(state.block_width, std::min(state.cols - x, state.cols - state.empty_x));
    int copy_h = std::min(state.block_height, std::min(state.rows - y, state.rows - state.empty_y));
    cv::Rect from_rect(x, y, copy_w, copy_h), to_rect(state.empty_x, state.empty_y, copy_w, copy_h);

    if (from_rect.size() != to_rect.size() || copy_w <= 0 || copy_h <= 0) {
        return;
    }

    cv::Mat temp;
    state.image_altered(from_rect).copyTo(temp);
    state.image_altered(to_rect).copyTo(state.image_altered(from_rect));
    temp.copyTo(state.image_altered(to_rect));

    if (state.perm) {
        int num_blocks_x = state.cols / state.block_width;
        int from_idx = (y / state.block_height) * num_blocks_x + (x / state.block_width);
        int to_idx = (state.empty_y / state.block_height) * num_blocks_x + (state.empty_x / state.block_width);
        std::swap((*state.perm)[from_idx], (*state.perm)[to_idx]);
    }

    state.empty_x = x; state.empty_y = y;
}

bool Puzzle::is_solved(const std::vector<int>& perm) {
    for (size_t i = 1; i < perm.size(); ++i) {
        if ((int)i != perm[i]) {
            return false;
        }
    }
    return true;
}

std::vector<int> Puzzle::get_empty_neighbors(int empty_idx, int num_blocks_x, int num_blocks_y, const std::vector<int>& perm, bool avoid_zero) {
    std::vector<int> neighbors;
    int ex = empty_idx % num_blocks_x;
    int ey = empty_idx / num_blocks_x;

    // Lambda to add neighbor if in bounds and (optionally) not zero
    auto try_add = [&](int nx, int ny) {
        if (nx >= 0 && nx < num_blocks_x && ny >= 0 && ny < num_blocks_y) {
            int nidx = ny * num_blocks_x + nx;
            if (!avoid_zero || perm[nidx] != 0) {
                neighbors.push_back(nidx);
            }
        }
    };

    try_add(ex - 1, ey); // left
    try_add(ex + 1, ey); // right
    try_add(ex, ey - 1); // up
    try_add(ex, ey + 1); // down

    return neighbors;
}

void Puzzle::shuffle_permutation(std::vector<int>& perm, int num_blocks_x, int num_blocks_y, int& empty_idx, int min_challenge) {
    int total_blocks = num_blocks_x * num_blocks_y;
    cv::RNG rng((unsigned)cv::getTickCount());
    int challenge = 0;

    do {
        std::iota(perm.begin(), perm.end(), 0);
        empty_idx = 0;
        int shuffle_moves = rng.uniform(30, 100);

        for (int i = 0; i < shuffle_moves; ++i) {
            bool avoid_zero = (i > 0);
            auto neighbors = get_empty_neighbors(empty_idx, num_blocks_x, num_blocks_y, perm, avoid_zero);
            if (neighbors.empty()) {
                break;
            }

            int nidx = neighbors[rng.uniform(0, static_cast<int>(neighbors.size()))];
            std::swap(perm[empty_idx], perm[nidx]);
            empty_idx = nidx;
        }

        challenge = permutation_manhattan_distance(perm, num_blocks_x, num_blocks_y);
    }
    while (challenge < min_challenge);
}

std::vector<cv::Rect> Puzzle::make_blocks(int cols, int rows, int block_width, int block_height) {
    std::vector<cv::Rect> blocks;

    for (int y = 0; y < rows; y += block_height) {
        for (int x = 0; x < cols; x += block_width) {
            if (!(x == 0 && y == 0)) {
                blocks.emplace_back(x, y, block_width, block_height);
            }
        }
    }
    
    return blocks;
}

PuzzleLayout Puzzle::make_puzzle_layout(const cv::Mat& image, int num_blocks_x, int num_blocks_y) {
    PuzzleLayout layout;
    layout.padded = pad_image_to_blocks(image, 
        num_blocks_x, num_blocks_y, 
        layout.cols, layout.rows, 
        layout.block_width, layout.block_height);
    return layout;
}
