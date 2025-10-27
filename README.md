# ReVision

Sliding puzzle game.

<p align="center">
   <img src="media/revision.gif" alt="Demo Animation" width="720"/>
</p>

A visually polished, robust sliding puzzle game implemented in C++ with OpenCV. The game features a collection of famous artwork puzzles, Unicode text rendering, persistent progress tracking, and a modern, user-friendly interface.

## Features

- **Sliding Puzzles:** Generates sliding puzzle from an image, subdivided into a 3x3, 4x4, 5x5, etc. grid.
- **Main Menu:** Browse puzzles with previews, artist/title info, and a clear "Solved" (green) or "Unsolved" (red) indicator for each puzzle.
- **Unicode Text:** All text (including diacriticsm, Cyrillic, and Japanese) is rendered crisply using FreeType.
- **Aspect Ratio Handling:** Puzzles and UI scale gracefully to the window size.
- **Persistent Progress:** Solved puzzle state and last selected puzzle are saved in a compact binary file.

## User Guide

1. Main Menu
   - Browse puzzles using the navigation buttons.
   - Each puzzle shows a preview, title, artist, and a colored indicator:
     - **Green "Solved"**: You have completed this puzzle.
     - **Red "Unsolved"**: Not yet solved.
   - Click a puzzle to start.
2. Solving a Puzzle
   - Click or drag tiles to slide them into the empty space.
   - The goal is to restore the original image.
3. Progress Tracking
   - Your solved puzzles and last page are saved automatically and are restored on next launch.
   - Deleting `res/puzzle_state.bin` resets all progress.

## Puzzle Data

To add or regenerate puzzles, use the provided Python script:

1. Place your source images in `src/make_puzzles/`. Their format should be similar to:  
    `Artist_Full_Name-English_Painting_Name_Without_Diacritics.jpg`.
2. Run the `gen_puzzle_data.py` script to generate or update `res/puzzles.json` and `res/puzzles.dat` based on all the `jpg` and `png` files in the make_puzzle_data directory.
    - `puzzles.json` contains metadata for each puzzle (title, artist, image offsets, etc).
    - `puzzles.dat` contains the packed, resized PNG images used by the game.

## Build Requirements

- `OpenCV 4.5`.
- `FreeType` for Unicode text rendering.
- `Python 3` with `Pillow` for generating puzzle data.

## License

This software is licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html). See [COPYING](COPYING) for details.
