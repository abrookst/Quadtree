#include "quadtree.h"

#include <iostream>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <vector>

static FillState filled_to_state(bool filled)
{
    return filled ? FillState::Solid : FillState::Empty;
}

QuadtreeNode::QuadtreeNode(const Bounds& bounds, int depth, int max_depth, FillState state)
    : bounds_(bounds)
    , depth_(depth)
    , max_depth_(std::max(0, max_depth))
    , state_(state)
{
    for (auto& child : children_) child.reset();
}

void Quadtree::set_circle(float radius, float x, float y) {
    QuadtreeNode* root = root_.get();
    root->set_circle(radius, x, y);
    
}

void QuadtreeNode::set_circle(float radius, float x, float y) {
    //if the circle doesnt intersect this node's bounding box, we can move on
    if (!bounds_.intersects_circle(radius, x, y)) {
        return;
    } 
    //if the bounds of this node are fully encased in the circle, we can set all its kids to empty and move on.
    if (bounds_.fully_contained_by_circle(radius, x, y)) {
        set_all(FillState::Empty);
        return;
    } 
    //if we're at max depth, set to empty and move on
    if (depth_ >= max_depth_) {
        set_all(FillState::Empty);
        return;
    }
    //we know this is on the edge of the circle (intersects but doesnt fully contain), so subdivide for greater circle resolution
    if (is_leaf()) {
        subdivide();
    }

    //recurse to all children, then try to collapse any redundant nodes when back.
    children_[0]->set_circle(radius, x, y);
    children_[1]->set_circle(radius, x, y);
    children_[2]->set_circle(radius, x, y);
    children_[3]->set_circle(radius, x, y);
    try_collapse();
}

void QuadtreeNode::subdivide()
{
    if (!is_leaf()) return;

    const float hw = bounds_.half_width * 0.5f;
    const float hh = bounds_.half_height * 0.5f;
    const float x = bounds_.x;
    const float y = bounds_.y;

    const int child_depth = depth_ + 1;
    const FillState child_state = (state_ == FillState::Solid) ? FillState::Solid : FillState::Empty;

    // NW, NE, SW, SE
    children_[0] = std::make_unique<QuadtreeNode>(Bounds(x - hw, y + hh, hw * 2.0f, hh * 2.0f), child_depth, max_depth_, child_state);
    children_[1] = std::make_unique<QuadtreeNode>(Bounds(x + hw, y + hh, hw * 2.0f, hh * 2.0f), child_depth, max_depth_, child_state);
    children_[2] = std::make_unique<QuadtreeNode>(Bounds(x - hw, y - hh, hw * 2.0f, hh * 2.0f), child_depth, max_depth_, child_state);
    children_[3] = std::make_unique<QuadtreeNode>(Bounds(x + hw, y - hh, hw * 2.0f, hh * 2.0f), child_depth, max_depth_, child_state);

    state_ = FillState::Mixed;
}

void QuadtreeNode::set_all(FillState state)
{
    state_ = state;
    for (auto& child : children_) child.reset();
}

void QuadtreeNode::set_region(const Bounds& region, FillState state)
{
    if (!bounds_.intersects(region)) return;

    if (region.contains_bounds(bounds_) || depth_ >= max_depth_)
    {
        set_all(state);
        return;
    }

    if (is_leaf())
    {
        if (state_ != FillState::Mixed && state_ == state) return;
        subdivide();
    }

    for (auto& child : children_)
    {
        child->set_region(region, state);
    }

    try_collapse();
}

int QuadtreeNode::child_index_for_point(float px, float py) const
{
    const bool east = px >= bounds_.x;
    const bool north = py >= bounds_.y;

    // NW, NE, SW, SE
    if (!east && north) return 0;
    if (east && north) return 1;
    if (!east && !north) return 2;
    return 3;
}

QuadtreeNode* Quadtree::findNodeAtPoint(float px, float py) {
    QuadtreeNode* returnNode = root_.get();
    while (!returnNode->is_leaf()) {
        int childIndex = returnNode->child_index_for_point(px, py);
        returnNode = returnNode->get_child(childIndex);
    }
    return returnNode;
}

bool QuadtreeNode::is_filled(float px, float py) const
{
    if (!bounds_.contains(px, py)) return false;

    if (is_leaf())
    {
        return state_ == FillState::Solid;
    }

    const int index = child_index_for_point(px, py);
    assert(index >= 0 && index < 4);
    return children_[index]->is_filled(px, py);
}

