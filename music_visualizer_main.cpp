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
// #include "SDL/src/SDL_internal.h"
// #include "SDL/src/SDL_internal.h"
#include "SDL3/SDL_events.h"
// #include "SDL3/SDL_oldnames.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_render.h"
#include <SDL3/SDL_audio.h>
#include "SDL3/SDL_scancode.h"
#include "audiostream.h"
#include <complex>
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
#include "parseargs.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static double global_max = 0.01;
static double global_min = 100.0;
static double freq_min = 40.0;
static double freq_max = 8000.0;
static bool playing = true;
static uint64_t sample_rate = 44100;
static uint64_t frames_per_chunk = 2048;
static string gradient_style = "vertical";
int actual_screen_width = 0;
int actual_screen_height = 0;
float* paused_data_buffer;
int paused_buffer_size = 0;

fs::path fpath = "/Users/sjber/Coding/C++/SDL_Projects/MusicVisualizer/cliffsofdover.wav";
// fs::path fpath = "/Users/sjber/Coding/C++/SDL_Projects/MusicVisualizer/footstepswav.wav";
BarsDisplay bd;
// AudioStream audio_stream(fpath, 2048);
AudioStream* audio_stream;
SDL_AudioStream* sdl_audio_stream;

//Global variables for keeping track of the current playhead
uint64_t total_frames_sent = 0; //total number of frames SENT to the audio device (not necessarily played yet)
uint64_t last_update_pos = 0;
uint64_t current_playhead = 0;
uint64_t num_bars = 32;
fs::path project_root;

namespace fs = std::filesystem;

typedef unsigned char uchar;

template<typename numT> struct Pair{
    numT first;
    numT second;
};

typedef Pair<double> Range;



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


template<typename numT>
numT vecmax(vector<numT>& vec) {
    numT max_val = 0;
    for (numT num : vec) {
        if (num > max_val) {
            max_val = num;
        }
    }
    return max_val;
}


template<typename numT>
Pair<numT> vecminmax(vector<numT>& vec) {
    numT max_val = 0;
    numT min_val = vec[0];
    for (numT num : vec) {
        if (num > max_val) {
            max_val = num;
        }
        if (num < min_val) {
            min_val = num;
        }
    }
    return Pair<numT>{min_val, max_val};
}

double convert_ranges(double x, Range curr_range, Range new_range) {
    return (((x - curr_range.first) * (new_range.second - new_range.first)) / (curr_range.second - curr_range.first)) + new_range.first;
}

vector<double> convert_vec_to_range(vector<double>& vec, Range curr_range, Range new_range) {
    vector<double> res(vec.size());
    for (int i=0; i<vec.size(); ++i) {
        res[i] = convert_ranges(vec[i], curr_range, new_range);
    }
    return res;
}


// template<typename numT>
// vector<numT> normalize_range_to_0_1(vector<numT> vec) {
//     vector<numT> res;
//
// }





template<typename numT>
vector<numT> divide_vector(vector<numT>& vec, numT divisor) {
    vector<numT> res(vec);
    for (int i=0; i<res.size(); ++i) {
        res[i] /= divisor;
    }
    return res;
}


vector<uint64_t> powers_of_2 = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};

bool is_low_pow2(uint64_t num) {
    for (uint64_t pow2 : powers_of_2) {
        if (num == pow2) {
            return true;
        }
    }
    return false;
}

uint64_t closest_pow2(uint64_t x) {
    if (is_low_pow2(x)) {
        return x;
    }
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
    // cout << "curr_len: " << curr_len << endl;
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
        i++;
    }
    return lst;
}

vector<complex<double>> mono_chunk_to_complex(vector<double> chunk) {
    vector<complex<double>> lst;
    uint64_t curr_len = chunk.size();
    uint64_t closest_pow = closest_pow2(curr_len);
    int i=0;
    double half_pow2_16 = pow(2.0, 16) / 2;
    for (; i<curr_len; ++i) {
        lst.push_back(complex<double>(chunk[i], 0));
    }

    while (i < closest_pow) {
        lst.push_back(complex<double>(0.0, 0.0));
        i++;
    }
    return lst;
}

double frame_to_mono(Frame frame) {
    double total = 0.0;
    for (double val : frame.channels) {
        total += val;
    }
    return total / static_cast<double>(frame.num_channels);
}

