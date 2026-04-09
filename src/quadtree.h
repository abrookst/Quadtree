#ifndef QUADTREE_H
#define QUADTREE_H

#include <memory>
#include <string>
#include <math.h>
#include <stdbool.h>

// 2D bounding rectangle
struct Bounds
{
    float x, y;           // Center position
    float half_width;     // Half width (radius)
    float half_height;    // Half height (radius)

    Bounds() : x(0), y(0), half_width(0), half_height(0) {}
    Bounds(float x, float y, float w, float h) 
        : x(x), y(y), half_width(w / 2.0f), half_height(h / 2.0f) {}

    bool contains(float px, float py) const
    {
        return px >= x - half_width && px <= x + half_width &&
               py >= y - half_height && py <= y + half_height;
    }

    bool intersects(const Bounds& other) const
    {
        return !(x - half_width > other.x + other.half_width ||
                 x + half_width < other.x - other.half_width ||
                 y - half_height > other.y + other.half_height ||
                 y + half_height < other.y - other.half_height);
    }

    float clamp(float val, float min, float max) {
        if (val < min) return min;
        if (val > max) return max;
        else return val;
    }

    bool intersects_circle(float radius, float x, float y) {
        float closestX = clamp(x, get_min_x(), get_max_x());
        float closestY = clamp(y, get_min_y(), get_max_y());

        float distanceX = x - closestX;
        float distanceY = y - closestY;
        float distanceSquared = (distanceX * distanceX) + (distanceY * distanceY);

        return distanceSquared <= (radius * radius);
    }

    bool fully_contained_by_circle(float radius, float x, float y) {
        float radiusSquared = radius * radius;


        float dx = fmaxf(fabsf(x - get_min_x()), fabsf(x - get_max_x()));
        float dy = fmaxf(fabsf(y - get_min_y()), fabsf(y - get_max_y()));

        return (dx * dx + dy * dy) <= radiusSquared;

    }


    bool contains_bounds(const Bounds& other) const
    {
        return get_min_x() <= other.get_min_x() && get_max_x() >= other.get_max_x() &&
               get_min_y() <= other.get_min_y() && get_max_y() >= other.get_max_y();
    }

    float get_min_x() const { return x - half_width; }
    float get_max_x() const { return x + half_width; }
    float get_min_y() const { return y - half_height; }
    float get_max_y() const { return y + half_height; }
};

enum class FillState
{
    Empty,
    Solid,
    Mixed,
};




// Forward declaration
class QuadtreeNode;

// Terrain occupancy quadtree
class Quadtree
{
public:
    Quadtree(const Bounds& bounds, int max_depth = 8, bool initial_filled = false);
    ~Quadtree();
    void build_from_bitmap(const uint8_t* solid, int width, int height);

    // Set entire tree to solid/empty
    void set_all(bool filled);

    // Fill/clear a rectangular region
    void set_region(const Bounds& region, bool filled);

    // Query if a single point is inside solid terrain
    bool is_filled(float px, float py) const;

    // Query whether a region is entirely empty/solid, or mixed
    FillState query_region(const Bounds& region) const;

    // find node at world space coords
    QuadtreeNode* findNodeAtPoint(float px, float py);

    // call set_circle on nodes
    void set_circle(float radius, float x, float y);

    // Clear the tree
    void clear();

    // Get statistics
    int get_node_count() const;
    int get_depth() const;

    // Get root node for visualization
    const QuadtreeNode* get_root() const { return root_.get(); }

private:
    std::unique_ptr<QuadtreeNode> root_;
    int max_depth_;
    bool initial_filled_;
};

// Internal node structure (used for visualization)
class QuadtreeNode
{
    friend class Quadtree;

public:
    QuadtreeNode(const Bounds& bounds, int depth, int max_depth, FillState state);

    
    // fills in the circle
    void set_circle(float radius, float x, float y);

    
    const Bounds& get_bounds() const { return bounds_; }
    bool is_leaf() const { return children_[0] == nullptr; }
    FillState get_state() const { return state_; }
    QuadtreeNode* get_child(int index) const 
    { 
        if (index >= 0 && index < 4) return children_[index].get();
        return nullptr;
    }

private:
    void subdivide();
    void set_all(FillState state);
    void set_region(const Bounds& region, FillState state);
    bool is_filled(float px, float py) const;
    FillState query_region(const Bounds& region) const;
    int get_node_count_recursive() const;
    int get_depth_recursive() const;
    void try_collapse();
    int child_index_for_point(float px, float py) const;

    Bounds bounds_;
    std::unique_ptr<QuadtreeNode> children_[4];  // NW, NE, SW, SE
    int depth_;
    int max_depth_;
    FillState state_;
};

#endif // QUADTREE_H
