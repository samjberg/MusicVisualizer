#ifndef BARSDISPLAY_H
#define BARSDISPLAY_H

// #include "bar.h"
#include <iostream>
#include <algorithm>
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
    float height = 0.0; //the actual current height of the bar, normalized to the range [0.0, 1.0]
    float target_height = 0.0; //the target height, height will approach target height over time, also normalized to [0.0, 1.0]
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
        std::string gradient_style;

        BarsDisplay(ScreenInfo screen_info, std::vector<Bar> bars, float rise_speed=12.0, float fall_speed=8.0) 
            : screen_size(screen_info.screen_size), bars(bars), rising_speed(rise_speed), falling_speed(fall_speed) 
        {
            render_scale = screen_info.render_scale;
            screen_width = screen_size.x;
            screen_height = screen_size.y;
            count = bars.size();
            bar_width = screen_width / count;
            // gradient = std::vector<Color>({RED, GREEN, BLUE});
            gradient_style = "vertical";
            gradient = create_gradient(GREEN, RED, screen_height);
            lerp_t = 0.4;
        }
        BarsDisplay(ScreenInfo screen_info, std::vector<Bar> bars, GradientInfo grad_info, float rise_speed=12.0, float fall_speed=8.0) 
            : screen_size(screen_info.screen_size), bars(bars), rising_speed(rise_speed), falling_speed(fall_speed) 
        {
            render_scale = screen_info.render_scale;
            screen_width = screen_size.x;
            screen_height = screen_size.y;
            count = bars.size();
            bar_width = screen_width / count;
            gradient_style = grad_info.style;
            gradient = create_gradient(grad_info);
            lerp_t = 0.4;
        }
        BarsDisplay(Size screen_size, std::vector<Bar> bars, float r_scale=4.0, float rise_speed=12.0, float fall_speed=8.0)
            : screen_size(screen_size), bars(bars), rising_speed(rise_speed), falling_speed(fall_speed)
        {
            render_scale = r_scale;
            screen_width  = screen_size.x;
            screen_height = screen_size.y;
            count = bars.size();
            bar_width = screen_width / count;
            lerp_t = 0.4;
        }

        BarsDisplay(Size screen_size, int32_t count, float r_scale=4.0, float rise_speed=12.0, float fall_speed=8.0)
            : screen_size(screen_size), count(count), rising_speed(rise_speed), falling_speed(fall_speed)
        {
            render_scale = r_scale;
            screen_width = screen_size.x;
            screen_height = screen_size.y;
            bars = std::vector<Bar>(count, Bar{0.0, 0.0, Color(0, 255, 0, 255)});
            gradient = create_gradient(GREEN, RED, screen_height);
            lerp_t = 0.4;
        }

        BarsDisplay() = default;


        //NOTE: This function is outdated, but I am temporarily keeping it in the code just in case
        void __update_heights(std::vector<double>& heights) {
            double t = lerp_t;//0.4; //time value for lerp
            for (int i=0; i<count; ++i) {
                //I know this is hard to read.  It is just a ternary assignment statement where if the bar is rising (height[i]>bars[i].height)
                //it uses a higher (faster) lerp value.  So the bar can jump up quickly.  If the bar is falling, then it uses the actual
                //lerp_t member variable instead, which results in a much slower interpolation, so the bars fall slower than they rise
                bars[i].height = heights[i]>bars[i].height ? 
                    mylerp(bars[i].height, std::clamp(heights[i], 0.0, 1.0), fmin(fmax(0.75, 1.0-lerp_t), 0.85)) : 
                    mylerp(bars[i].height, std::clamp(heights[i], 0.0, 1.0), lerp_t);
            }
        }

        void update_heights(std::vector<double>& target_heights) {
            for (int i=0; i<count; ++i) {
                bars[i].target_height = std::clamp(target_heights[i], 0.0, 1.0);
            }
        }

        //Run this function every iteration of SDL_AppIterate, it handles smoothly interpolating bar heights towards their target height
        void process_visual_frame(float elapsed_seconds) {
            // std::cout << "Processing visual frame with elapsed_seconds: " << elapsed_seconds << std::endl;
            // float speed = 8.0;
            for (int i=0; i<count; ++i) {
                // float new_height = bars[i].height == bars[i].target_height ?
                float new_height = bars[i].target_height >= bars[i].height ? 
                    mylerp(bars[i].height, bars[i].target_height, rising_speed * elapsed_seconds) :
                    mylerp(bars[i].height, bars[i].target_height, falling_speed * elapsed_seconds);
                bars[i].height = new_height;

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


        //gradient_style options are "vertical" for volume based gradient, and "horizontal" for a simple gradient across the bins
        void render(SDL_Renderer *renderer) {
            for (int i=0; i<bars.size(); ++i) {
                Rect rect = getRect(i);
                uint64_t color_height = static_cast<uint64_t>(bars[i].height * (gradient.size()-1));
                uint8_t r = gradient_style == "vertical" ? gradient[color_height].r : gradient[i].r;
                uint8_t g = gradient_style == "vertical" ? gradient[color_height].g : gradient[i].g;
                uint8_t b = gradient_style == "vertical" ? gradient[color_height].b : gradient[i].b;
                uint8_t a = gradient_style == "vertical" ? gradient[color_height].a : gradient[i].a;
                SDL_SetRenderDrawColor(renderer, r, g, b, a);
                SDL_RenderFillRect(renderer, &rect);

            }
        }



    private:
        Size screen_size;    //The size of the screen, duh
        float render_scale;  //The factor by which the render is scaled up
        float rising_speed;  //The speed factor that affects the temporal lerp of how fast bars rise
        float falling_speed; //The speed factor that affects the temporal lerp of how fast bars fall
        int32_t screen_width, screen_height; //figure it out
        int32_t bar_width; //The width of each bar in window pixels
        std::vector<Bar> bars; //list of the actual bar objects (structs) that are displayed
        std::vector<float> prev_heights;
        Gradient gradient; //the gradient used across the bars (horizontally) to determine bar color
};


#endif