vector<double> chunk_to_mono(vector<Frame> chunk) {
    vector<double> mono_vals(chunk.size());
    for (int i=0; i<chunk.size(); ++i) {
        mono_vals[i] = frame_to_mono(chunk[i]);
    }
    return mono_vals;
}

vector<vector<double>> chunk_to_samples(vector<Frame> chunk) {
    int num_channels = chunk[0].num_channels;
    int num_samples = chunk.size();
    //Create a vector of vector<double> of length num_channels.  One vector<double> for each channel
    vector<vector<double>> samples_list(num_channels);
    //Initialize each element of samples_list to be a vector<double> of length num_samples
    for (int i=0; i<num_channels; ++i) {
        samples_list[i] = vector<double>(num_samples);
    }
    //Assign the actual values
    for (int i=0; i<chunk.size(); ++i) {
        for (int c=0; c<num_channels; ++c) {
            samples_list[c][i] = chunk[i].channels[c];
        }
    }
    return samples_list;
}


vector<double> create_bins(vector<double> vals, uint64_t num_bins) {
    uint64_t len = vals.size() / 2;
    uint64_t vals_per_bin = len / num_bins;
    vector<double> binned_vals;
    for (int i=1; i<len; i+=vals_per_bin) {
    // for (int i=0; i<len; i+=vals_per_bin) {
        double sum = 0.0;
        for (int j=i; j<i+vals_per_bin; ++j) {
            sum += vals[j];
        }
        binned_vals.push_back(calculate_db_from_power(sum));
        // binned_vals.push_back(calculate_db_from_power(sum / vals_per_bin));
    }
    cout << "Returning: " << binned_vals.size() << " binned vals";
    return binned_vals;
}

uint64_t calculate_fft_index(double freq, double fft_size, double sample_rate) {
    return freq * fft_size / sample_rate;
}


//Calculates the boundary between each bar as a frequency
vector<double> make_log_freq_edges(double min_freq, double max_freq, int num_bars) {
    vector<double> edges(num_bars + 1);

    double min_log = log(min_freq);
    double max_log = log(max_freq);

    for (int i = 0; i <= num_bars; ++i) {
        double t = double(i) / double(num_bars);
        edges[i] = exp(min_log + t * (max_log - min_log));
    }

    return edges;
}


int freq_to_fft_index(double freq, int fft_size, int sample_rate) {
    return int(freq * fft_size / sample_rate);
}


