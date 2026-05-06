#ifndef AUDIOSTREAM_H
#define AUDIOSTREAM_H

#include <iostream>
#include <vector>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include "frame.h"

namespace fs = std::filesystem;
using namespace std;


struct WaveHeader {
    string chunk_id; //Big
    uint64_t chunk_size; //Little
    string format; //Big
};


struct Chunk {
    string chunk_id; //Big
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


// inline float* chunk_to_float32_buff(std::vector<Frame> chunk) {
//     float* buff = new float[chunk.size()];
//     for (int i = 0; i < chunk.size(); ++i) {
//         float val = 0.0;
//         for (int c=0; c<chunk[i].num_channels; ++c) {
//             val += chunk[i].channels[c];
//         }
//         buff[i] = val / chunk[i].num_channels;
//     }
//     return buff;
// }

inline float* chunk_to_float32_buff(std::vector<Frame> chunk) {
    float* buff = new float[chunk.size() * chunk[0].num_channels];
    int count = 0;
    for (int i = 0; i < chunk.size(); ++i) {
        float val = 0.0;
        for (int c=0; c<chunk[i].num_channels; ++c) {
            buff[count] = chunk[i].channels[c];
            count++;
        }
    }
    return buff;
}

class AudioStream {
    public:
        uint64_t data_size;
        uint64_t num_channels, sample_rate, byte_rate, block_align, bits_per_sample;
        uint64_t bytes_per_sample, bits_per_frame, bytes_per_frame, frames_per_chunk;
        uint64_t chunk_size; //This is the size of a streaming chunk.  NOT an actual wav chunk, like header, fmt, list, data.
                             //It represents the size in bytes to be read for each "frame"/update of the visualizer display.
                             //It is these chunks that will get fed into the fft
        AudioStream(fs::path path, uint64_t frames_per_chunk) : frames_per_chunk(frames_per_chunk) {
            file = ifstream(path, ios_base::binary);
            WaveHeader header = read_header();
            Chunk fmt = read_fmt_chunk();
            num_channels = fmt.num_channels;
            sample_rate = fmt.sample_rate;
            byte_rate = fmt.byte_rate;
            block_align = fmt.block_align;
            bits_per_sample = fmt.bits_per_sample;
            bytes_per_sample = bits_per_sample / 8;
            bits_per_frame = bits_per_sample * num_channels;
            bytes_per_frame = bits_per_frame / 8;
            chunk_size = frames_per_chunk * bytes_per_frame;
            data_size = ff_to_data();
            pos = file.tellg();

            cout << "data_size: " << data_size << endl;
            cout << "bytes_per_sample: " << bytes_per_sample << endl;
            cout << "block_align: " << block_align << endl;
            cout << "frames_per_chunk: " << frames_per_chunk << endl;
            cout << "chunk_size: " << chunk_size << endl;
        }

        AudioStream() {}


        ~AudioStream() {
            if (file.is_open()) {
                file.close();
            }
        }


        char* next_n_bytes(uint64_t n) {
            //we never read more than 16 bytes
            char *buff = new char[n+1];
            file.read(buff, n);
            buff[n] = '\0';
            return buff;
        }

        // uint64_t next_n_bytes_sizet(uint64_t n, bool little_endian=true) {
        //     // cout << "converting " << n << " bytes to uint64_t" << endl;
        //     char* s = next_n_bytes(n);
        //     return n_bytes_to_int(s, n, little_endian);
        // }

        template<typename numT>
        numT next_n_bytes_sizet(uint64_t n, bool little_endian=true) {
            char* s = next_n_bytes(n);
            return n_bytes_to_int<numT>(s, n);
        }

