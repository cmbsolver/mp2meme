#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <memory>
#include <array>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>

struct Options {
    std::string input_file;
    std::string output_file;
    std::string text;
    std::string top_text;
    std::string bottom_text;
    std::string position = "bottom"; // "top", "bottom", or "both"
    std::string start_time;
    std::string end_time;
    int font_size = 0; // 0 means auto-calculate
    std::string font_file = "";
    int fps = 15;
    int scale_width = -1; // -1 means preserve/original
};

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n\n"
              << "Options:\n"
              << "  -i, --input <file>        Input MP4 video file path (required)\n"
              << "  -o, --output <file>       Output GIF file path (required)\n"
              << "  -t, --text <text>         Text to overlay on video (uses position, default: bottom)\n"
              << "      --top <text>          Text to overlay at the top of the video\n"
              << "      --bottom <text>       Text to overlay at the bottom of the video\n"
              << "  -p, --pos <top|bottom>    Text placement: 'top', 'bottom', or 'both' (default: bottom)\n"
              << "  -s, --start <time>        Start time (e.g. 0, 00:00:01, 1.5) (required)\n"
              << "  -e, --end <time>          End time (e.g. 5, 00:00:05, 5.0) (required)\n"
              << "      --font-size <size>    Font size in pixels (default: auto-sized based on video dimensions)\n"
              << "      --font <path>         Path to TTF/OTF font file (optional)\n"
              << "      --fps <fps>           Frames per second for output GIF (default: 15)\n"
              << "  -w, --scale, --width <w>  Scale width in pixels, keeping aspect ratio (default: original)\n"
              << "  -h, --help                Show this help message\n";
}

bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string escape_filter_text(const std::string& input) {
    // Characters to escape in ffmpeg drawtext filter text parameter:
    // '\', ':', ''', '%', etc.
    std::string escaped;
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '\\') {
            escaped += "\\\\\\\\";
        } else if (c == '\'') {
            escaped += "'\\\\\\''";
        } else if (c == ':') {
            escaped += "\\:";
        } else if (c == '%') {
            escaped += "\\%";
        } else if (c == '[' || c == ']' || c == ';' || c == ',') {
            escaped += std::string("\\") + c;
        } else {
            escaped += c;
        }
    }
    return escaped;
}

std::string escape_shell_arg(const std::string& arg) {
    std::string res = "'";
    for (char c : arg) {
        if (c == '\'') {
            res += "'\\''";
        } else {
            res += c;
        }
    }
    res += "'";
    return res;
}

bool get_video_info(const std::string& file, int& width, int& height, double& duration) {
    width = 0;
    height = 0;
    duration = 0.0;
    std::string cmd = "ffprobe -v error -show_entries format=duration -show_entries stream=width,height,duration -of default=noprint_wrappers=1:nokey=0 " + escape_shell_arg(file);
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);

    std::stringstream ss(result);
    std::string line;
    while (std::getline(ss, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            try {
                if (key == "width" && width <= 0) {
                    width = std::stoi(val);
                } else if (key == "height" && height <= 0) {
                    height = std::stoi(val);
                } else if (key == "duration" && duration <= 0.0) {
                    duration = std::stod(val);
                }
            } catch (...) {}
        }
    }
    return (width > 0 && height > 0);
}