FillState QuadtreeNode::query_region(const Bounds& region) const
{
    if (!bounds_.intersects(region))
    {
        // No overlap: doesn't contribute to the queried region.
        return FillState::Empty;
    }

    if (region.contains_bounds(bounds_))
    {
        return state_;
    }

    if (is_leaf())
    {
        return state_;
    }

    bool any_solid = false;
    bool any_empty = false;
    bool any_touched = false;

    for (const auto& child : children_)
    {
        if (!child->bounds_.intersects(region)) continue;
        any_touched = true;

        const FillState child_state = child->query_region(region);
        if (child_state == FillState::Mixed) return FillState::Mixed;

        if (child_state == FillState::Solid) any_solid = true;
        if (child_state == FillState::Empty) any_empty = true;
        if (any_solid && any_empty) return FillState::Mixed;
    }

    if (!any_touched) return FillState::Empty;
    return any_solid ? FillState::Solid : FillState::Empty;
}

int QuadtreeNode::get_node_count_recursive() const
{
    int count = 1;
    if (!is_leaf())
    {
        for (const auto& child : children_)
        {
            count += child->get_node_count_recursive();
        }
    }
    return count;
}

int QuadtreeNode::get_depth_recursive() const
{
    if (is_leaf()) return 1;

    int deepest_child = 0;
    for (const auto& child : children_)
    {
        deepest_child = std::max(deepest_child, child->get_depth_recursive());
    }
    return deepest_child + 1;
}

void QuadtreeNode::try_collapse()
{
    if (is_leaf()) return;

    FillState first_state = FillState::Mixed;
    for (const auto& child : children_)
    {
        if (!child->is_leaf())
        {
            state_ = FillState::Mixed;
            return;
        }

        const FillState child_state = child->state_;
        if (child_state == FillState::Mixed)
        {
            state_ = FillState::Mixed;
            return;
        }

        if (first_state == FillState::Mixed)
        {
            first_state = child_state;
        }
        else if (child_state != first_state)
        {
            state_ = FillState::Mixed;
            return;
        }
    }

    state_ = first_state;
    for (auto& child : children_) child.reset();
}

Quadtree::Quadtree(const Bounds& bounds, int max_depth, bool initial_filled)
    : max_depth_(std::max(0, max_depth))
    , initial_filled_(initial_filled)
{
    root_ = std::make_unique<QuadtreeNode>(bounds, 0, max_depth_, filled_to_state(initial_filled_));
}

Quadtree::~Quadtree() = default;

void Quadtree::set_all(bool filled)
{
    root_->set_all(filled_to_state(filled));
}

void Quadtree::set_region(const Bounds& region, bool filled)
{
    root_->set_region(region, filled_to_state(filled));
}

bool Quadtree::is_filled(float px, float py) const
{
    return root_->is_filled(px, py);
}

FillState Quadtree::query_region(const Bounds& region) const
{
    return root_->query_region(region);
}

void Quadtree::clear()
{
    Bounds bounds = root_->get_bounds();
    root_ = std::make_unique<QuadtreeNode>(bounds, 0, max_depth_, filled_to_state(initial_filled_));
}

int Quadtree::get_node_count() const
{
    return root_->get_node_count_recursive();
}

int Quadtree::get_depth() const
{
    return root_->get_depth_recursive();
}

