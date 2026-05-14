#ifndef IAUDIOSTREAM_H
#define IAUDIOSTREAM_H

#include <cstdint>
#include <vector>
#include "frame.h"
#include <string>
#include <filesystem>
#include <span>
#include <miniaudio/miniaudio.h>


// namespace fs = std::filesystem;
// using namespace std;


struct WaveHeader {
    std::string chunk_id; //Big
    uint64_t chunk_size; //Little
    std::string format; //Big
};


struct Chunk {
    std::string chunk_id; //Big
    uint64_t chunk_size; //Little
    uint64_t format; //Little
    uint64_t num_channels; //Little
    uint64_t sample_rate; //Little
    uint64_t byte_rate; //Little
    uint64_t block_align; //Little
    uint64_t bits_per_sample; //Little
    uint64_t extra_param_size;
    uint64_t extra_params; //I'm not sure if this is necessary, and also it may NEED to be removed
};


//Converts a chunk (a std::vector of Frames) to a float*, ready to be passed to SDL_PutAudioStreamData to be played as audio
inline float* chunk_to_float32_buff(std::vector<Frame>& chunk) {
    float* buff = new float[chunk.size() * chunk[0].num_channels];
    int count = 0;
    for (int i = 0; i < chunk.size(); ++i) {
        for (int c=0; c<chunk[i].num_channels; ++c) {
            buff[count] = chunk[i].channels[c];
            count++;
        }
    }
    return buff;
}



inline char* _next_n_bytes(std::ifstream *file, uint64_t n, int64_t start=-1) {
    if (start == -1) {
        //we never read more than 16 bytes
        char *buff = new char[n+1];
        file->read(buff, n);
        buff[n] = '\0';
        return buff;
    }
    //Seek to start
    file->seekg(start);
    char* buff = new char[n+1];
    file->read(buff, n);
    buff[n] = '\0';
    return buff;
}


template<typename numT>
inline numT _next_n_bytes_sizet(std::ifstream *f, uint64_t n, bool little_endian=true) {
    char* s = _next_n_bytes(f, n);
    return n_bytes_to_int<numT>(s, n);
}


enum audiostream_type {
    file_stream,
    loopback_stream
};

class IAudioStream {
protected:
    std::ifstream *file;
    std::vector<Frame> stored_frames;

public:
    audiostream_type stream_type;
    uint64_t current_playhead;
    uint64_t data_size;
    double normalization_multiplier;
    uint64_t pos; //The current stream pos, always equal to file->tellg();
    uint64_t num_channels, sample_rate, byte_rate, block_align, bits_per_sample;
    uint64_t bytes_per_sample, bits_per_frame, bytes_per_frame, frames_per_chunk;
    uint64_t chunk_size; //This is the size of a streaming chunk.  NOT an actual wav chunk, like header, fmt, list, data.
    uint64_t total_frames_read;
    uint64_t total_frames_consumed;
    IAudioStream(std::filesystem::path path, uint64_t frames_per_chunk);
    IAudioStream(int it_literally_doesnt_matter_pass_in_anything_this_is_to_deal_with_cpps_bullshit) {};
    virtual ~IAudioStream() = default;
    // virtual std::vector<Frame> next_n_frames(uint64_t n) = 0;
    virtual std::vector<Frame> read_next_chunk() = 0;
    virtual uint64_t total_frames_available() = 0;
    virtual std::vector<Frame> next_display_chunk() = 0; //Returns the next chunk to be displayed visually with a BarsDisplay
    //Performs all functions related to updating the playhead, whatever that means for the IAudioStream's stream_type, and returns
    //whether or not the stream is ready to display more visual data (has a new full chunk available)
    virtual bool update_playhead_should_play(uint64_t queued_bytes=0) = 0; 
    virtual void close() = 0;
};


#endif
