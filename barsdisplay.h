#ifndef BARSDISPLAY_H
#define BARSDISPLAY_H

// #include "bar.h"
#include <cstdint>
#include <vector>
#include <fstream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "SDL3/SDL_render.h"
#include <SDL3/SDL_rect.h>
#include "colorutils.h"




struct Pos {
    int32_t x;
    int32_t y;
};

inline std::ostream& operator<<(std::ostream& out, Pos p) {
        out << "(" << p.x << ", " << p.y << ")";
        return out;
}
inline const bool operator==(Pos& left, Pos& right) {return (left.x == right.x) && (left.y == right.y);}

typedef Pos Size;
typedef SDL_FRect Rect;


//This bar struct is meant very specifically for use with BarsDisplay.  It really has no use otherwise, as it is index based
struct Bar {
    float height = 0.0; //the height of the bar, normalized to the range [0.0, 1.0]
    Color color = Color{255, 0, 0, 0}; //I think you can figure this one out (red by default)
};

struct ScreenInfo {
    Size screen_size;
    float render_scale = 1.0;
};


inline std::vector<Bar> create_bars(std::vector<float>& heights) {
    std::vector<Bar> bars(heights.size());
    for (int i=0; i<heights.size(); ++i) {
        bars[i] = Bar{heights[i]};
    }
    return bars;
}

inline float mylerp(float oldval, float target, float a) {
    return oldval + (a * (target - oldval));
}

class BarsDisplay {
    public:
        int32_t count;
        float lerp_t;

        BarsDisplay(ScreenInfo screen_info, std::vector<Bar> bars) : screen_size(screen_info.screen_size), bars(bars) {
            render_scale = screen_info.render_scale;
            screen_width = screen_size.x;
            screen_height = screen_size.y;
            count = bars.size();
            bar_width = screen_width / count;
            // gradient = std::vector<Color>({RED, GREEN, BLUE});
            gradient = create_gradient(GREEN, RED, screen_height);
            lerp_t = 0.4;
        }
        BarsDisplay(ScreenInfo screen_info, std::vector<Bar> bars, GradientInfo grad_info) : screen_size(screen_info.screen_size), bars(bars) {
            render_scale = screen_info.render_scale;
            screen_width = screen_size.x;
            screen_height = screen_size.y;
            count = bars.size();
            bar_width = screen_width / count;
            gradient = create_gradient(grad_info);
            lerp_t = 0.4;
        }
        BarsDisplay(Size screen_size, std::vector<Bar> bars) : screen_size(screen_size), bars(bars) {
            render_scale = 1.0;
            screen_width  = screen_size.x;
            screen_height = screen_size.y;
            count = bars.size();
            bar_width = screen_width / count;
            lerp_t = 0.4;
        }

        BarsDisplay(Size screen_size, int32_t count) : screen_size(screen_size), count(count) {
            render_scale = 1.0;
            screen_width = screen_size.x;
            screen_height = screen_size.y;
            bars = std::vector<Bar>(count, Bar{0.0, Color(255, 0, 0, 255)});
            lerp_t = 0.4;
        }

        BarsDisplay() = default;


        void update_heights(std::vector<double> heights) {
            double t = lerp_t;//0.4; //time value for lerp
            for (int i=0; i<count; ++i) {
                bars[i].height = mylerp(bars[i].height, heights[i], lerp_t);
            }
        }

        void decay_heights(double factor = 0.99) {
            for (int i=0; i<count; ++i) {
                float new_val = bars[i].height * factor;
                // bars[i].height = std::lerp(bars[i].height, new_val, 0.05);
                bars[i].height = std::lerp(bars[i].height, new_val, 0.05);

            }
        }

        void update_screen_size(Size new_screen_size) {
            screen_width  = new_screen_size.x;
            screen_height = new_screen_size.y;
            bar_width = screen_width / count;
        }

        void update_screen_info(ScreenInfo new_screen_info) {
            update_screen_size(new_screen_info.screen_size);
            render_scale = new_screen_info.render_scale;
        }

        //Returns the SDL_FRect representing the bar at index idx, ready to be passed into SDL_RenderRect
        Rect getRect(int32_t idx) {
            Bar& b = bars[idx];
            //First calculate x, y, w, h without taking into account render_scale
            float x = idx * bar_width;
            //b.height is normalized in the range 0-1.  1.0-b.height is just handling the flipping at the earliest and simplest place.
            float y = (1.0 - b.height) * screen_height;
            float w = bar_width;
            float h = b.height * screen_height;
            return Rect{x/render_scale, y/render_scale, w/render_scale, h/render_scale};
        }

        //Renders the entire BarsDisplay object
        // void render(SDL_Renderer *renderer) {
        //     for (int i=0; i<bars.size(); ++i) {
        //         Rect rect = getRect(i);
        //         uint8_t r = gradient[i].r;
        //         uint8_t g = gradient[i].g;
        //         uint8_t b = gradient[i].b;
        //         uint8_t a = gradient[i].a;
        //         SDL_SetRenderDrawColor(renderer, r, g, b, a);
        //         SDL_RenderFillRect(renderer, &rect);
        //
        //     }
        // }

        //gradient_style options are "vertical" for volume based gradient, and "horizontal" for a simple gradient across the bins
        void render(SDL_Renderer *renderer, std::string gradient_style="vertical") {
            for (int i=0; i<bars.size(); ++i) {
                Rect rect = getRect(i);
                uint8_t r = gradient_style == "vertical" ? gradient[rect.h].r : gradient[i].r;
                uint8_t g = gradient_style == "vertical" ? gradient[rect.h].g : gradient[i].g;
                uint8_t b = gradient_style == "vertical" ? gradient[rect.h].b : gradient[i].b;
                uint8_t a = gradient_style == "vertical" ? gradient[rect.h].a : gradient[i].a;
                SDL_SetRenderDrawColor(renderer, r, g, b, a);
                SDL_RenderFillRect(renderer, &rect);

            }
        }



    private:
        Size screen_size; //The size of the screen, duh
        float render_scale; //The factor by which the render is scaled up
        int32_t screen_width, screen_height; //figure it out
        int32_t bar_width; //The width of each bar in window pixels
        std::vector<Bar> bars; //list of the actual bar objects (structs) that are displayed
        std::vector<float> prev_heights;
        Gradient gradient; //the gradient used across the bars (horizontally) to determine bar color


};


#endif