        //reads the next sample in the stream, automatically handling different int widths, and returns as a normalized double
        double read_as_normalized_double() {
            if (bits_per_sample == 16) {
                int16_t val = next_n_bytes_sizet<int16_t>(2);
                return static_cast<double>(val) / 32786.0; //32786 is 2^16 / 2
            }
            else if (bits_per_sample == 24) {
                int32_t val = next_n_bytes_sizet<int32_t>(3);
                return static_cast<double>(val) / 8388608.0; //8388608 is 2^24 / 2
            }
            return 0.0;
        }



        WaveHeader read_header() {
            //Read the initial "RIFF" bytes
            string chunk_id = next_n_bytes(4);
            cout << "chunk_id: " << chunk_id << endl;
            uint64_t header_chunk_size = next_n_bytes_sizet<uint64_t>(4);
            cout << "header_chunk_size: " << header_chunk_size << endl;
            string format = next_n_bytes(4);
            cout << "format: " << format << endl;
            // cout << "at end of read_header, file.tell(): " << file.tellg() << endl;
            return WaveHeader{chunk_id, header_chunk_size, format};
        }

        Chunk read_fmt_chunk() {
            string chunk_id_var = next_n_bytes(4);
            uint64_t chunk_size_var = next_n_bytes_sizet<uint64_t>(4, true);
            uint64_t format_var = next_n_bytes_sizet<uint64_t>(2);
            uint64_t num_channels_var = next_n_bytes_sizet<uint64_t>(2);
            uint64_t sample_rate_var = next_n_bytes_sizet<uint64_t>(4);
            uint64_t byte_rate_var = next_n_bytes_sizet<uint64_t>(4);
            uint64_t block_align_var = next_n_bytes_sizet<uint64_t>(2);
            uint64_t bits_per_sample_var = next_n_bytes_sizet<uint64_t>(2);
            return Chunk{chunk_id_var, chunk_size_var, format_var, num_channels_var, sample_rate_var, byte_rate_var, block_align_var, bits_per_sample_var};
        }


        uint64_t ff_to_data() {
            cout << "Stream pos at beginning of ff_to_data: " << file.tellg() << endl;
            string word = "data";
            char c[2];
            c[1] = '\0';
            int16_t i = 0;
            while (c[0] != 'd') {
                // cout << i << endl;
                file.read(c, 1);
                i++;
                if (i > 1000) {
                    cout << "FAILED TO FIND d" << endl;
                    break;
                }
            }
            string s = next_n_bytes(3);
            if (s == "ata") {
                uint32_t datasize;
                file.read(reinterpret_cast<char*>(&datasize), 4);
                return datasize;
            }
            return 0;
        }


        Frame next_frame() {
            vector<double> vals(num_channels);
            for (int i=0; i<num_channels; ++i) {
                vals[i] = read_as_normalized_double();
            }
            Frame frame(num_channels, vals);
            stored_frames.push_back(frame);
            return frame;
        }



        vector<Frame> read_next_chunk() {
            // uint64_t frames_per_chunk = chunk_size / bytes_per_frame;
            vector<Frame> frames(frames_per_chunk);
            // while (i < frames_per_chunk) {
            for (int i=0; i<frames_per_chunk; i++) {
                frames[i] = next_frame();
            }
            return frames;
        }

        vector<Frame> get_chunk_centered_at(uint64_t idx) {
            int start_idx = max(int(idx - (frames_per_chunk / 2)), 0);
            int end_idx = start_idx + frames_per_chunk;
            cout << "chunk centered at: " << idx << " starts at: " << start_idx << " and ends at: " << end_idx << endl;
            vector<Frame> chunk(frames_per_chunk);
            for (int i=start_idx; i<end_idx; ++i) {
                chunk[i-start_idx] = stored_frames[i];
            }
            // for (int i=0; i<chunk_size; ++i) {
            //     int idx = i + chunk_size;
            //     chunk[i] = stored_frames[idx];
            // }
            return chunk;
        }



        
        // inline std::ostream& operator<<(std::ostream& out, Frame<numT> frame) {




    private:
        ifstream file;
        vector<Frame> stored_frames;
        uint64_t pos;


};


#endif
