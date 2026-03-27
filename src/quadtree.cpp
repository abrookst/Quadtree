#include "quadtree.h"
#include <algorithm>

// QuadtreeNode implementation
QuadtreeNode::QuadtreeNode(const Bounds& bounds, int max_points)
    : bounds_(bounds), max_points_(max_points)
{
    for (int i = 0; i < 4; ++i)
        children_[i] = nullptr;
}

void QuadtreeNode::subdivide()
{
    float half_w = bounds_.half_width / 2.0f;
    float half_h = bounds_.half_height / 2.0f;
    float x = bounds_.x;
    float y = bounds_.y;

    // NW (top-left)
    children_[0] = std::make_unique<QuadtreeNode>(
        Bounds(x - half_w, y + half_h, half_w * 2.0f, half_h * 2.0f), max_points_);
    
    // NE (top-right)
    children_[1] = std::make_unique<QuadtreeNode>(
        Bounds(x + half_w, y + half_h, half_w * 2.0f, half_h * 2.0f), max_points_);
    
    // SW (bottom-left)
    children_[2] = std::make_unique<QuadtreeNode>(
        Bounds(x - half_w, y - half_h, half_w * 2.0f, half_h * 2.0f), max_points_);
    
    // SE (bottom-right)
    children_[3] = std::make_unique<QuadtreeNode>(
        Bounds(x + half_w, y - half_h, half_w * 2.0f, half_h * 2.0f), max_points_);

    // Move existing points to children
    for (const auto& point : points_)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (children_[i]->bounds_.contains(point.x, point.y))
            {
                children_[i]->insert(point);
                break;
            }
        }
    }
    points_.clear();
}

bool QuadtreeNode::insert(const Point& point)
{
    // Check if point is within bounds
    if (!bounds_.contains(point.x, point.y))
        return false;

    // If this is a leaf node and has space, add the point
    if (is_leaf())
    {
        if (points_.size() < static_cast<size_t>(max_points_))
        {
            points_.push_back(point);
            return true;
        }
        else
        {
            // Need to subdivide
            subdivide();
        }
    }

    // Try to insert into children
    for (int i = 0; i < 4; ++i)
    {
        if (children_[i]->insert(point))
            return true;
    }

    return false;
}

void QuadtreeNode::query(const Bounds& region, std::vector<Point>& results) const
{
    // If this region doesn't intersect with our bounds, return
    if (!bounds_.intersects(region))
        return;

    // If we're a leaf, check all points
    if (is_leaf())
    {
        for (const auto& point : points_)
        {
            if (region.contains(point.x, point.y))
                results.push_back(point);
        }
        return;
    }

    // Otherwise, query children
    for (int i = 0; i < 4; ++i)
    {
        if (children_[i])
            children_[i]->query(region, results);
    }
}

int QuadtreeNode::get_point_count_recursive() const
{
    int count = points_.size();
    for (int i = 0; i < 4; ++i)
    {
        if (children_[i])
            count += children_[i]->get_point_count_recursive();
    }
    return count;
}

int QuadtreeNode::get_node_count_recursive() const
{
    int count = 1;
    for (int i = 0; i < 4; ++i)
    {
        if (children_[i])
            count += children_[i]->get_node_count_recursive();
    }
    return count;
}

int QuadtreeNode::get_depth_recursive() const
{
    if (is_leaf())
        return 1;

    int max_depth = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (children_[i])
            max_depth = std::max(max_depth, children_[i]->get_depth_recursive());
    }
    return max_depth + 1;
}

// Quadtree implementation
Quadtree::Quadtree(const Bounds& bounds, int max_points)
    : max_points_per_node_(max_points)
{
    root_ = std::make_unique<QuadtreeNode>(bounds, max_points);
}

Quadtree::~Quadtree() = default;

bool Quadtree::insert(const Point& point)
{
    return root_->insert(point);
}

void Quadtree::query(const Bounds& region, std::vector<Point>& results) const
{
    results.clear();
    root_->query(region, results);
}

void Quadtree::get_all_points(std::vector<Point>& results) const
{
    results.clear();
    Bounds all_bounds = root_->get_bounds();
    root_->query(all_bounds, results);
}

void Quadtree::clear()
{
    Bounds bounds = root_->get_bounds();
    root_ = std::make_unique<QuadtreeNode>(bounds, max_points_per_node_);
}

int Quadtree::get_point_count() const
{
    return root_->get_point_count_recursive();
}

int Quadtree::get_node_count() const
{
    return root_->get_node_count_recursive();
}

int Quadtree::get_depth() const
{
    return root_->get_depth_recursive();
}
