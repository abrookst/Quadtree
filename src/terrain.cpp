#include "terrain.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

static std::string trim_copy(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;

    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;

    return s.substr(start, end - start);
}

static std::vector<std::string> split_ws(const std::string& s)
{
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

static bool try_open(std::ifstream& in, const std::filesystem::path& path)
{
    in.open(path, std::ios::in);
    return in.is_open();
}

static bool open_terrain_file(std::ifstream& in, const std::string& filename, std::string& out_opened_path)
{
    namespace fs = std::filesystem;

    // Try as provided first (works for absolute paths and simple relative input).
    // Try as provided
    if (try_open(in, filename))
    {
        out_opened_path = filename;
        return true;
    }

    const fs::path file_path(filename);
    fs::path probe = fs::current_path();

    // Walk up the directory tree so running from build/, build/Debug/, etc. still finds assets.
    for (int i = 0; i < 8; i++)
    {
        const fs::path p1 = probe / file_path;
        if (try_open(in, p1))
        {
            out_opened_path = p1.generic_string();
            return true;
        }

        const fs::path p2 = probe / "assets" / "terrain" / file_path;
        if (try_open(in, p2))
        {
            out_opened_path = p2.generic_string();
            return true;
        }

        if (!probe.has_parent_path()) break;
        const fs::path parent = probe.parent_path();
        if (parent == probe) break;
        probe = parent;
    }

    return false;
}

static int ceil_log2_int(int v)
{
    if (v <= 1) return 0;
    int p = 0;
    int x = 1;
    while (x < v && p < 30)
    {
        x <<= 1;
        p++;
    }
    return p;
}

struct BitmapTerrain
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> solid; // size = width*height, 1 = solid, 0 = air
};

static std::string strip_inline_comment(const std::string& s)
{
    const size_t hash = s.find('#');
    if (hash == std::string::npos) return s;
    return s.substr(0, hash);
}

static bool parse_bitmap_terrain(std::ifstream& in, BitmapTerrain& out, std::string& out_error)
{
    out = BitmapTerrain{};

    std::string line;
    int line_no = 0;

    // Find header line: "W H" (extra tokens are ignored for backwards compatibility)
    while (std::getline(in, line))
    {
        line_no++;
        line = trim_copy(strip_inline_comment(line));
        if (line.empty()) continue;

        auto tokens = split_ws(line);
        if (tokens.size() < 2)
        {
            out_error = "Parse error at line " + std::to_string(line_no) + ": expected '<width> <height>'";
            return false;
        }

        out.width = std::stoi(tokens[0]);
        out.height = std::stoi(tokens[1]);

        if (out.width <= 0 || out.height <= 0)
        {
            out_error = "Parse error: width/height must be > 0";
            return false;
        }

        break;
    }

    if (out.width <= 0 || out.height <= 0)
    {
        out_error = "Parse error: missing bitmap header";
        return false;
    }

    out.solid.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height), 0);

    // Read pixel rows
    int y = 0;
    while (y < out.height && std::getline(in, line))
    {
        line_no++;
        line = strip_inline_comment(line);
        if (trim_copy(line).empty()) continue;

        std::string row;
        row.reserve(static_cast<size_t>(out.width));
        for (char c : line)
        {
            if (c == '1' || c == '2') row.push_back(c);
        }

        if (static_cast<int>(row.size()) < out.width)
        {
            out_error = "Parse error at line " + std::to_string(line_no) + ": expected at least " +
                        std::to_string(out.width) + " pixels (chars '1'/'2')";
            return false;
        }

        for (int x = 0; x < out.width; x++)
        {
            const char c = row[static_cast<size_t>(x)];
            out.solid[static_cast<size_t>(y) * static_cast<size_t>(out.width) + static_cast<size_t>(x)] = (c == '1') ? 1 : 0;
        }

        y++;
    }

    if (y != out.height)
    {
        out_error = "Parse error: expected " + std::to_string(out.height) + " rows of pixels";
        return false;
    }

    return true;
}

