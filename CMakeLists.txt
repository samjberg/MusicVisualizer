cmake_minimum_required(VERSION 3.16)
project(music_visualizer)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# This assumes the SDL source is available in vendored/SDL
add_subdirectory(SDL EXCLUDE_FROM_ALL)

# Create your game executable target as usual
add_executable(music_visualizer music_visualizer_main.cpp)

# Link to the actual SDL3 library.
target_link_libraries(music_visualizer PRIVATE SDL3::SDL3)
