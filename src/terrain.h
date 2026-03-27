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

#endif // TERRAIN_H
