#include "player.h"
#include "appcontext.h"
#include <cmath>
#include <iostream>

Player::Player(float x, float y, float width, float height, AppContext& app)
  : x(x), y(y), vx(0), vy(0), width(width), height(height),
    speed(150.0f), jumpForce(250.0f), gravity(400.0f), onGround(false), active(true),
    terrain(app.terrain.get()),
    world_min_x(app.world_min_x), world_max_x(app.world_max_x),
    world_min_y(app.world_min_y), world_max_y(app.world_max_y)
{}

bool Player::checkCollision(float test_x, float test_y) {
    if (!terrain) return false;

    // Check points around the box to see if any hit the terrain.
    // Instead of full AABB, we sample points along the edges.
    const int samples = 5;
    for (int i = 0; i <= samples; i++) {
        float f = (float)i / samples;
        // Bottom edge
        if (terrain->is_filled(test_x - width/2 + width*f, test_y - height/2)) return true;
        // Top edge
        if (terrain->is_filled(test_x - width/2 + width*f, test_y + height/2)) return true;
        // Left edge
        if (terrain->is_filled(test_x - width/2, test_y - height/2 + height*f)) return true;
        // Right edge
        if (terrain->is_filled(test_x + width/2, test_y - height/2 + height*f)) return true;
    }
    return false;
}

void Player::update(float dt) {
    const Uint8* state = SDL_GetKeyboardState(NULL);

    // Apply gravity
    vy -= gravity * dt;

    // Horizontal Movement
    vx = 0.0f;
    if (state[SDL_SCANCODE_A]) vx -= speed;
    if (state[SDL_SCANCODE_D]) vx += speed;

    // Jumping
    if (state[SDL_SCANCODE_SPACE] && onGround) {
        vy = jumpForce;
        onGround = false;
    }

    // Try moving X
    if (vx != 0.0f) {
        float targetX = x + vx * dt;
        bool movedThisFrame = false;

        if (!checkCollision(targetX, y)) {
            x = targetX;
            movedThisFrame = true;
        } else {
            // Step-up incline / small pixel logic
            float maxStep = 5.0f; 
            float stepIncrement = 1.0f;
            for (float s = stepIncrement; s <= maxStep; s += stepIncrement) {
                if (!checkCollision(targetX, y + s)) {
                    x = targetX;
                    y += s;
                    movedThisFrame = true;
                    break;
                }
            }
        }

        if (!movedThisFrame) {
            // Blocked by a larger wall
            vx = 0.0f;
        }
    }

    // Try moving Y
    float targetY = y + vy * dt;
    if (!checkCollision(x, targetY)) {
        y = targetY;
        onGround = false;
    } else {
        if (vy <= 0.0f) {
            onGround = true;
        } else {
            onGround = false;
        }
        vy = 0.0f;
    }

    // Bounds checking
    if (x - width/2 < world_min_x) x = world_min_x + width/2;
    if (x + width/2 > world_max_x) x = world_max_x - width/2;
    if (y - height/2 < world_min_y) {
        y = world_min_y + height/2;
        onGround = true;
        vy = 0.0f;
    }
    if (y + height/2 > world_max_y) {
        y = world_max_y - height/2;
        vy = 0.0f;
    }
}

void Player::draw(ImDrawList* drawList, ImVec2 screenPos, float screenScale) {
    float sw = width * screenScale / 2.0f;
    float sh = height * screenScale / 2.0f;
    
    // In our coordinate system, y grows upward but screen y grows downward.
    ImVec2 pMin(screenPos.x - sw, screenPos.y - sh);
    ImVec2 pMax(screenPos.x + sw, screenPos.y + sh);

    drawList->AddRectFilled(pMin, pMax, IM_COL32(0, 255, 0, 255));
    drawList->AddRect(pMin, pMax, IM_COL32(0, 0, 0, 255), 0.0f, 0, 2.0f); // Outline
}
