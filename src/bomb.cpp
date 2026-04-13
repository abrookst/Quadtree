#include "Bomb.h"

// Constructor 
Bomb::Bomb(float x, float y, float radius) 
  : x(x),
    y(y),
    vx(0.0f),
    vy(0.0f),
    radius(radius),
    bounceStrength(0.8f),
    active(true)
    {}

// Update physics
void Bomb::update(float dt) {

    // Apply gravity
    vy += gravity * dt;

    // Move
    x += vx * dt;
    y += vy * dt;
}

// Draw bomb
void Bomb::draw(ImDrawList* drawList) {

    drawList->AddCircleFilled(
        ImVec2(x, y),
        radius,
        IM_COL32(255, 100, 100, 255)
    );
}

// Set velocity
void Bomb::setVelocity(float newVx, float newVy) {

     vx = newVx;
     vy = newVy;

}

// Getters
float Bomb::getX() const {
    return x;
}

float Bomb::getY() const {
    return y;
}

void Bomb::explode() {
    active = false;
}

bool Bomb::isActive() const {
    return active;
}