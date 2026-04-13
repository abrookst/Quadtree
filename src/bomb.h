#pragma once

#include "imgui.h"

class Bomb {

public:

    // Constructor
    Bomb(float x, float y, float radius);

    // Update physics
    void update(float dt);

    // Draw entity
    void draw(ImDrawList* drawList);

    // Set Velocity
    void setVelocity(float vx, float vy);

    // Get Position
    float getX() const;
    float getY() const;

    // Explode Bomb
    void explode();

    // Check if it exists in the level
    bool isActive() const;

private:

    // position
    float x;
    float y;

    // velocity
    float vx;
    float vy;

    // size
    float radius;

    // physics
    float bounceStrength;

    static constexpr float gravity = 980.0f;

    // active state
    bool active;

};