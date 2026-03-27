#include "quadtree.h"

#include <algorithm>
#include <cassert>

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
    children_[0] = std::make_unique<QuadtreeNode>(Bounds(x - hw, y + hh, hw, hh), child_depth, max_depth_, child_state);
    children_[1] = std::make_unique<QuadtreeNode>(Bounds(x + hw, y + hh, hw, hh), child_depth, max_depth_, child_state);
    children_[2] = std::make_unique<QuadtreeNode>(Bounds(x - hw, y - hh, hw, hh), child_depth, max_depth_, child_state);
    children_[3] = std::make_unique<QuadtreeNode>(Bounds(x + hw, y - hh, hw, hh), child_depth, max_depth_, child_state);

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
