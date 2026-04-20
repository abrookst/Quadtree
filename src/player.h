#pragma once

#include "imgui.h"
#include "quadtree.h"
#include <SDL2/SDL.h>

struct AppContext;

class Player {
public:
    // Constructor 
    Player(float x, float y, float width, float height, AppContext& app);

    // Update physics and input
    void update(float dt);

    // Draw entity
    void draw(ImDrawList* drawList, ImVec2 screenPos, float screenScale);

    // active state
    bool isActive() const { return active; }

    float getX() const { return x; }
    float getY() const { return y; }
    float getWidth() const { return width; }
    float getHeight() const { return height; }

private:
    // Collision checking helper
    bool checkCollision(float test_x, float test_y);

    float x, y;
    float vx, vy;
    float width, height;

    float speed;
    float jumpForce;
    float gravity;
    bool onGround;

    Quadtree* terrain;
    float world_min_x, world_max_x, world_min_y, world_max_y;

    bool active;
};