bool terrain_load_bitmap_file_into_quadtree(
    const std::string& filename,
    std::unique_ptr<Quadtree>& out_tree,
    int max_depth,
    std::string& out_error)
{
    out_error.clear();

    auto t_start = std::chrono::high_resolution_clock::now();

    std::ifstream in;
    std::string opened_path;
    auto t_open_start = std::chrono::high_resolution_clock::now();
    if (!open_terrain_file(in, filename, opened_path))
    {
        out_error = "Could not open terrain file: '" + filename + "'";
        return false;
    }
    auto t_open_end = std::chrono::high_resolution_clock::now();
    auto ms_open = std::chrono::duration_cast<std::chrono::milliseconds>(t_open_end - t_open_start).count();

    BitmapTerrain bmp;
    auto t_parse_start = std::chrono::high_resolution_clock::now();
    if (!parse_bitmap_terrain(in, bmp, out_error))
    {
        if (!opened_path.empty())
            out_error = out_error + " (file: " + opened_path + ")";
        return false;
    }
    auto t_parse_end = std::chrono::high_resolution_clock::now();
    auto ms_parse = std::chrono::duration_cast<std::chrono::milliseconds>(t_parse_end - t_parse_start).count();

    const int inferred_depth = ceil_log2_int(std::max(bmp.width, bmp.height));
    const int depth_to_use = (max_depth >= 0) ? max_depth : inferred_depth;
    // Bounds constructor takes full width/height (it stores half extents internally).
    const Bounds bounds(0.0f, 0.0f, static_cast<float>(bmp.width), static_cast<float>(bmp.height));

    auto tree = std::make_unique<Quadtree>(bounds, std::max(0, depth_to_use), false);
    auto t_build_start = std::chrono::high_resolution_clock::now();
    tree->build_from_bitmap(bmp.solid.data(), bmp.width, bmp.height);
    auto t_build_end = std::chrono::high_resolution_clock::now();
    auto ms_build = std::chrono::duration_cast<std::chrono::milliseconds>(t_build_end - t_build_start).count();

    auto t_end = std::chrono::high_resolution_clock::now();
    auto ms_total = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();

    // Log timing details to stderr for console output
    std::cerr << "[TIMING] File open: " << ms_open << "ms, Parse: " << ms_parse 
              << "ms, Build quadtree: " << ms_build << "ms, Total: " << ms_total << "ms" << std::endl;

    out_tree = std::move(tree);
    return true;
}

bool terrain_save_bitmap_file(
    const std::string& filename,
    const uint8_t* solid,
    int width,
    int height,
    std::string& out_error)
{
    out_error.clear();

    if (!solid || width <= 0 || height <= 0)
    {
        out_error = "Invalid bitmap dimensions or null data";
        return false;
    }

    std::ofstream out;
    out.open(filename, std::ios::out);
    if (!out.is_open())
    {
        out_error = "Could not open file for writing: " + filename;
        return false;
    }

    // Write header
    out << width << " " << height << "\n";

    if (!out.good())
    {
        out_error = "Error writing bitmap header";
        return false;
    }

    // Write pixel rows
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const uint8_t pixel = solid[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
            out << (pixel ? '1' : '2');
        }
        out << "\n";

        if (!out.good())
        {
            out_error = "Error writing bitmap data at row " + std::to_string(y);
            return false;
        }
    }

    out.close();
    return true;
}

bool terrain_build_quadtree_from_bitmap(
    const uint8_t* solid,
    int width,
    int height,
    int max_depth,
    std::unique_ptr<Quadtree>& out_tree)
{
    if (!solid || width <= 0 || height <= 0)
        return false;

    const int inferred_depth = ceil_log2_int(std::max(width, height));
    const int depth_to_use = (max_depth >= 0) ? max_depth : inferred_depth;
    // Bounds constructor takes full width/height (it stores half extents internally).
    const Bounds bounds(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

    auto tree = std::make_unique<Quadtree>(bounds, std::max(0, depth_to_use), false);
    tree->build_from_bitmap(solid, width, height);

    out_tree = std::move(tree);
    return true;
}
