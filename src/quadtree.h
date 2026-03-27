#ifndef QUADTREE_H
#define QUADTREE_H

#include <vector>
#include <memory>

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

    float get_min_x() const { return x - half_width; }
    float get_max_x() const { return x + half_width; }
    float get_min_y() const { return y - half_height; }
    float get_max_y() const { return y + half_height; }
};

// Point data structure
struct Point
{
    float x, y;
    int id;

    Point() : x(0), y(0), id(-1) {}
    Point(float x, float y, int id = -1) : x(x), y(y), id(id) {}
};

// Forward declaration
class QuadtreeNode;

// Quadtree container
class Quadtree
{
public:
    Quadtree(const Bounds& bounds, int max_points = 4);
    ~Quadtree();

    // Insert a point into the quadtree
    bool insert(const Point& point);

    // Query all points within a region
    void query(const Bounds& region, std::vector<Point>& results) const;

    // Query all points and return them
    void get_all_points(std::vector<Point>& results) const;

    // Clear the tree
    void clear();

    // Get statistics
    int get_point_count() const;
    int get_node_count() const;
    int get_depth() const;

    // Get root node for visualization
    const QuadtreeNode* get_root() const { return root_.get(); }

private:
    std::unique_ptr<QuadtreeNode> root_;
    int max_points_per_node_;
};

// Internal node structure (used for visualization)
class QuadtreeNode
{
    friend class Quadtree;

public:
    QuadtreeNode(const Bounds& bounds, int max_points);

    const Bounds& get_bounds() const { return bounds_; }
    bool is_leaf() const { return children_[0] == nullptr; }
    int get_point_count() const { return points_.size(); }
    const std::vector<Point>& get_points() const { return points_; }
    QuadtreeNode* get_child(int index) const 
    { 
        if (index >= 0 && index < 4) return children_[index].get();
        return nullptr;
    }

private:
    bool insert(const Point& point);
    void subdivide();
    void query(const Bounds& region, std::vector<Point>& results) const;
    int get_point_count_recursive() const;
    int get_node_count_recursive() const;
    int get_depth_recursive() const;

    Bounds bounds_;
    std::vector<Point> points_;
    std::unique_ptr<QuadtreeNode> children_[4];  // NW, NE, SW, SE
    int max_points_;
};

#endif // QUADTREE_H
