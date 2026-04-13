cmake_minimum_required(VERSION 3.14)
project(Quadtree)

# Fix FetchContent timestamp robustness warnings on newer CMake
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use FetchContent to automatically download dependencies
include(FetchContent)

# --- SDL2 ---
# On Linux, prefer system-installed SDL2 if available. On Windows/macOS,
# it's more reliable to fetch it.
if(UNIX AND NOT APPLE)
    find_package(SDL2 REQUIRED)
    set(SDL2_FETCHED_SOURCE FALSE)
else()
    # Fetch SDL2
    FetchContent_Declare(SDL2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.28.5/SDL2-2.28.5.tar.gz
        SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/sdl2-src"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(SDL2_FETCHED_SOURCE TRUE)
endif()

# --- ImGui ---
# Fetch ImGui
FetchContent_Declare(imgui
    # Docking (IDE-like snapping) lives on the 'docking' branch
    URL https://github.com/ocornut/imgui/archive/refs/heads/docking.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/imgui-src"
)

if(SDL2_FETCHED_SOURCE)
    FetchContent_MakeAvailable(SDL2 imgui)
else()
    FetchContent_MakeAvailable(imgui)
endif()

# Find OpenGL (portable target names differ across platforms)
if(WIN32 AND NOT MSVC)
    # For MinGW, use direct OpenGL library
    set(OPENGL_LIBRARIES opengl32)
else()
    find_package(OpenGL REQUIRED)
    if(TARGET OpenGL::OpenGL)
        set(OPENGL_LIBRARIES OpenGL::OpenGL)
    else()
        # Common on Linux/Windows
        set(OPENGL_LIBRARIES OpenGL::GL)
    endif()
endif()

# ImGui source files
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Source files
set(SOURCES
    src/main.cpp
    src/gui.cpp
    src/quadtree.cpp
    src/terrain.cpp
    src/bomb.cpp
    ${IMGUI_SOURCES}
)

# Create executable
add_executable(quadtree ${SOURCES})

# Link libraries - SDL_MAIN_HANDLED is defined, so we don't need SDL2main
target_link_libraries(quadtree
    SDL2::SDL2
    ${OPENGL_LIBRARIES}
)

target_compile_definitions(quadtree PRIVATE SDL_MAIN_HANDLED)

# If FetchContent swaps/updates ImGui sources but extracted timestamps don't
# reflect content changes, build systems can keep stale .obj files. Fingerprint
# the ImGui content and inject it as a compile definition so any ImGui update
# reliably triggers recompilation of all ImGui .cpp files.
set(_imgui_fingerprint_input "")
foreach(_f IN ITEMS
    imgui.cpp
    imgui_draw.cpp
    imgui_widgets.cpp
    imgui_tables.cpp
    imgui_demo.cpp
    imgui.h
    imgui_internal.h
    imconfig.h
    backends/imgui_impl_sdl2.cpp
    backends/imgui_impl_opengl3.cpp
)
    if(EXISTS "${imgui_SOURCE_DIR}/${_f}")
        file(SHA1 "${imgui_SOURCE_DIR}/${_f}" _imgui_file_sha1)
        string(APPEND _imgui_fingerprint_input "${_imgui_file_sha1}")
    endif()
endforeach()
string(SHA1 IMGUI_FINGERPRINT "${_imgui_fingerprint_input}")
target_compile_definitions(quadtree PRIVATE IMGUI_FINGERPRINT=\"${IMGUI_FINGERPRINT}\")

# --- Runtime DLL copying (Windows) ---
# When using MinGW/MSYS2 toolchains, executables often depend on runtime DLLs that
# are not in the PATH of VS Code's CMake Tools run environment.
if(WIN32)
    # Copy SDL2 runtime DLL next to the executable (when SDL2 is built shared)
    if(TARGET SDL2::SDL2)
        add_custom_command(TARGET quadtree POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL2::SDL2>
                $<TARGET_FILE_DIR:quadtree>
            VERBATIM
        )
    endif()

    # Copy common MinGW runtime DLLs when using MinGW
    if(MINGW)
        get_filename_component(_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(_dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            if(EXISTS "${_MINGW_BIN_DIR}/${_dll}")
                add_custom_command(TARGET quadtree POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_MINGW_BIN_DIR}/${_dll}"
                        $<TARGET_FILE_DIR:quadtree>
                    VERBATIM
                )
            endif()
        endforeach()
    endif()
endif()

# Include directories
target_include_directories(quadtree PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${SDL2_INCLUDE_DIRS}
)

# Platform-specific settings
if(APPLE)
    # macOS specific settings
    set_target_properties(quadtree PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.quadtree.app
        MACOSX_BUNDLE_BUNDLE_NAME "Quadtree"
    )
elseif(WIN32)
    # Windows specific settings
    set_target_properties(quadtree PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()