vector<double> create_log_bins(vector<Frame> chunk, uint64_t num_bins, double sample_rate,
                               double min_freq=60.0, uint64_t max_freq=12000.0, double norm_factor_multiplier=10.0) {

    vector<complex<double>> complex_chunk = mono_chunk_to_complex(chunk_to_mono(chunk));
    vector<complex<double>> freq_data = fft(complex_chunk);
    uint64_t fft_size = freq_data.size();
    uint64_t len = fft_size/2;
    uint64_t vals_per_bin = len / num_bins;
    vector<double> edges = make_log_freq_edges(min_freq, max_freq, num_bins);
    vector<double> binned_vals;
    for (int i=0; i<num_bins; ++i) {
        int start_idx = freq_to_fft_index(edges[i], fft_size, sample_rate);
        int end_idx = freq_to_fft_index(edges[i+1], fft_size, sample_rate);
        if (end_idx <= start_idx) {
            end_idx = start_idx + 1;
        }
        double sum = 0.0;
        for (int j=start_idx; j<end_idx; ++j) {
            sum += calculate_power(freq_data[j]);
        }
        double avg_power = sum/(end_idx-start_idx);
        avg_power = max(avg_power, 1e-12);
        double normed_avg_power = norm_factor_multiplier * avg_power / (fft_size * fft_size);
        binned_vals.push_back(calculate_db_from_power(normed_avg_power));
    }

    return binned_vals;
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

//Takes a raw chunk of frames, runs an fft on it, and then converts the fft output to power.
//returns a vector<double> which represent the power for each frequency
vector<double> fft_chunk_to_power_chunk(vector<Frame> chunk) {
    vector<complex<double>> complex_chunk = mono_chunk_to_complex(chunk_to_mono(chunk));
    vector<complex<double>> freq_data = fft(complex_chunk);
    vector<double> powers(freq_data.size());
    for (int i=0; i<freq_data.size(); ++i) {
        powers[i] = calculate_power(freq_data[i]);
    }
    return powers;
}

// vector<double> create_decibel_bins_from_frame_chunk(vector<Frame> chunk, uint64_t num_bins) {
//     vector<complex<double>> complex_chunk = mono_chunk_to_complex(chunk_to_mono(chunk));
//     vector<complex<double>> freq_data = fft(complex_chunk);
//     uint64_t len = freq_data.size() / 2; //only use first half of fft output
//     uint64_t vals_per_bin = len / num_bins;
//
//     for (int i=1; i<len; i+=vals_per_bin) {
//         double sum = 0.0;
//         for (int j=i; j<i+vals_per_bin; ++j) {
//
//         }
//
//     }
//
//
// }


// vector<double> fft_chunk_to_decibels_chunk(vector<Frame> chunk) {
//     vector<complex<double>> complex_chunk = mono_chunk_to_complex(chunk_to_mono(chunk));
//     vector<complex<double>> freq_data = fft(complex_chunk);
//     vector<double> decibels(freq_data.size());
//     for (int i=0; i<freq_data.size(); ++i) {
//         decibels[i] = calculate_decibels(freq_data[i]);
//     }
//     return decibels;
// }

// vector<double> fft_chunk_to_decibel_bins(vector<Frame> chunk) {
//
// }
//
vector<double> fft_chunk_to_binned_power(vector<Frame> chunk, uint64_t num_bins=num_bars) {
    // cout << "size of chunk (beginning): " << chunk.size();
    vector<double> powers = fft_chunk_to_power_chunk(chunk);
    // cout << "size of powers chunk (end): " << powers.size() << endl;
    // cout << "audio_stream->chunk_size: " << audio_stream->chunk_size << endl;
    return create_bins(powers, num_bins);
}


vector<double> fft_chunk_to_binned_decibels(vector<Frame> chunk, uint64_t num_bins=num_bars) {
    // cout << "size of chunk (beginning): " << chunk.size();
    vector<double> powers = fft_chunk_to_power_chunk(chunk);
    // cout << "size of powers chunk (end): " << powers.size() << endl;
    // cout << "audio_stream->chunk_size: " << audio_stream->chunk_size << endl;
    return create_bins(powers, num_bins);
}

double max_chunk_power(vector<complex<double>> chunk) {
    double max_val = 0.0;
    for (complex<double> c : chunk) {
        double power = calculate_decibels(c);
        if (power > max_val) {
            max_val = power;
        }
    }
    return max_val;
}



Size get_screen_size() {
    int w, h;
    SDL_GetRenderOutputSize(renderer, &w, &h);
    return Size{w, h};
}

bool put_audiostream_data(vector<Frame> chunk) {
    float* buff = chunk_to_float32_buff(chunk);
    bool res = SDL_PutAudioStreamData(sdl_audio_stream, buff, audio_stream->frames_per_chunk * sizeof(float) * audio_stream->num_channels);
    delete[] buff;
    return res;
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_Init(SDL_INIT_AUDIO);
    // fs::path imgpath = "/Users/sjber/Coding/C++/SDL_Projects/PPM_Viewer/img2.ppm";

    ParsedArgs args(argc, argv);
    auto short_flag_names = args.short_flag_names;
    auto long_flag_names = args.long_flag_names;
    auto short_flags = args.short_flags;
    auto long_flags = args.long_flags;
    fs::path cwd = fs::current_path();
    fs::path argpath = args.plain_args.back();


    if (contains<string>(short_flag_names, "c")) {
        frames_per_chunk = stoull(short_flags["c"]);
    }
    else if (contains<string>(long_flag_names, "frames-per-chunk")) {
        frames_per_chunk = stoi(long_flags["frames-per-chunk"]);
    }
    else if (contains<string>(long_flag_names, "fpc")) {
        frames_per_chunk = stoi(long_flags["fpc"]);
    }

    if (contains<string>(short_flag_names, "b")) {
        num_bars = stoull(short_flags["b"]);
    }
    else if (contains<string>(long_flag_names, "num-bars")) {
        num_bars = stoi(long_flags["bars"]);
    }

    if (contains<string>(short_flag_names, "g")) {
        string gs = short_flags["g"];
        if (gs == "vertical" || gs.starts_with('v')) {
            gradient_style = "vertical";
        }
        else if (gs == "horizontal" || gs.starts_with("hor")) {
            gradient_style = "horizontal";
        }
    }

    if (contains<string>(short_flag_names, "minfreq")) {
        freq_min = stoull(short_flags["minfreq"]);
    }
    if (contains<string>(short_flag_names, "maxfreq")) {
        freq_max = stoull(short_flags["maxfreq"]);
    }

    float lerp_t = 0.4;

    if (contains<string>(short_flag_names, "l")) {
        lerp_t = stof(short_flags["l"]);
    }




    // int32_t frames_per_chunk = 2048;
    // if (argc > 1) {
    //
    // }
    

    audio_stream = new AudioStream(cwd/argpath, frames_per_chunk);
    sample_rate = audio_stream->sample_rate;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (dev == 0) {
        cout << SDL_GetError() << endl;
        return SDL_APP_FAILURE;

    }


    SDL_AudioSpec dst_spec;
    // audio_stream->next_frame();
    SDL_GetAudioDeviceFormat(dev, &dst_spec, NULL);
    SDL_AudioFormat audio_format = SDL_AUDIO_F32;
    int num_channels = audio_stream->num_channels;
    int sample_rate = audio_stream->sample_rate;
    SDL_AudioSpec audio_spec{audio_format, num_channels, sample_rate};
    sdl_audio_stream = SDL_CreateAudioStream(&audio_spec, &dst_spec);
    SDL_BindAudioStream(dev, sdl_audio_stream);
    // SDL_PutAudioStreamData(sdl_audio_stream,




    int width = 800;
    int height = 600;
    Size screen_size{width, height};
    ScreenInfo screen_info{screen_size, 4.0};
    vector<Frame> chunk = audio_stream->read_next_chunk();
    // vector<double> chunk_power = fft_chunk_to_power_chunk(chunk);
    // vector<double> bins = fft_chunk_to_binned_decibels(chunk);
    vector<double> bins = create_log_bins(chunk, num_bars, sample_rate, freq_min, freq_max);
    // vector<double> bins = fft_chunk_to_binned_power(chunk, num_bars);
    Range minmax = vecminmax(bins);
    double min_val = minmax.first;
    double max_val = minmax.second;
    if (max_val > global_max) {
        global_max = max_val;
    }
    else {
        global_max *= 0.99f;
    }

    if (min_val > global_min) {
        global_min = min_val;
    }
    else {
        global_min *= 0.99f;
    }
    // for (int i=0; i<20; ++i) {
    //     chunk = audio_stream->read_next_chunk();
    //     bins = fft_chunk_to_binned_power(chunk);
    //     max_val = vecmax(bins);
    //     if (max_val > global_max) {
    //         global_max = max_val;
    //     }
    //     else {
    //         global_max *= 0.99f;
    //     }
    // }
    vector<complex<double>> vals = channel_to_complex(chunk, 0);
    cout << "len(vals): " << vals.size() << endl;
    vector<complex<double>> freq_data = fft(vals);
    double max_power = max_chunk_power(freq_data);
    cout << "max_val: " << max_val << endl;
    cout << "max_power: " << max_power << endl;
    std::vector<Bar> bars;
    std::vector<Color> colors = {Color{255, 0, 0, 255}, Color{0, 255, 0, 255}, Color{0, 0, 255, 255}};
    vector<double> normed_bins = convert_vec_to_range(bins, Range{global_min, global_max}, Range{0.0, 1.0});
    // for (int i=0; i<100; i++) {//vals.size(); i++) {
    for (int i=0; i<bins.size(); ++i) {
        // float h = i / 20.0;//static_cast<float>(vals.size());
        // float h = 17 * freq_data[i].real() / max_val;
        // float power = calculate_power(freq_data[i]) / max_power;
        float decibels = calculate_decibels(freq_data[i]);
        float power = bins[i] / global_max;
        // float normed_height = 

        cout << "freq_data[i]: " << freq_data[i] << endl;
        cout << "power: " << power << endl;
        cout << "db: " << decibels << endl;
        Color c = colors[i%3];
        bars.push_back(Bar{float(normed_bins[i]), c});
    }
    bd = gradient_style == "horizontal" ? BarsDisplay(screen_info, bars) : BarsDisplay(screen_info, bars, GradientInfo{GREEN, RED, static_cast<uint64_t>(screen_info.screen_size.y)});
    bd.lerp_t = lerp_t;
    bd.update_heights(normed_bins);
    
    

    cout << "actual number of bins: " << bins.size() << endl;
    cout << "dst channels: " << dst_spec.channels << endl;
    cout << "dst sample rate: " << dst_spec.freq << endl;
    cout << "dst format: " << dst_spec.format << endl;

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
            cout << "bytes_per_frame: " << audio_stream->bytes_per_frame;
            SDL_UnbindAudioStream(sdl_audio_stream);
            cout << "sample_rate: " << sample_rate << endl;
            return SDL_APP_SUCCESS;
        }
        else if (event->key.key == 'd') {
            vector<Frame> chunk = audio_stream->read_next_chunk();
            vector<double> bins = fft_chunk_to_binned_power(chunk);
            double max_val = vecmax(bins);
            if (max_val > global_max) {
                global_max = max_val;
            }
            else {
                global_max *= 0.99f;
            }
            bd.update_heights(divide_vector(bins, max_val));

        }
        else if (event->key.key == ' ') {
            playing = !playing;
        }
        else if (event->key.scancode == SDL_SCANCODE_LEFT) {
            //User hit left arrow key, rewind stream 5 seconds
            audio_stream->rewind_seconds(2.5);
            SDL_ClearAudioStream(sdl_audio_stream);

        }
        else if (event->key.scancode == SDL_SCANCODE_RIGHT) {
            //User hit left arrow key, rewind stream 5 seconds
            audio_stream->ff_seconds(2.5);
            SDL_ClearAudioStream(sdl_audio_stream);

        }
    }
    else if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        Size new_screen_size = get_screen_size();
        bd.update_screen_size(new_screen_size);
    }
    else if (event->type == SDL_EVENT_QUIT) {
        SDL_UnbindAudioStream(sdl_audio_stream);
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

    int w = actual_screen_width  != 0 ? actual_screen_width  : 800;
    int h = actual_screen_height != 0 ? actual_screen_height : 600;
    // SDL_FRect rect{250, 325, 100, 50};
    // SDL_Rect

    float decay_factor = 0.99f;



    if (playing) {
        if (SDL_AudioStreamDevicePaused(sdl_audio_stream)) {
            SDL_ResumeAudioStreamDevice(sdl_audio_stream);
        }
        vector<Frame> chunk = audio_stream->read_next_chunk();
        put_audiostream_data(chunk);
        // float* buff = chunk_to_float32_buff(chunk);
        // SDL_PutAudioStreamData(sdl_audio_stream, buff, audio_stream->frames_per_chunk * sizeof(float) * audio_stream->num_channels);
        total_frames_sent += chunk.size();
        uint64_t queued_bytes = static_cast<uint64_t>(SDL_GetAudioStreamQueued(sdl_audio_stream));
        uint64_t queued_frames = queued_bytes / (sizeof(float) * audio_stream->num_channels);
        current_playhead = total_frames_sent - queued_frames;
        if ((current_playhead - last_update_pos) >= audio_stream->frames_per_chunk) {
            vector<Frame> curr_chunk = audio_stream->get_chunk_centered_at(current_playhead);
            vector<double> bins = create_log_bins(curr_chunk, num_bars, sample_rate, freq_min, freq_max);
            Pair<double> minmax = vecminmax(bins);
            double min_val = minmax.first;
            double max_val = minmax.second;
            // cout << "minmax min: " << min_val << " minmax max: " << max_val << endl;
            if (max_val > global_max) {
                global_max = max_val;
            }
            else {
                global_max *= decay_factor;
            }
            if (min_val < global_min) {
                global_min = min_val;
            }
            else {
                global_min *= 0.99f;
            }
            vector<double> normed_bins = convert_vec_to_range(bins, Range{global_min, global_max}, Range{0.0, 1.0});
            bd.update_heights(normed_bins);
            last_update_pos = current_playhead;
        }
        // else {
        //     bd.decay_heights(0.99f);
        // }
        // delete[] buff;
    }
    else {
        SDL_PauseAudioStreamDevice(sdl_audio_stream);
    }

    //dev essentially represents the actual sound card device
    // SDL_CreateAudioStream(&audio_spec,
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

    bd.render(renderer, gradient_style);

    SDL_RenderPresent(renderer);


    /* Draw the loaded PPM image */


    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}