void Quadtree::build_from_bitmap(const uint8_t* solid, int width, int height)
{
    if (!solid || width <= 0 || height <= 0)
    {
        clear();
        return;
    }

    // Build an integral image of solid counts so region sums are O(1).
    const int stride = width + 1;
    std::vector<uint32_t> integral(static_cast<size_t>(stride) * static_cast<size_t>(height + 1), 0);

    for (int y = 0; y < height; y++)
    {
        uint32_t row_sum = 0;
        for (int x = 0; x < width; x++)
        {
            row_sum += (solid[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] != 0) ? 1u : 0u;
            const uint32_t above = integral[static_cast<size_t>(y) * static_cast<size_t>(stride) + static_cast<size_t>(x + 1)];
            integral[static_cast<size_t>(y + 1) * static_cast<size_t>(stride) + static_cast<size_t>(x + 1)] = above + row_sum;
        }
    }

    auto sum_region = [&](int x0, int y0, int x1, int y1) -> uint32_t
    {
        x0 = std::clamp(x0, 0, width);
        x1 = std::clamp(x1, 0, width);
        y0 = std::clamp(y0, 0, height);
        y1 = std::clamp(y1, 0, height);
        if (x1 <= x0 || y1 <= y0) return 0u;

        const size_t A = static_cast<size_t>(y0) * static_cast<size_t>(stride) + static_cast<size_t>(x0);
        const size_t B = static_cast<size_t>(y0) * static_cast<size_t>(stride) + static_cast<size_t>(x1);
        const size_t C = static_cast<size_t>(y1) * static_cast<size_t>(stride) + static_cast<size_t>(x0);
        const size_t D = static_cast<size_t>(y1) * static_cast<size_t>(stride) + static_cast<size_t>(x1);
        return integral[D] - integral[B] - integral[C] + integral[A];
    };

    const Bounds root_bounds = root_->get_bounds();
    const float min_x = root_bounds.get_min_x();
    const float max_x = root_bounds.get_max_x();
    const float min_y = root_bounds.get_min_y();
    const float max_y = root_bounds.get_max_y();

    const float dx = (max_x - min_x) / static_cast<float>(width);
    const float dy = (max_y - min_y) / static_cast<float>(height);

    auto pixel_rect_to_bounds = [&](int x0, int y0, int x1, int y1) -> Bounds
    {
        const float r_min_x = min_x + static_cast<float>(x0) * dx;
        const float r_max_x = min_x + static_cast<float>(x1) * dx;
        const float r_max_y = max_y - static_cast<float>(y0) * dy;
        const float r_min_y = max_y - static_cast<float>(y1) * dy;

        const float cx = (r_min_x + r_max_x) * 0.5f;
        const float cy = (r_min_y + r_max_y) * 0.5f;
        const float w = (r_max_x - r_min_x);
        const float h = (r_max_y - r_min_y);
        return Bounds(cx, cy, w, h);
    };

    struct BuildResult
    {
        std::unique_ptr<QuadtreeNode> node;
        FillState uniform_state = FillState::Mixed;
        bool is_uniform_leaf = false;
    };

    std::function<BuildResult(int, int, int, int, int)> build;
    build = [&](int x0, int y0, int x1, int y1, int depth) -> BuildResult
    {
        const int wpx = x1 - x0;
        const int hpx = y1 - y0;
        const uint32_t area = (wpx > 0 && hpx > 0) ? static_cast<uint32_t>(wpx) * static_cast<uint32_t>(hpx) : 0u;

        if (area == 0u)
        {
            BuildResult r;
            r.uniform_state = FillState::Empty;
            r.is_uniform_leaf = true;
            r.node = std::make_unique<QuadtreeNode>(pixel_rect_to_bounds(x0, y0, x1, y1), depth, max_depth_, FillState::Empty);
            return r;
        }

        const uint32_t solid_count = sum_region(x0, y0, x1, y1);
        const uint32_t empty_count = area - solid_count;

        auto make_leaf = [&](FillState st) -> BuildResult
        {
            BuildResult r;
            r.uniform_state = st;
            r.is_uniform_leaf = true;
            r.node = std::make_unique<QuadtreeNode>(pixel_rect_to_bounds(x0, y0, x1, y1), depth, max_depth_, st);
            return r;
        };

        // All-or-nothing: if region is entirely one type, it's a leaf.
        if (solid_count == 0u)
            return make_leaf(FillState::Empty);
        if (empty_count == 0u)
            return make_leaf(FillState::Solid);

        // Region is mixed: subdivide unless at max depth or can't split.
        if (depth >= max_depth_ || (wpx == 1 && hpx == 1))
        {
            // At max depth with mixed content: use majority for visual.
            return make_leaf((solid_count >= empty_count) ? FillState::Solid : FillState::Empty);
        }

        const int mx = x0 + wpx / 2;
        const int my = y0 + hpx / 2;
        if (mx == x0 || mx == x1 || my == y0 || my == y1)
        {
            // Can't split further: use majority.
            return make_leaf((solid_count >= empty_count) ? FillState::Solid : FillState::Empty);
        }

        auto node = std::make_unique<QuadtreeNode>(pixel_rect_to_bounds(x0, y0, x1, y1), depth, max_depth_, FillState::Mixed);

        // NW, NE, SW, SE in pixel space.
        BuildResult nw = build(x0, y0, mx, my, depth + 1);
        BuildResult ne = build(mx, y0, x1, my, depth + 1);
        BuildResult sw = build(x0, my, mx, y1, depth + 1);
        BuildResult se = build(mx, my, x1, y1, depth + 1);

        // Collapse if all four children are uniform leaves of the same state.
        if (nw.is_uniform_leaf && ne.is_uniform_leaf && sw.is_uniform_leaf && se.is_uniform_leaf &&
            nw.uniform_state == ne.uniform_state && nw.uniform_state == sw.uniform_state && nw.uniform_state == se.uniform_state &&
            nw.uniform_state != FillState::Mixed)
        {
            return make_leaf(nw.uniform_state);
        }

        node->children_[0] = std::move(nw.node);
        node->children_[1] = std::move(ne.node);
        node->children_[2] = std::move(sw.node);
        node->children_[3] = std::move(se.node);
        node->state_ = FillState::Mixed;

        BuildResult r;
        r.node = std::move(node);
        return r;
    };

    root_ = build(0, 0, width, height, 0).node;
}
