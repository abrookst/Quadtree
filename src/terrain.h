#ifndef TERRAIN_H
#define TERRAIN_H

#include <memory>
#include <string>

#include "quadtree.h"

// Loads an ASCII bitmap terrain file and constructs a Quadtree.
// Each pixel is either:
//   '1' = solid
//   '2' = air
bool terrain_load_bitmap_file_into_quadtree(
    const std::string& filename,
    std::unique_ptr<Quadtree>& out_tree,
    int max_depth,
    std::string& out_error);

bool terrain_build_quadtree_from_bitmap(
    const uint8_t* solid,
    int width,
    int height,
    int max_depth,
    std::unique_ptr<Quadtree>& out_tree);

bool terrain_save_bitmap_file(
    const std::string& filename,
    const uint8_t* solid,
    int width,
    int height,
    std::string& out_error);

// Get bitmap file dimensions without loading full data
bool terrain_get_bitmap_dimensions(
    const std::string& filename,
    int& out_width,
    int& out_height,
    std::string& out_error);

#endif // TERRAIN_H
