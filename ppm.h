#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "color.h"

namespace fs = std::filesystem;
using namespace std;





class PPM {
    public:
        PPM(fs::path path) {
            load(path);
        }

        PPM() {};

        void load(fs::path path) {
            ifstream f(path);
            string curr_line;

            //Read first line, ensure it is 'P3'
            getline(f, curr_line);
            if (!curr_line.starts_with("")) {
                cerr << "Error: First line is not 'P3'\n";
            }

            //Read second line, ensure width and height are read correctly
            getline(f, curr_line);

            int width_check = -1;
            int height_check = -1;
            stringstream ss(curr_line);
            ss >> width_check >> height_check;
            if ((width_check == -1) || (height_check == -1)) {
                cerr << "Error: failed to read width and/or height from file\n";
            }
            width = static_cast<size_t>(width_check);
            height = static_cast<size_t>(height_check);
            total_size = width * height;


            //Read third line, ensure it is 255
            getline(f, curr_line);
            ss = stringstream(curr_line);
            int num255;
            ss >> num255;
            if (num255 != 255) {
                cerr << "Error: line that is supposed to be '255' is not '255'";
            }

            //actually load in the color data
            while (getline(f, curr_line)) {
                ss = stringstream(curr_line);
                int ri, gi, bi;
                uchar r, g, b;
                ss >> ri >> gi >> bi;
                r = static_cast<uchar>(ri);
                g = static_cast<uchar>(gi);
                b = static_cast<uchar>(bi);
                data.emplace_back(color(r, g, b));

            }
        }

        size_t get_width() {return this->width;}
        size_t get_height() {return this->height;}

        color& get(int x, int y) {
            int i = y*width + x;
            return data[i];
        }

        void set(int x, int y, color val) {
            int i = y*width + x;
            data[i] = val;
        }

        void set(int x, int y, uchar r, uchar g, uchar b) {
            int i = y*width + x;
            data[i] = color(r, g, b);
        }



    private:
        size_t width, height, total_size;
        vector<color> data;



};
