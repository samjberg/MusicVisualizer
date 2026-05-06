/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
// #include "SDL/src/SDL_internal.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_render.h"
#include "audiostream.h"
#include <SDL3/SDL_rect.h>
#include <filesystem>
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include "barsdisplay.h"
#include "fft.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
int actual_screen_width = 0;
int actual_screen_height = 0;

namespace fs = std::filesystem;

typedef unsigned char uchar;

fs::path get_project_root(fs::path exepath) {
    fs::path curr_path = exepath;
    while (curr_path.string().size() > 3) {
        if (fs::is_directory(curr_path)) {
            for (const auto &direntry : fs::directory_iterator(curr_path)) {
                if (direntry.path().filename().string() == ".git") {
                    cout << "FOUND .GIT in: " << direntry.path() << endl;
                    return curr_path;
                }
            }
        }
        curr_path = curr_path.parent_path();
    }
    return curr_path;
}


uint64_t closest_pow2(uint64_t x) {
    uint64_t curr = 2;
    for (uint64_t i=1; i<50; ++i) {
        curr = pow(2, i);
        if (curr > x) {
            return curr;
        }
    }
    return curr;
}

vector<complex<double>> channel_to_complex(vector<Frame>& frames, int channel) {
    vector<complex<double>> lst;
    uint64_t curr_len = frames.size();
    uint64_t closest_pow = closest_pow2(curr_len);
    // lst.reserve(closest_pow);
    int i=0;
    double half_pow2_16 = pow(2.0, 16) / 2;
    for (; i<curr_len; ++i) {
        double val = static_cast<double>(frames[i].channels[channel]) / half_pow2_16;
        lst.push_back(complex<double>(val, 0.0));
    }

    //zero pad lst until we reach the closest power of 2 in length.  Necessary for fft
    while (i < closest_pow) {
        lst.push_back(complex<double>(0.0, 0.0));
        i += 1;
    }
    return lst;
}

double max_chunk_val(vector<Frame> frames) {
    double max_val = 0;
    for (Frame frame : frames) {
        for (int c=0; c<frame.num_channels; ++c) {
            if (frame.channels[c] > max_val)
                max_val = frame.channels[c];
        }
    }
    return max_val;
}


// void drawImg(PPM img) {
//     size_t width = img.get_width();
//     size_t height = img.get_height();
//     SDL_RenderClear(renderer);
//     for (size_t y=0; y<height; ++y) {
//         for (size_t x=0; x<width; ++x) {
            // color c = img.get(x, y);
//             uchar r = c.x();
//             uchar g = c.y();
//             uchar b = c.z();
//             SDL_SetRenderDrawColor(renderer, r, g, b, 255);
//             SDL_RenderPoint(renderer, static_cast<float>(x), static_cast<float>(y));
//         }
//     }
//
//     SDL_RenderPresent(renderer);
// }

BarsDisplay bd;
AudioStream audio_stream("/Users/sjber/Coding/C++/SDL_Projects/MusicVisualizer/footstepswav.wav", 4096);

Size get_screen_size() {
    int w, h;
    SDL_GetRenderOutputSize(renderer, &w, &h);
    return Size{w, h};
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    // fs::path imgpath = "/Users/sjber/Coding/C++/SDL_Projects/PPM_Viewer/img2.ppm";
    fs::path cwd = fs::current_path();
    fs::path argpath = argv[1];
    fs::path imgpath = argpath.is_absolute() ? argpath : cwd / argpath;
    int width = 600;
    int height = 400;
    Size screen_size{width, height};
    ScreenInfo screen_info{screen_size, 4.0};
    vector<Frame> chunk = audio_stream.read_next_chunk();
    for (int i=0; i<20; ++i) {
        chunk = audio_stream.read_next_chunk();
    }
    double max_val = max_chunk_val(chunk);
    cout << "max_val: " << max_val << endl;
    vector<complex<double>> vals = channel_to_complex(chunk, 0);
    vector<complex<double>> freq_data = fft(vals);
    std::vector<Bar> bars;
    std::vector<Color> colors = {Color{255, 0, 0, 255}, Color{0, 255, 0, 255}, Color{0, 0, 255, 255}};
    for (int i=0; i<100; i++) {//vals.size(); i++) {
        // float h = i / 20.0;//static_cast<float>(vals.size());
        float h = 17 * freq_data[i].real() / max_val;
        cout << "h: " << h << endl;
        Color c = colors[i%3];
        bars.push_back(Bar{h, c});
    }
    bd = BarsDisplay(screen_info, bars);
    // size_t width = img.get_width();
    // size_t height = img.get_height();
    // *appstate = new stateinfo{int(width), int(height), img};
    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("Hello World", width, height, SDL_WINDOW_FULLSCREEN, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.key == 'q') {
            return SDL_APP_SUCCESS;
        }
        else if (event->key.key == 'd') {
            vector<Frame> chunk = audio_stream.read_next_chunk();

        }
    }
    else if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        Size new_screen_size = get_screen_size();
        bd.update_screen_size(new_screen_size);
    }
    else if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{

    const char *message = "Hello World!";
    // int w = img.get_width(), h = img.get_height();
    float x, y;
    const float scale = 4.0f;

    int w = actual_screen_width  != 0 ? actual_screen_width  : 600;
    int h = actual_screen_height != 0 ? actual_screen_height : 400;
    // SDL_FRect rect{250, 325, 100, 50};
    // SDL_Rect

    // SDL_Color c;


    /* Center the message and scale it up */
    SDL_GetRenderOutputSize(renderer, &w, &h);
    SDL_SetRenderScale(renderer, scale, scale);
    x = ((w / scale) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(message)) / 2;
    y = ((h / scale) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2;

    /* Draw the message */
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderDebugText(renderer, x, y, message);
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    // SDL_RenderRect(renderer, &rect);

    bd.render(renderer);

    SDL_RenderPresent(renderer);


    /* Draw the loaded PPM image */


    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}
