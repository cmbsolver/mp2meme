# mp2meme

A C++ command-line tool that takes an MP4 video, trims it, overlays caption text, and generates an animated GIF meme.

---

## Requirements

- **C++ Compiler**: A compiler with C++17 support (e.g., `g++` or `clang++`)
- **Build System**: `make` or `CMake` (version 3.10+)
- **FFmpeg**: `ffmpeg` and `ffprobe` must be installed and accessible in your `PATH` (used at runtime for video processing and rendering)

---

## Compilation & Installation

You can build and install `mp2meme` using either `make` (with optional `./configure`) or `CMake`.

### Option 1: Using `./configure` and `make` (Recommended)

1. **Configure the build:**
   ```sh
   ./configure
   ```
   *Optional configuration flags:*
   - `--prefix=<path>`: Installation prefix (default: `~/.local`)
   - `--bindir=<path>`: Binary directory (default: `PREFIX/bin`)
   - `--cxx=<compiler>`: C++ compiler (default: `g++`)
   - `--cxxflags=<flags>`: Compiler flags (default: `-O2 -std=c++17 -Wall`)

2. **Compile:**
   ```sh
   make
   ```

3. **Install (optional):**
   ```sh
   make install
   ```

4. **Other targets:**
   - `make uninstall`: Remove the installed binary.
   - `make clean`: Remove build objects and binary.
   - `make distclean`: Remove build objects, binary, and generated `config.mk`.

---

### Option 2: Using CMake

1. **Configure:**
   ```sh
   cmake -B build
   ```

2. **Build:**
   ```sh
   cmake --build build
   ```

3. **Install (optional):**
   ```sh
   cmake --install build
   ```

---

## Usage

```sh
mp2meme [options]
```

### Options

| Option | Description |
| :--- | :--- |
| `-i, --input <file>` | Path to input MP4 video file (*required*) |
| `-o, --output <file>` | Path to output GIF file (*required*) |
| `-s, --start <time>` | Start time in seconds or `HH:MM:SS` / `MM:SS` (*required*) |
| `-e, --end <time>` | End time in seconds or `HH:MM:SS` / `MM:SS` (*required*) |
| `-t, --text <text>` | Text overlay (position determined by `--pos`, default: bottom) |
| `--top <text>` | Text overlay at the top of the video |
| `--bottom <text>` | Text overlay at the bottom of the video |
| `-p, --pos <top\|bottom\|both>` | Placement for `-t`: `top`, `bottom`, or `both` (default: `bottom`) |
| `--font-size <size>` | Font size in pixels (default: auto-calculated based on video resolution) |
| `--font <path>` | Path to custom TTF/OTF font file (optional) |
| `--fps <fps>` | Output GIF frame rate (default: `15`) |
| `-w, --scale, --width <w>` | Scale width in pixels, preserving aspect ratio (default: original resolution) |
| `-h, --help` | Display help and usage message |

---

## Examples

- **Basic bottom-caption meme:**
  ```sh
  ./mp2meme -i input.mp4 -o output.gif -s 00:00:01 -e 00:00:05 -t "WHEN THE CODE COMPILES ON FIRST TRY"
  ```

- **Top and bottom captions:**
  ```sh
  ./mp2meme -i input.mp4 -o meme.gif -s 1.5 -e 4.5 --top "TOP TEXT" --bottom "BOTTOM TEXT"
  ```

- **Custom FPS, resolution width, and font:**
  ```sh
  ./mp2meme -i input.mp4 -o meme.gif -s 0 -e 3 -t "FAST & COMPACT" --fps 12 --width 480 --font /path/to/font.ttf
  ```
