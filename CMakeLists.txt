cmake_minimum_required(VERSION 3.16)
project(music_visualizer)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# This assumes the SDL source is available in ./SDL
add_subdirectory(include/SDL EXCLUDE_FROM_ALL)

add_library(miniaudio STATIC miniaudio_impl.cpp)
target_include_directories(miniaudio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Create your game executable target as usual
add_executable(music_visualizer music_visualizer_main.cpp)
target_compile_features(music_visualizer PRIVATE cxx_std_20)
target_link_libraries(music_visualizer PRIVATE SDL3::SDL3 miniaudio)


# add_executable(wavreader wavreader.cpp)
# target_compile_features(wavreader PRIVATE cxx_std_20)

add_executable(test test.cpp)
target_compile_features(test PRIVATE cxx_std_20)
target_link_libraries(test PRIVATE miniaudio)

# add_executable(test_runner test.cpp)
# target_compile_features(test_runner PRIVATE cxx_std_20)





# Link to the actual SDL3 library.
