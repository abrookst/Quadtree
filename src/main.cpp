#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #include <fcntl.h>
#endif

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include "gui.h"

void gui_render();

// Application context structure
struct AppContext
{
    SDL_Window* window;
    SDL_GLContext gl_context;
};

// Forward declarations
bool app_init(AppContext& app);
void app_run(AppContext& app);
void app_cleanup(AppContext& app);

bool app_init(AppContext& app)
{
    std::cout << "[START] Quadtree application starting..." << std::endl;
    std::cout.flush();

    try
    {
        // Initialize SDL
        std::cout << "[INIT] Initializing SDL..." << std::endl;
        std::cout.flush();
        
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            std::cerr << "[ERROR] Failed to initialize SDL: " << SDL_GetError() << std::endl;
            std::cerr.flush();
            return false;
        }
        std::cout << "[OK] SDL initialized" << std::endl;
        std::cout.flush();

        // GL attributes
        std::cout << "[INIT] Setting OpenGL attributes..." << std::endl;
        std::cout.flush();
        
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        std::cout << "[OK] OpenGL attributes set" << std::endl;
        std::cout.flush();

        // Create window
        std::cout << "[INIT] Creating window..." << std::endl;
        std::cout.flush();
        
        app.window = SDL_CreateWindow(
            "Quadtree Visualization",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            1280, 720,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
        );

        if (!app.window)
        {
            std::cerr << "[ERROR] Failed to create SDL window: " << SDL_GetError() << std::endl;
            std::cerr.flush();
            SDL_Quit();
            return false;
        }
        std::cout << "[OK] SDL window created (1280x720)" << std::endl;
        std::cout.flush();

        // Create OpenGL context
        std::cout << "[INIT] Creating OpenGL context..." << std::endl;
        std::cout.flush();
        
        app.gl_context = SDL_GL_CreateContext(app.window);
        if (!app.gl_context)
        {
            std::cerr << "[ERROR] Failed to create OpenGL context: " << SDL_GetError() << std::endl;
            std::cerr.flush();
            SDL_DestroyWindow(app.window);
            SDL_Quit();
            return false;
        }
        std::cout << "[OK] OpenGL context created" << std::endl;
        std::cout.flush();

        // Enable vsync
        SDL_GL_SetSwapInterval(1);

        // Setup Dear ImGui context
        std::cout << "[INIT] Initializing ImGui..." << std::endl;
        std::cout.flush();
        
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Docking disabled

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        const char* glsl_version = "#version 330";
        ImGui_ImplSDL2_InitForOpenGL(app.window, app.gl_context);
        ImGui_ImplOpenGL3_Init(glsl_version);
        std::cout << "[OK] ImGui initialized" << std::endl;
        std::cout << "\n[RUN] ============================================" << std::endl;
        std::cout << "[RUN]  Application Running" << std::endl;
        std::cout << "[RUN]  Press ESC or close window to exit" << std::endl;
        std::cout << "[RUN] ============================================\n" << std::endl;
        std::cout.flush();

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n[EXCEPTION] Initialization failed: " << e.what() << std::endl;
        std::cerr.flush();
        return false;
    }
}

void app_run(AppContext& app)
{
    bool running = true;
    SDL_Event event;

    // Main loop
    while (running)
    {
        // Read in keys
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            
            switch (event.type)
            {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE)
                        running = false;
                    break;
            }
        }

        // Check for console commands
        std::string command = ConsoleBuffer::get_command();
        if (!command.empty())
        {
            std::cout << "[COMMAND] Processing: " << command << std::endl;
            // TODO: Add any user commands here (file reading, mostly. )
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Render GUI
        gui_render();

        // Rendering
        ImGui::Render();
        
        // Clear and render scene
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render ImGui draw data
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        SDL_GL_SwapWindow(app.window);
    }
}

void app_cleanup(AppContext& app)
{
    std::cout << "\n[CLEANUP] Shutting down..." << std::endl;
    std::cout.flush();

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // Cleanup SDL
    SDL_GL_DeleteContext(app.gl_context);
    SDL_DestroyWindow(app.window);
    SDL_Quit();

    std::cout << "[EXIT] Application closed cleanly" << std::endl;
    std::cout.flush();
}

int main(int argc, char* argv[])
{
    // Setup console capture to ImGui
    static ConsoleBuffer console_buffer;
    std::cout.rdbuf(&console_buffer);
    std::cerr.rdbuf(&console_buffer);

    AppContext app = { nullptr, nullptr };

    try
    {
        if (!app_init(app))
            return -1;
        app_run(app);
        app_cleanup(app);
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n[EXCEPTION] Unhandled exception: " << e.what() << std::endl;
        std::cerr.flush();
        return -1;
    }
    catch (...)
    {
        std::cerr << "\n[EXCEPTION] Unknown error occurred" << std::endl;
        std::cerr.flush();
        return -1;
    }

    return 0;
}
