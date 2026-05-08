#ifndef COLORUTILS_H
#define COLORUTILS_H
#include <cmath>
#include <vector>
#include <cstdint>
#include <fstream>


struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

inline Color RED{255, 0, 0, 255};
inline Color GREEN{0, 255, 0, 255};
inline Color BLUE{0, 0, 255, 255};
inline Color WHITE{255, 255, 255, 255};
inline Color BLACK{0, 0, 0, 255};

inline std::ostream& operator<<(std::ostream& out, Color c) {
        out << "(" << int(c.r) << ", " << int(c.g) << ", " << int(c.b) << ")";
        return out;
}


//This is just a helper struct for use in this file.  It is NOT meant for representing colors in the range 0.0 - 1.
//It is literally just for representing 8 bit colors, but just representing them as floats to give room for more precision
//to make implementing a gradient easier
struct ColorF {
    float r;
    float g;
    float b;
    float a;

    ColorF (float r, float g, float b, float a = 255.0) : r(r), g(g), b(b), a(a) {}
    ColorF (Color c) : r(float(c.r)), g(float(c.g)), b(float(c.b)), a(float(c.a)) {}

    const ColorF operator/(float val) {
        return ColorF{r/val, g/val, b/val, a};
    }

    const ColorF operator+(ColorF other) {
        return ColorF{r+other.r, g+other.g, b+other.b, a};
    }


    const ColorF operator-(ColorF other) {
        return ColorF{r-other.r, g-other.g, b-other.b, a};
    }
};

typedef std::vector<Color> Gradient;

struct GradientInfo {
    Color start;
    Color end;
    uint64_t len;

};

//Literally just perform component wise subtraction of c1 - c2, excluding the alpha channel if ingore_alpha is true
inline Color subtract_colors(Color c1, Color c2, bool ignore_alpha = true) {
    uint8_t r = c1.r - c2.r;
    uint8_t g = c1.g - c2.g;
    uint8_t b = c1.b - c2.b;
    uint8_t a = ignore_alpha ? c1.a : c1.a - c2.a;
    return Color{r, g, b, a};
}


inline ColorF subtract_colors(ColorF c1, ColorF c2, bool ignore_alpha = true) {
    float r = c1.r - c2.r;
    float g = c1.g - c2.g;
    float b = c1.b - c2.b;
    float a = ignore_alpha ? c1.a : c1.a - c2.a;
    return ColorF{r, g, b, a};
}

inline Color subtract_scalar(Color color, uint8_t val, bool ignore_alpha = true) {
    uint8_t r = color.r - val;
    uint8_t g = color.g - val;
    uint8_t b = color.b - val;
    uint8_t a = ignore_alpha ? color.a : color.a - val;
    return Color{r, g, b, a};

}

inline ColorF subtract_scalar(ColorF color, float val, bool ignore_alpha = true) {
    float r = color.r - val;
    float g = color.g - val;
    float b = color.b - val;
    float a = ignore_alpha ? color.a : color.a - val;
    return ColorF{r, g, b, a};

}


//Ensures a clamps all individual channels of a ColorF to the 8 bit range (0-255)
inline ColorF sanitize_colorf(ColorF color) {
    if (color.r < 0) 
        color.r = 0;
    else if (color.r > 255) 
        color.r = 255;

    if (color.g < 0) 
        color.g = 0;
    else if (color.g > 255) 
        color.g = 255;

    if (color.b < 0) 
        color.b = 0;
    else if (color.b > 255) 
        color.b = 255;


    if (color.a < 0) 
        color.a = 0;
    else if (color.a > 255) 
        color.a = 255;
    return color;
}


//Creates a gradient (vector<Color>) from start to stop of length len.
//For xample create_gradient({255, 0, 0}, {0, 0, 255}, len)
inline Gradient create_gradient(Color start, Color end, uint64_t len) {
    std::vector<Color> gradient(len);
    gradient[0] = start;
    ColorF curr_color{float(start.r), float(start.g), float(start.b), float(start.a)};
    //direction = (end - start) / len.  It acts as a vector pointing from start to end, with a magnitude such that it will take len steps
    //to traverse all the way from start to end.  Exactly what we want for a gradient
    ColorF direction = subtract_colors(ColorF(end), ColorF(start)) / len;
    for (int i=1; i<len-1; ++i) {
        curr_color = sanitize_colorf(curr_color + direction);
        gradient[i] = Color(uint8_t(curr_color.r), uint8_t(curr_color.g), uint8_t(curr_color.b), uint8_t(curr_color.a));
    }
    return gradient;
}


inline Gradient create_gradient(GradientInfo info) {
    Color start = info.start;
    Color end = info.end;
    uint64_t len = info.len;
    std::vector<Color> gradient(len);
    gradient[0] = start;
    ColorF curr_color{float(start.r), float(start.g), float(start.b), float(start.a)};
    //direction = (end - start) / len.  It acts as a vector pointing from start to end, with a magnitude such that it will take len steps
    //to traverse all the way from start to end.  Exactly what we want for a gradient
    ColorF direction = subtract_colors(ColorF(end), ColorF(start)) / len;
    for (int i=1; i<len-1; ++i) {
        curr_color = sanitize_colorf(curr_color + direction);
        gradient[i] = Color(uint8_t(curr_color.r), uint8_t(curr_color.g), uint8_t(curr_color.b), uint8_t(curr_color.a));
    }
    return gradient;
}

// class Gradient {
//     public:
//
// };

#endif
