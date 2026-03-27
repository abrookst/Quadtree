#ifndef TERRAIN_H
#define TERRAIN_H

#include <memory>
#include <string>

#include "quadtree.h"

// Loads an ASCII bitmap terrain file and constructs a Quadtree.
// Each pixel is either:
//   '1' = solid
//   '2' = air
//
// File format (minimal):
//   <width> <height>
//   <row0 of width chars containing 1/2>
//   ...
//   <row height-1>
//
// Note: any extra tokens on the header line are ignored (so older files with
// depth/threshold fields still load), because max depth is controlled via the GUI.
//
// Returns true on success; on failure, returns false and sets out_error.
bool terrain_load_bitmap_file_into_quadtree(
    const std::string& filename,
    std::unique_ptr<Quadtree>& out_tree,
    int max_depth,
    std::string& out_error);

#endif // TERRAIN_H
