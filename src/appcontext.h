#pragma once
#include <memory>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>


#include "quadtree.h"
#include "bomb.h"

class Player;

// Application context structure
struct AppContext
{
    SDL_Window* window;
    SDL_GLContext gl_context;
    std::unique_ptr<Quadtree> terrain;
    std::string loaded_terrain_file;
    int loaded_terrain_depth;
    int last_depth_reload_attempt;
    
    // Cached world bounds for coordinate transformation
    float world_min_x = 0.0f;
    float world_max_x = 0.0f;
    float world_min_y = 0.0f;
    float world_max_y = 0.0f;

    std::vector<Bomb> bombs;
    std::unique_ptr<Player> player;
};
