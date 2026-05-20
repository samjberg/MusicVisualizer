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
#include <complex>
#include <filesystem>
#include <memory>
#include <span>
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include "barsdisplay.h"
#include "fft.h"
#include "parseargs.h"
#include "miniaudio/miniaudio.h"
#include "iaudiostream.h"
#include "audiostream.h"
#include "audioloopbackstream.h"


using namespace std;
namespace fs = std::filesystem;

struct AppState {
    SDL_Window *window;
    SDL_Renderer* renderer;
    int actual_screen_width;
    int actual_screen_height;
    unique_ptr<IAudioStream> audio_stream;
    SDL_AudioStream* sdl_audio_stream;
    BarsDisplay* bd;
    uint64_t sample_rate;
    double freq_min;
    double freq_max;
    double global_max;
    double global_min;
    double max_queued_bytes;
    uint64_t total_frames_sent;
    uint64_t current_playhead;
    uint64_t last_update_pos;
    uint64_t prev_ticks;
    bool playing;
};


fs::path fpath = "/Users/sjber/Coding/C++/SDL_Projects/MusicVisualizer/cliffsofdover.wav";
// fs::path fpath = "/Users/sjber/Coding/C++/SDL_Projects/MusicVisualizer/footstepswav.wav";

//Global variables for keeping track of the current playhead
uint64_t total_frames_sent = 0; //total number of frames SENT to the audio device (not necessarily played yet)
uint64_t last_update_pos = 0;
uint64_t current_playhead = 0;
uint64_t num_bars = 32;
double max_queued_chunks = 4.0;
double max_queued_frames = 0.0;
double max_queued_bytes = 0.0;
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



vector<double> chunk_to_mono(span<Frame> chunk) {
    vector<double> mono_vals(chunk.size());
    for (int i=0; i<chunk.size(); ++i) {
        mono_vals[i] = frame_to_mono(chunk[i]);
    }
    return mono_vals;
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


vector<double> create_log_bins(span<Frame> chunk, uint64_t num_bins, double sample_rate,
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
        double normed_avg_power = norm_factor_multiplier * avg_power / (fft_size);
        binned_vals.push_back(calculate_db_from_power(normed_avg_power));
    }
    return binned_vals;
}

vector<double> create_log_bins(vector<double> samples, uint64_t num_bins, double sample_rate,
                               double min_freq=60.0, uint64_t max_freq=12000.0, double norm_factor_multiplier=10.0) {

    vector<complex<double>> complex_chunk = mono_chunk_to_complex(samples);
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
        double normed_avg_power = norm_factor_multiplier * avg_power / (fft_size);
        binned_vals.push_back(calculate_db_from_power(normed_avg_power));
    }
    return binned_vals;
}


