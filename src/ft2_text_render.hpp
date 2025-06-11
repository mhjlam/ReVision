#pragma once

#include <string>
#include <vector>
#include <locale>
#include <codecvt>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <opencv2/opencv.hpp>


// Minimal UTF-8 to Unicode codepoint decoder
inline std::vector<uint32_t> utf8_to_codepoints(const std::string& utf8) {
    std::vector<uint32_t> codepoints;
    size_t i = 0;
    while (i < utf8.size()) {
        uint32_t cp = 0;
        unsigned char c = utf8[i];
        if (c < 0x80) { cp = c; i += 1; }
        else if ((c & 0xE0) == 0xC0) { cp = ((c & 0x1F) << 6) | (utf8[i+1] & 0x3F); i += 2; }
        else if ((c & 0xF0) == 0xE0) { cp = ((c & 0x0F) << 12) | ((utf8[i+1] & 0x3F) << 6) | (utf8[i+2] & 0x3F); i += 3; }
        else if ((c & 0xF8) == 0xF0) { cp = ((c & 0x07) << 18) | ((utf8[i+1] & 0x3F) << 12) | ((utf8[i+2] & 0x3F) << 6) | (utf8[i+3] & 0x3F); i += 4; }
        else { i += 1; }
        codepoints.push_back(cp);
    }
    return codepoints;
}


class FT2TextRenderer {
public:
    FT2TextRenderer(const std::string& font_path, int font_height = 32) : font_height(font_height) {
        FT_Init_FreeType(&ftlib);
        FT_New_Face(ftlib, font_path.c_str(), 0, &face);
        FT_Set_Pixel_Sizes(face, 0, font_height);
    }

    ~FT2TextRenderer() {
        if (face) FT_Done_Face(face);
        if (ftlib) FT_Done_FreeType(ftlib);
    }

    // Draws UTF-8 text at baseline (org.x, org.y) in BGR color
    void putText(cv::Mat& img, const std::string& text, cv::Point org, cv::Scalar color, int thickness = 1, bool center = false) {
        auto codepoints = utf8_to_codepoints(text);
        int baseline = org.y;
        int x = org.x;

        // Optionally measure width for centering
        if (center) {
            int width = 0;
            FT_GlyphSlot slot = face->glyph;
            for (auto cp : codepoints) {
                if (FT_Load_Char(face, cp, FT_LOAD_RENDER)) continue;
                width += (slot->advance.x >> 6);
            }
            x -= width / 2;
        }

        for (auto cp : codepoints) {
            if (FT_Load_Char(face, cp, FT_LOAD_RENDER)) continue;

            FT_GlyphSlot slot = face->glyph;
            int y = baseline - slot->bitmap_top;
            int w = slot->bitmap.width, h = slot->bitmap.rows;
            
            for (int row = 0; row < h; ++row) {
                for (int col = 0; col < w; ++col) {
                    int px = x + slot->bitmap_left + col;
                    int py = y + row;

                    if (px < 0 || py < 0 || px >= img.cols || py >= img.rows) {
                        continue;
                    }

                    uchar alpha = slot->bitmap.buffer[row * w + col];
                    for (int c = 0; c < img.channels(); ++c) {
                        img.at<cv::Vec3b>(py, px)[c] = (uchar)((img.at<cv::Vec3b>(py, px)[c] * (255 - alpha) + color[c] * alpha) / 255);
                    }
                }
            }
            x += (slot->advance.x >> 6);
        }
    }

private:
    FT_Library ftlib = nullptr;
    FT_Face face = nullptr;
    int font_height = 32;

};
