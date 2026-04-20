#include "bomb.h"
#include "imgui.h"
#include "appcontext.h"
#include <iostream>

// Constructor 
Bomb::Bomb(float x, float y, float radius, bool timedExplosion, float explodeTime, int hitsToExplode, AppContext& app) 
  : x(x),
    y(y),
    vx(0.0f),
    vy(0.0f),
    radius(radius),
    bounceStrength(0.5f),
    gravity(98.0f),
    active(true),
    timedExplosion(timedExplosion),
    explodeTime(explodeTime),
    hitsToExplode(hitsToExplode),
    startHits(hitsToExplode),
    lifeTimer(0.0f),
    terrain(app.terrain.get()),
    world_max_x(app.world_max_x),
    world_max_y(app.world_max_y),
    world_min_x(app.world_min_x),
    world_min_y(app.world_min_y)
    {}

// Update physics
void Bomb::update(float dt) {

    // Apply gravity
    vy -= gravity * dt;

    // Move
    x += vx * dt;
    y += vy * dt;

    // Terrain collision
    resolveTerrainCollision();

    lifeTimer = lifeTimer + dt;

    if (lifeTimer > explodeTime && timedExplosion) {
        std::cout << "[BOMB] Explode!" << std::endl;
        explode();
    }
}

// Draw bomb
void Bomb::draw(ImDrawList* drawList, ImVec2 screenPos, float screenRadius) {
    if (timedExplosion) {
        drawList->AddCircleFilled(
        screenPos,
        screenRadius,
        IM_COL32(lifeTimer / explodeTime * 255, 100, 100, 255));
    } else {
        drawList->AddCircleFilled(
        screenPos,
        screenRadius,
        IM_COL32(hitsToExplode / startHits * 255, 100, 100, 255));
    }
    
    
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
    terrain->set_circle(explodeRadius, x, y); // x and y are world coordinates!
    active = false;
}

bool Bomb::isActive() const {
    return active;
}

void Bomb::screen_to_world(float screen_x, float screen_y, float& out_world_x, float& out_world_y) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 vp_pos = vp->Pos;
    const ImVec2 vp_size = vp->Size;
    
    float range_x = world_max_x - world_min_x;
    float range_y = world_max_y - world_min_y;
    
    if (range_x <= 0.0f || range_y <= 0.0f)
    {
        out_world_x = 0.0f;
        out_world_y = 0.0f;
        return;
    }
    
    // Normalize screen coordinates relative to viewport
    float norm_x = (screen_x - vp_pos.x) / vp_size.x;
    float norm_y = (screen_y - vp_pos.y) / vp_size.y;
    
    // Convert to world coordinates
    out_world_x = world_min_x + norm_x * range_x;
    out_world_y = world_max_y - norm_y * range_y; // Invert Y like in rendering
}

void Bomb::resolveTerrainCollision() {

    const int maxIterations = 5;

    for (int i = 0; i < maxIterations; i++) {
        bool collided = false;

        const int samples = 8;

        for (int i = 0; i < samples; i++) {
            float angle = (2.0f * 3.1415926f * i) / samples;

            float px = x + cos(angle) * radius;

            float py = y + sin(angle) * radius;

            if (terrain->is_filled(px, py)) {
                std::cout << "[BOMB] Collision event!" << std::endl;
                
                if (!timedExplosion) {
                    hitsToExplode--;

                    if (hitsToExplode <= 0) {
                        explode();
                    }
                }

                collided = true;
                break;
            }

            
        }

        if (!collided) {
            break;
        }

        handleCollision();

    }

    
}

void Bomb::handleCollision() {
    float nx = 0;
    float ny = 0;

    computeTerrainNormal(nx, ny);

    // Dot product
    float dot = vx * nx + vy * ny;

    if (dot < 0) {
        // Normal bounce 
        vx -= 2.0f * dot * nx;
        vy -= 2.0f * dot * ny;

        vx *= bounceStrength;
        vy *= bounceStrength;
        
        float tx = -ny;
        float ty = nx;
        float tDot = vx * tx + vy * ty;

        vx -= tDot * tx * 0.2f;
        vy -= tDot * ty * 0.2f;

        if (abs(dot) < 15.0f && bounceStrength < 0.9f) {
            // Apply heavy damping if it's barely bouncing anymore
            vx *= 0.5f;
            vy *= 0.5f;
        }
    }
    x += nx * 2.5f;
    y += ny * 2.5f;
}


void Bomb::computeTerrainNormal(float& nx, float& ny)
{
    nx = 0;
    ny = 0;

    const int samples = 8;

    for (int i = 0; i < samples; i++)
    {
        float angle =
            (2.0f * 3.1415926f * i) / samples;

        float px =
            x - cos(angle) * radius;

        float py =
            y - sin(angle) * radius;

        if (terrain->is_filled(px, py))
        {
            nx += cos(angle);
            ny += sin(angle);
        }
    }

    float length =
        sqrt(nx*nx + ny*ny);

    if (length > 0.0001f)
    {
        nx /= length;
        ny /= length;
    }
    else
    {
        nx = 0;
        ny = -1;
    }
}