vector<double> create_log_bins_new(span<Frame> chunk, uint64_t num_bins, double sample_rate,
                               double min_freq=60.0, uint64_t max_freq=12000.0, double norm_factor_multiplier=10.0) {

    uint64_t num_channels = chunk[0].num_channels;
    if (num_channels == 1) {
        return create_log_bins(chunk, num_bins, sample_rate, min_freq, max_freq, norm_factor_multiplier);
    }
    vector<double> binned_vals(num_bins);
    for (int c=0; c<num_channels; ++c) {
        vector<double> samples(chunk.size());
        for (int i=0; i<chunk.size(); ++i) {
            samples[i] = chunk[i].channels[c];
        }
        uint64_t half_bins_size = num_bins / 2;
        vector<double> channel_bins = create_log_bins(samples, half_bins_size, sample_rate, min_freq, max_freq, norm_factor_multiplier);
        if (c%2 == 0) {
            //Even channel index, push values onto binned_vals in reverse order
            for (int i=channel_bins.size()-1; i>=0; --i) {
                //i is in reverse order, so we first have to reverse it to get a number from 0 to half_bins_size
                //Then we add the offset to take into account that we are doing each channel separately starting at a new 0-size range
                int j = (half_bins_size - i - 1) + (c * half_bins_size);
                binned_vals[j] = channel_bins[i];
            }

        }
        else {
            //Odd channel index, push values onto binned_vals in forward order
            for (int i=0; i<half_bins_size; ++i) {
                int j = i + (c * half_bins_size);
                binned_vals[j] = channel_bins[i];
            }

        }


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


Size get_screen_size(SDL_Renderer *renderer) {
    int w, h;
    SDL_GetRenderOutputSize(renderer, &w, &h);
    return Size{w, h};
}

bool put_audiostream_data(vector<Frame>& chunk, SDL_AudioStream *sdl_audio_stream, int num_channels) {
    float* buff = chunk_to_float32_buff(chunk);
    bool res = SDL_PutAudioStreamData(sdl_audio_stream, buff, chunk.size() * sizeof(float) * num_channels);
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
    fs::path argpath;
    if (args.plain_args.size() > 0) {
        argpath = args.plain_args.back();
    }
    else {
        argpath = "loopback";
    }
    //Reassign argpath if it has a value because of my bad command line argument handling.  Right now ParsedArgs will always
    //read the last flag value as both that flag's value, but also as a plain_arg.  So this is just dealing with both that
    //scenario, as well as the scenario where there are no plain args.
    if (argpath.string().size() == 0) {
        cout << "argpath empty, setting it to \"loopback\"" << endl;
        argpath = "loopback";
    }
    else {
        for (auto pair : short_flags) {
            if (argpath == pair.second) {
                argpath = "loopback";
                break;
            }
        }
        for (auto pair : long_flags) {
            if (argpath == pair.second) {
                argpath = "loopback";
                break;
            }
        }
    }



    static SDL_Window *window = NULL;
    static SDL_Renderer *renderer = NULL;
    static double global_max = -1000.0;         //min sample value found.  This default gets overridden at initialization, it for safety
    static double global_min = 1000.0;          //max sample value found.  This default gets overridden at initialization, it for safety
    static double freq_min = 40.0;              //minimum frequency displayed in the bar visualization
    static double freq_max = 8000.0;            //maximum frequency displayed in the bar visualization
    static double skip_duration = 2.5;          //Number of seconds per fastforward/rewind
    static bool playing = true;                 //Whether or not the audio (and accompanying visuals) is currently playing
    static uint64_t sample_rate = 48000;
    static uint64_t frames_per_chunk = 2048;    //Number of audio frames read and processed per visual frame (chunk)
    static string gradient_style = "vertical";  //Orientation of the color gradient on the bars
    int actual_screen_width = 800;
    int actual_screen_height = 600;
    float* paused_data_buffer;
    int paused_buffer_size = 0;
    uint64_t prev_ticks = 0;
    float rising_speed = 12.0;
    float falling_speed = 8.0;


    BarsDisplay *bd;
    // AudioStream audio_stream(fpath, 2048);
    SDL_AudioStream *sdl_audio_stream = nullptr;


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

    if (contains<string>(short_flag_names, "mqc")) {
        max_queued_chunks = stod(short_flags["mqc"]);
    }

    float lerp_t = 0.4;

    if (contains<string>(short_flag_names, "l")) {
        lerp_t = stof(short_flags["l"]);
    }

    if (contains<string>(short_flag_names, "rs")) {
        rising_speed = stof(short_flags["rs"]);
    }
    if (contains<string>(short_flag_names, "fs")) {
        falling_speed = stof(short_flags["fs"]);
    }

    if (contains<string>(short_flag_names, "s")) {
        rising_speed = stof(short_flags["s"]);
        falling_speed = rising_speed / 2.0f;
    }


    // unique_ptr<IAudioStream> audio_stream;

    // int32_t frames_per_chunk = 2048;
    // if (argc > 1) {
    //
    // }
    

    // if (argpath == "loopback") {
    //     au
    // }
    // audio_stream = make_unique<AudioStream>(cwd/argpath, frames_per_chunk);
    // AudioStream *audio_stream = new AudioStream(cwd/argpath, frames_per_chunk);

    cout << "Frames per chunk: " << frames_per_chunk << endl;
    unique_ptr<IAudioStream> audio_stream;
    if ((argpath == ".") || (argpath == "loopback") || (argpath == "live")) {
        audio_stream = make_unique<AudioLoopbackStream>(frames_per_chunk);
    }
    else {
        try {
            audio_stream = make_unique<AudioStream>(cwd/argpath, frames_per_chunk);
        }
        catch (std::exception e) {
            cout << e.what() << endl;


        }


        // audio_stream = make_unique<AudioStream>(cwd/argpath, frames_per_chunk);
        // a->sdl
    }
    sample_rate = audio_stream->sample_rate;
    max_queued_bytes = max_queued_chunks * audio_stream->bytes_per_frame * audio_stream->frames_per_chunk;

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
    SDL_AudioSpec audio_spec{audio_format, num_channels, static_cast<int>(sample_rate)};
    if (argpath != "loopback") {
        sdl_audio_stream = SDL_CreateAudioStream(&audio_spec, &dst_spec);
        SDL_BindAudioStream(dev, sdl_audio_stream);
    }
    // SDL_PutAudioStreamData(sdl_audio_stream,




    int width = 800;
    int height = 600;
    float render_scale = 4.0;
    Size screen_size{width, height};
    ScreenInfo screen_info{screen_size, render_scale};
    vector<double> bins(num_bars, 0.0);
    Range minmax = vecminmax(bins);
    double min_val = -60.0;
    double max_val = 10.0;
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
    cout << "max_val: " << max_val << endl;
    std::vector<Bar> bars;
    Gradient colors = {Color{255, 0, 0, 255}, Color{0, 255, 0, 255}, Color{0, 0, 255, 255}};
    vector<double> normed_bins = convert_vec_to_range(bins, Range{global_min, global_max}, Range{0.0, 1.0});
    // for (int i=0; i<100; i++) {//vals.size(); i++) {
    for (int i=0; i<bins.size(); ++i) {
        float power = bins[i] / global_max;
        Color c = colors[i%3];
        bars.push_back(Bar{0.0, 0.0, Color{0, 255, 0, 255}});
    }
    if (gradient_style == "horizontal") {
        bd = new BarsDisplay(screen_info, bars, rising_speed, falling_speed);
    }
    else {
        bd = new BarsDisplay(screen_info, bars, GradientInfo{GREEN, RED, static_cast<uint64_t>(height), gradient_style}, rising_speed, falling_speed);
    }
    // bd = gradient_style == "horizontal" ? BarsDisplay(screen_info, bars) : BarsDisplay(screen_info, bars, GradientInfo{GREEN, RED, static_cast<uint64_t>(screen_info.screen_size.y)});
    bd->lerp_t = lerp_t;
    bd->update_heights(normed_bins);
    

    cout << "actual number of bins: " << bins.size() << endl;
    cout << "dst channels: " << dst_spec.channels << endl;
    cout << "dst sample rate: " << dst_spec.freq << endl;
    cout << "dst format: " << dst_spec.format << endl;


    if (audio_stream->stream_type == file_stream) {
        AudioStream *audio_stream_specific = dynamic_cast<AudioStream*>(audio_stream.get());
        audio_stream_specific->set_sdl_audio_stream(sdl_audio_stream);
    }

    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("Hello World", width, height, SDL_WINDOW_FULLSCREEN, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;

    }

    *appstate = new AppState({window, renderer, actual_screen_width, actual_screen_height, std::move(audio_stream), sdl_audio_stream, bd, 
                            sample_rate, freq_min, freq_max, global_max, global_min, max_queued_bytes, total_frames_sent, current_playhead,
                            last_update_pos, prev_ticks,playing});

    // if ((argpath == ".") || (argpath == "loopback") || (argpath == "live")) {
    // }
    // else {
    //     *appstate = new AppState({window, renderer, actual_screen_width, actual_screen_height, 
    //                             make_unique<AudioStream>(cwd/argpath, frames_per_chunk), sdl_audio_stream, bd, sample_rate, freq_min,
    //                             freq_max, global_max, global_min, max_queued_bytes, total_frames_sent, current_playhead, last_update_pos,
    //                             prev_ticks, playing});
    //     // audio_stream = make_unique<AudioStream>(cwd/argpath, frames_per_chunk);
    // }

    // *appstate = new AppState({window, renderer, actual_screen_width, actual_screen_height, sdl_audio_stream, bd,
    //                           sample_rate, freq_min, freq_max, global_max, global_min, max_queued_bytes, total_frames_sent,
    //                           current_playhead, last_update_pos, prev_ticks, playing});
    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN) {
        AppState *as = (AppState*)appstate;
        // as->audio_stream->next_n_frames(5);
        SDL_AudioStream *sdl_audio_stream = as->sdl_audio_stream;
        cout << "sample_rate: " << as->sample_rate << endl;
        if (event->key.key == 'q') {
            cout << "bytes_per_frame: " << as->audio_stream->bytes_per_frame;
            if (as->audio_stream->stream_type == file_stream) {
                SDL_UnbindAudioStream(sdl_audio_stream);
            }
            cout << "sample_rate: " << as->sample_rate << endl;
            return SDL_APP_SUCCESS;
        }
        else if (event->key.key == ' ') {
            as->playing = !as->playing;
        }
        else if (event->key.scancode == SDL_SCANCODE_LEFT) {
            //User hit left arrow key, rewind stream 5 seconds
            if (as->audio_stream->stream_type == file_stream) {
                SDL_ClearAudioStream(sdl_audio_stream);
                AudioStream *audio_stream = dynamic_cast<AudioStream*>(as->audio_stream.get());
                audio_stream->rewind_seconds(2.5);
            }


        }
        else if (event->key.scancode == SDL_SCANCODE_RIGHT) {
            //User hit left arrow key, rewind stream 5 seconds
            if (as->audio_stream->stream_type == file_stream) {
                SDL_ClearAudioStream(sdl_audio_stream);
                AudioStream *audio_stream = dynamic_cast<AudioStream*>(as->audio_stream.get());
                audio_stream->ff_seconds(2.5);
            }

        }
    }
    else if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        AppState *as = (AppState*)appstate;
        Size new_screen_size = get_screen_size(as->renderer);
        as->bd->update_screen_size(new_screen_size);
    }
    else if (event->type == SDL_EVENT_QUIT) {
        AppState *as = (AppState*)appstate;
        if (as->audio_stream->stream_type == file_stream) {
            SDL_UnbindAudioStream(as->sdl_audio_stream);
        }
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{

    AppState* as = (AppState*)appstate;
    // AudioStream *audio_stream = as->audio_stream;
    SDL_AudioStream *sdl_audio_stream = as->sdl_audio_stream;
    BarsDisplay *bd = as->bd;
    SDL_Renderer *renderer = as->renderer;
    int actual_screen_width = as->actual_screen_width;
    int actual_screen_height = as->actual_screen_height;
    double global_max = as->global_max;
    double global_min = as->global_min;
    uint64_t sample_rate = as->sample_rate;
    double freq_max = as->freq_max;
    double freq_min = as->freq_min;
    uint64_t prev_ticks = as->prev_ticks;




    const char *message = "Hello World!";
    // int w = img.get_width(), h = img.get_height();
    float x, y;
    const float scale = 4.0f;

    int w = actual_screen_width  != 0 ? actual_screen_width  : 800;
    int h = actual_screen_height != 0 ? actual_screen_height : 600;
    // SDL_FRect rect{250, 325, 100, 50};
    // SDL_Rect

    float decay_factor = 0.99f;


    uint64_t queued_bytes = 0;


    if (as->playing) {
        if (as->audio_stream->stream_type == file_stream) {
            if (SDL_AudioStreamDevicePaused(sdl_audio_stream)) {
                SDL_ResumeAudioStreamDevice(sdl_audio_stream);
            }
            // uint64_t queued_bytes = 0;
            queued_bytes = static_cast<uint64_t>(SDL_GetAudioStreamQueued(sdl_audio_stream));

            if (queued_bytes < max_queued_bytes) {
                AudioStream* audio_stream = dynamic_cast<AudioStream*>(as->audio_stream.get());
                vector<Frame> chunk = as->audio_stream->read_next_chunk();
                audio_stream->put_audiostream_data(chunk);
                // put_audiostream_data(chunk, sdl_audio_stream, as->audio_stream->num_channels);
                // as->audio_stream->total_frames_consumed += chunk.size();
            }
            // float* buff = chunk_to_float32_buff(chunk);
            // SDL_PutAudioStreamData(sdl_as->audio_stream, buff, audio_stream->frames_per_chunk * sizeof(float) * audio_stream->num_channels);
            // uint64_t queued_frames = queued_bytes / (sizeof(float) * as->audio_stream->num_channels);
            // current_playhead = total_frames_sent - queued_frames;
            // if ((current_playhead - last_update_pos) >= as->audio_stream->frames_per_chunk) {
            // if (as->audio_stream->upd
            // AudioStream *audio_stream = dynamic_cast<AudioStream*>(as->audio_stream.get());
        }
        if (as->audio_stream->update_playhead_should_play(queued_bytes)) {
            vector<Frame> curr_chunk = as->audio_stream->next_display_chunk();
            vector<double> bins = create_log_bins_new(curr_chunk, bd->count, sample_rate, freq_min, freq_max, 20.0);
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
                global_min *= decay_factor;
            }
            as->global_max = global_max;
            as->global_min = global_min;
            vector<double> normed_bins = convert_vec_to_range(bins, Range{global_min, global_max}, Range{0.0, 1.0});
            bd->update_heights(normed_bins);
            // last_update_pos = current_playhead;
        }
    }
    else if (as->audio_stream->stream_type == file_stream) {
        // else {
        SDL_PauseAudioStreamDevice(sdl_audio_stream);
        // }

    }

    uint64_t curr_ticks = SDL_GetTicks();
    uint64_t ticks_elapsed = curr_ticks - prev_ticks;
    float seconds_elapsed = static_cast<float>(ticks_elapsed) / 1000.0f;
    as->prev_ticks = curr_ticks;
    prev_ticks = curr_ticks;

    bd->process_visual_frame(seconds_elapsed);

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

    bd->render(renderer);

    SDL_RenderPresent(renderer);


    /* Draw the loaded PPM image */


    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    AppState *as = (AppState*)appstate;
    as->audio_stream->close();
    cout << "normalization_multiplier: " << as->audio_stream->normalization_multiplier << endl;
    cout << "max queued chunks: " << max_queued_chunks << endl;
}