bool parse_time_to_seconds(const std::string& time_str, double& seconds) {
    if (time_str.empty()) return false;
    std::vector<std::string> parts;
    std::stringstream ss(time_str);
    std::string item;
    while (std::getline(ss, item, ':')) {
        parts.push_back(item);
    }

    try {
        if (parts.size() == 1) {
            size_t idx = 0;
            seconds = std::stod(parts[0], &idx);
            return idx == parts[0].size() && seconds >= 0.0;
        } else if (parts.size() == 2) {
            size_t idx1 = 0, idx2 = 0;
            double m = std::stod(parts[0], &idx1);
            double s = std::stod(parts[1], &idx2);
            if (idx1 == parts[0].size() && idx2 == parts[1].size() && m >= 0.0 && s >= 0.0 && s < 60.0) {
                seconds = m * 60.0 + s;
                return true;
            }
        } else if (parts.size() == 3) {
            size_t idx1 = 0, idx2 = 0, idx3 = 0;
            double h = std::stod(parts[0], &idx1);
            double m = std::stod(parts[1], &idx2);
            double s = std::stod(parts[2], &idx3);
            if (idx1 == parts[0].size() && idx2 == parts[1].size() && idx3 == parts[2].size()
                && h >= 0.0 && m >= 0.0 && m < 60.0 && s >= 0.0 && s < 60.0) {
                seconds = h * 3600.0 + m * 60.0 + s;
                return true;
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

int calculate_font_size(int video_width, int video_height, size_t text_len) {
    // Determine reasonable font size: roughly 8-10% of video height,
    // adjusted downwards if text is long compared to width.
    int base_size = std::max(16, video_height / 11);
    if (text_len > 0) {
        // Average char width in proportional/monospace font is approx 0.55 * font_size
        // Ensure text length fits within ~90% of video_width
        int max_fitting_size = static_cast<int>((video_width * 0.90) / (text_len * 0.58));
        if (max_fitting_size < base_size && max_fitting_size >= 12) {
            base_size = max_fitting_size;
        }
    }
    return std::max(12, base_size);
}

int main(int argc, char* argv[]) {
    Options opt;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            opt.input_file = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            opt.output_file = argv[++i];
        } else if ((arg == "-t" || arg == "--text") && i + 1 < argc) {
            opt.text = argv[++i];
        } else if ((arg == "--top" || arg == "--top-text") && i + 1 < argc) {
            opt.top_text = argv[++i];
        } else if ((arg == "--bottom" || arg == "--bottom-text") && i + 1 < argc) {
            opt.bottom_text = argv[++i];
        } else if ((arg == "-p" || arg == "--pos" || arg == "--position") && i + 1 < argc) {
            opt.position = argv[++i];
        } else if ((arg == "-s" || arg == "--start") && i + 1 < argc) {
            opt.start_time = argv[++i];
        } else if ((arg == "-e" || arg == "--end") && i + 1 < argc) {
            opt.end_time = argv[++i];
        } else if (arg == "--font-size" && i + 1 < argc) {
            opt.font_size = std::atoi(argv[++i]);
        } else if (arg == "--font" && i + 1 < argc) {
            opt.font_file = argv[++i];
        } else if (arg == "--fps" && i + 1 < argc) {
            opt.fps = std::atoi(argv[++i]);
        } else if ((arg == "-w" || arg == "--scale" || arg == "--width") && i + 1 < argc) {
            opt.scale_width = std::atoi(argv[++i]);
        } else if (opt.input_file.empty()) {
            // Positional fallback: <input> <text> <output> <start> <end> [top|bottom]
            opt.input_file = arg;
            if (i + 1 < argc && opt.text.empty()) opt.text = argv[++i];
            if (i + 1 < argc && opt.output_file.empty()) opt.output_file = argv[++i];
            if (i + 1 < argc && opt.start_time.empty()) opt.start_time = argv[++i];
            if (i + 1 < argc && opt.end_time.empty()) opt.end_time = argv[++i];
            if (i + 1 < argc) opt.position = argv[++i];
        } else {
            std::cerr << "Unknown or duplicate option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Normalize position to lowercase
    std::transform(opt.position.begin(), opt.position.end(), opt.position.begin(), ::tolower);
    if (opt.position != "top" && opt.position != "bottom" && opt.position != "both") {
        std::cerr << "Error: Position must be 'top', 'bottom', or 'both' (got: " << opt.position << ")\n";
        return 1;
    }

    // Resolve general text into top_text and/or bottom_text if not explicitly set
    if (!opt.text.empty()) {
        if (opt.position == "top" && opt.top_text.empty()) {
            opt.top_text = opt.text;
        } else if (opt.position == "bottom" && opt.bottom_text.empty()) {
            opt.bottom_text = opt.text;
        } else if (opt.position == "both") {
            if (opt.top_text.empty()) opt.top_text = opt.text;
            if (opt.bottom_text.empty()) opt.bottom_text = opt.text;
        }
    }

    if (opt.input_file.empty() || opt.output_file.empty() || opt.start_time.empty() || opt.end_time.empty() || (opt.top_text.empty() && opt.bottom_text.empty())) {
        std::cerr << "Error: Missing required arguments. Please specify input, output, start/end time, and text (-t, --top, or --bottom).\n";
        print_usage(argv[0]);
        return 1;
    }

    if (!file_exists(opt.input_file)) {
        std::cerr << "Error: Input file does not exist: " << opt.input_file << "\n";
        return 1;
    }

    // Validate and parse start and end times
    double start_sec = 0.0;
    double end_sec = 0.0;
    if (!parse_time_to_seconds(opt.start_time, start_sec)) {
        std::cerr << "Error: Invalid start time format: '" << opt.start_time << "'. Expected seconds (e.g. 10, 1.5) or HH:MM:SS / MM:SS.\n";
        return 1;
    }
    if (!parse_time_to_seconds(opt.end_time, end_sec)) {
        std::cerr << "Error: Invalid end time format: '" << opt.end_time << "'. Expected seconds (e.g. 10, 1.5) or HH:MM:SS / MM:SS.\n";
        return 1;
    }

    if (start_sec >= end_sec) {
        std::cerr << "Error: Start time (" << opt.start_time << " / " << start_sec << "s) must be less than end time (" << opt.end_time << " / " << end_sec << "s).\n";
        return 1;
    }

    int vid_w = 640;
    int vid_h = 360;
    double duration = 0.0;
    if (get_video_info(opt.input_file, vid_w, vid_h, duration)) {
        if (duration > 0.0) {
            if (start_sec >= duration) {
                std::cerr << "Error: Start time (" << opt.start_time << " / " << start_sec << "s) is beyond video duration (" << duration << "s).\n";
                return 1;
            }
            if (end_sec > duration) {
                end_sec = duration;
                opt.end_time = std::to_string(duration);
            }
        }
        if (opt.scale_width > 0) {
            vid_h = (vid_h * opt.scale_width) / vid_w;
            vid_w = opt.scale_width;
        }
    }

    if (end_sec - start_sec < 0.01) {
        std::cerr << "Error: Selected time duration is too short.\n";
        return 1;
    }

    int margin_y = std::max(8, vid_h / 20);
    std::vector<std::string> drawtext_filters;

    if (!opt.top_text.empty()) {
        int chosen_font_size = opt.font_size;
        if (chosen_font_size <= 0) {
            chosen_font_size = calculate_font_size(vid_w, vid_h, opt.top_text.length());
        }
        int border_w = std::max(2, chosen_font_size / 10);
        std::ostringstream ss;
        ss << "drawtext=text='" << escape_filter_text(opt.top_text) << "'"
           << ":fontsize=" << chosen_font_size
           << ":fontcolor=white"
           << ":borderw=" << border_w
           << ":bordercolor=black"
           << ":x=(w-text_w)/2"
           << ":y=" << margin_y;
        if (!opt.font_file.empty()) {
            ss << ":fontfile=" << escape_filter_text(opt.font_file);
        }
        drawtext_filters.push_back(ss.str());
    }

    if (!opt.bottom_text.empty()) {
        int chosen_font_size = opt.font_size;
        if (chosen_font_size <= 0) {
            chosen_font_size = calculate_font_size(vid_w, vid_h, opt.bottom_text.length());
        }
        int border_w = std::max(2, chosen_font_size / 10);
        std::ostringstream ss;
        ss << "drawtext=text='" << escape_filter_text(opt.bottom_text) << "'"
           << ":fontsize=" << chosen_font_size
           << ":fontcolor=white"
           << ":borderw=" << border_w
           << ":bordercolor=black"
           << ":x=(w-text_w)/2"
           << ":y=h-text_h-" << margin_y;
        if (!opt.font_file.empty()) {
            ss << ":fontfile=" << escape_filter_text(opt.font_file);
        }
        drawtext_filters.push_back(ss.str());
    }

    std::string filters = "fps=" + std::to_string(opt.fps > 0 ? opt.fps : 15);
    if (opt.scale_width > 0) {
        filters += ",scale=" + std::to_string(opt.scale_width) + ":-1:flags=lanczos";
    }
    for (const auto& dt : drawtext_filters) {
        filters += "," + dt;
    }

    std::string vf = filters + ",split[s0][s1];[s0]palettegen=stats_mode=full[p];[s1][p]paletteuse=dither=sierra2_4a";

    std::ostringstream cmd;
    cmd << "ffmpeg -y -ss " << escape_shell_arg(opt.start_time)
        << " -to " << escape_shell_arg(opt.end_time)
        << " -i " << escape_shell_arg(opt.input_file)
        << " -filter_complex " << escape_shell_arg(vf)
        << " " << escape_shell_arg(opt.output_file);

    int ret = std::system(cmd.str().c_str());
    if (ret != 0) {
        std::cerr << "Warning: High-quality palettegen filter failed, trying standard GIF rendering...\n";
        std::ostringstream fallback_cmd;
        std::string fallback_vf = filters;
        fallback_cmd << "ffmpeg -y -ss " << escape_shell_arg(opt.start_time)
                     << " -to " << escape_shell_arg(opt.end_time)
                     << " -i " << escape_shell_arg(opt.input_file)
                     << " -vf " << escape_shell_arg(fallback_vf)
                     << " " << escape_shell_arg(opt.output_file);
        ret = std::system(fallback_cmd.str().c_str());
    }

    bool success = (ret == 0);
    if (file_exists(opt.output_file)) {
        struct stat st;
        if (stat(opt.output_file.c_str(), &st) != 0 || st.st_size == 0) {
            std::remove(opt.output_file.c_str());
            success = false;
        }
    } else {
        success = false;
    }

    if (!success) {
        if (file_exists(opt.output_file)) {
            std::remove(opt.output_file.c_str());
        }
        std::cerr << "Error: Failed to generate GIF. Please verify that the specified time range contains valid video frames.\n";
        return 1;
    }

    std::cout << "Successfully generated GIF: " << opt.output_file << "\n";
    return 0;
}
