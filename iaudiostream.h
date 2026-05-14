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



inline std::vector<Frame> f32_buff_to_frames(const void *input_buff, uint32_t frame_count, uint32_t num_channels) {
    const float *buff = (const float*)input_buff;
    std::vector<Frame> frames(frame_count);
    for (int i=0; i<frame_count; i++) {
        std::vector<double> channels(num_channels);
        int channel_idx = 0;
        for (int j = i * num_channels; j<(i*num_channels)+num_channels; ++j) {
            channels[channel_idx] = buff[j];
            channel_idx++;
        }
    }
    return frames;
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


inline Chunk _read_fmt_chunk(std::ifstream *f) {
    std::string chunk_id_var = _next_n_bytes(f, 4);
    uint64_t chunk_size_var = _next_n_bytes_sizet<uint64_t>(f, 4, true);
    uint64_t format_var = _next_n_bytes_sizet<uint64_t>(f, 2);
    uint64_t num_channels_var = _next_n_bytes_sizet<uint64_t>(f, 2);
    uint64_t sample_rate_var = _next_n_bytes_sizet<uint64_t>(f, 4);
    uint64_t byte_rate_var = _next_n_bytes_sizet<uint64_t>(f, 4);
    uint64_t block_align_var = _next_n_bytes_sizet<uint64_t>(f, 2);
    uint64_t bits_per_sample_var = _next_n_bytes_sizet<uint64_t>(f, 2);
    return Chunk{chunk_id_var, chunk_size_var, format_var, num_channels_var, sample_rate_var, byte_rate_var, block_align_var, bits_per_sample_var};
}



inline WaveHeader read_header(std::ifstream *f) {
        //Read the initial "RIFF" bytes
        std::string chunk_id = _next_n_bytes(f, 4);
        // cout << "chunk_id: " << chunk_id << endl;
        uint64_t header_chunk_size = _next_n_bytes_sizet<uint64_t>(f, 4);
        // cout << "header_chunk_size: " << header_chunk_size << endl;
        std::string format = _next_n_bytes(f, 4);
        // cout << "format: " << format << endl;
        // cout << "at end of read_header, file->tell(): " << file->tellg() << endl;
        return WaveHeader{chunk_id, header_chunk_size, format};
}



inline uint64_t ff_to_data(std::ifstream *file) {
    // cout << "Stream pos at beginning of ff_to_data: " << file->tellg() << endl;
    std::string word = "data";
    char c[2];
    c[1] = '\0';
    int16_t i = 0;
    while (c[0] != 'd') {
        // cout << i << endl;
        file->read(c, 1);
        i++;
        if (i > 1000) {
            break;
        }
    }
    std::string s = _next_n_bytes(file, 3);
    if (s == "ata") {
        uint32_t datasize;
        file->read(reinterpret_cast<char*>(&datasize), 4);
        return datasize;
    }
    return 0;
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
    virtual std::vector<Frame> next_n_frames(uint64_t n) = 0;
    virtual std::vector<Frame> read_next_chunk() = 0;
    virtual std::span<Frame> get_chunk_centered_at(uint64_t idx) = 0;
    virtual uint64_t total_frames_available() = 0;
    virtual void close() = 0;


    void filestream_setup(std::filesystem::path path, uint64_t frames_per_chunk) {

            this->frames_per_chunk = frames_per_chunk;
            stream_type = file_stream;
            file = new std::ifstream(path, std::ios_base::binary);
            WaveHeader header = read_header(file);
            Chunk fmt = _read_fmt_chunk(file);
            num_channels = fmt.num_channels;
            sample_rate = fmt.sample_rate;
            byte_rate = fmt.byte_rate;
            block_align = fmt.block_align;
            bits_per_sample = fmt.bits_per_sample;
            bytes_per_sample = bits_per_sample / 8;
            bits_per_frame = bits_per_sample * num_channels;
            bytes_per_frame = bits_per_frame / 8;
            chunk_size = frames_per_chunk * bytes_per_frame;
            data_size = ff_to_data(file);
            pos = static_cast<uint64_t>(file->tellg());
            uint64_t normalization_divisor = 2 << (bits_per_sample - 2);
            normalization_multiplier = 1 / static_cast<double>(normalization_divisor);
            uint64_t total_frames = data_size / bytes_per_frame;
            stored_frames.reserve(total_frames);
        

    }
    // virtual std::std::vector<Frame> get_chunk_centered_at(uint64_t idx);

};


#endif
