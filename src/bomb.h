#pragma once

#include "imgui.h"
#include "quadtree.h"
#include <vector>
#include <utility>
#include <glm/glm.hpp>


struct AppContext;

class Bomb {

public:

    // Constructor
    Bomb(float x, float y, float radius, bool timedExplosion, float explodeTime, int hitsToExplode, AppContext& app);

    // Update physics
    void update(float dt);

    // Get and Edit properties
    void setGravity(float g) { gravity = g; }
    void setBounceStrength(float b) { bounceStrength = b; }
    void setExplodeRadius(float r) { explodeRadius = r; }
    void setExplodeTime(float t) { explodeTime = t; }

    float getRadius() const { return radius; }
    void setRadius(float r) { radius = r; }

    // Draw entity
    void draw(ImDrawList* drawList, ImVec2 screenPos, float screenRadius);

    // Set Velocity
    void setVelocity(float vx, float vy);

    // Get Position
    float getX() const;
    float getY() const;

    // Explode Bomb
    void explode();

    // Set explosion polygon (optional, uses circle if not set)
    void setExplosionPolygon(const std::vector<glm::vec2>& polygon) { explosionPolygon = polygon; }

    // Check if it exists in the level
    bool isActive() const;

    // Helper function to convert screen pos to world pos
    void screen_to_world(float screen_x, float screen_y, float& out_world_x, float& out_world_y);

    // Get Trail
    const std::vector<std::pair<float, float>>& getTrail() const { return trail; }

    // Collision Resolver
    void resolveTerrainCollision();

    // Collision Handler
    void handleCollision();

    // Calculate Normal
    void computeTerrainNormal(float& nx, float& ny);

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

    float gravity;

    bool timedExplosion;

    float explodeTime;
    float explodeRadius = 10.0f;

    int hitsToExplode;
    int startHits;

    float lifeTimer;

    Quadtree* terrain;

    float world_min_x = 0.0f;
    float world_max_x = 0.0f;
    float world_min_y = 0.0f;
    float world_max_y = 0.0f;

    std::vector<std::pair<float, float>> trail;
    float trailTimer;

    // active state
    bool active;

    // Explosion polygon (world space vertices)
    std::vector<glm::vec2> explosionPolygon;

};