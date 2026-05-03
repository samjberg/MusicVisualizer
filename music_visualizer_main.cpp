/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "SDL3/SDL_render.h"
#include <filesystem>
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>
#include <filesystem>
#include <vector>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

namespace fs = std::filesystem;

typedef unsigned char uchar;

// fs::path get_project_root(fs::path exepath) {
//     fs::path curr_path = exepath;
//     while (curr_path.string().size() > 3) {
//         if (fs::is_directory(curr_path)) {
//             for (const auto &direntry : fs::directory_iterator(curr_path)) {
//                 if (direntry.path().filename().string() == ".git") {
//                     cout << "FOUND .GIT in: " << direntry.path() << endl;
//                     return curr_path;
//                 }
//             }
//         }
//         curr_path = curr_path.parent_path();
//     }
//     return curr_path;
// }




// void drawImg(PPM img) {
//     size_t width = img.get_width();
//     size_t height = img.get_height();
//     SDL_RenderClear(renderer);
//     for (size_t y=0; y<height; ++y) {
//         for (size_t x=0; x<width; ++x) {
//             color c = img.get(x, y);
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


/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    // fs::path imgpath = "/Users/sjber/Coding/C++/SDL_Projects/PPM_Viewer/img2.ppm";
    fs::path cwd = fs::current_path();
    fs::path argpath = argv[1];
    fs::path imgpath = argpath.is_absolute() ? argpath : cwd / argpath;
    size_t width = 600;
    size_t height = 400;
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
        if (event->key.key == 'f') {
            return SDL_APP_SUCCESS;
        }

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

    int w = 600;
    int h = 400;


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
    SDL_RenderPresent(renderer);

    /* Draw the loaded PPM image */


    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}
