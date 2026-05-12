#ifndef AUDIOSTREAM_H
#define AUDIOSTREAM_H

#include <iostream>
#include <vector>
#include <span>
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


//Converts a chunk (a vector of Frames) to a float*, ready to be passed to SDL_PutAudioStreamData to be played as audio
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


class AudioStream {
    public:
        uint64_t data_size;
        double normalization_multiplier;
        uint64_t pos; //The current stream pos, always equal to file->tellg();
        uint64_t num_channels, sample_rate, byte_rate, block_align, bits_per_sample;
        uint64_t bytes_per_sample, bits_per_frame, bytes_per_frame, frames_per_chunk;
        uint64_t chunk_size; //This is the size of a streaming chunk.  NOT an actual wav chunk, like header, fmt, list, data.
                             //It represents the size in bytes to be read for each "frame"/update of the visualizer display.
                             //It is these chunks that will get fed into the fft
        AudioStream(fs::path path, uint64_t frames_per_chunk) : frames_per_chunk(frames_per_chunk) {
            file = new ifstream(path, ios_base::binary);
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
            pos = static_cast<uint64_t>(file->tellg());
            uint64_t normalization_divisor = 2 << (bits_per_sample - 2);
            normalization_multiplier = 1 / static_cast<double>(normalization_divisor);
            uint64_t total_frames = data_size / bytes_per_frame;
            stored_frames.reserve(total_frames);
        }

        AudioStream() {}


        ~AudioStream() {
            if (file->is_open()) {
                file->close();
            }
        }


        //Reads the next n bytes into a char* buffer from the underlying ifstream.  This function uses the ifstream's position
        char* next_n_bytes(uint64_t n, int64_t start=-1) {
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
        numT next_n_bytes_sizet(uint64_t n, bool little_endian=true) {
            char* s = next_n_bytes(n);
            return n_bytes_to_int<numT>(s, n);
        }

        template <typename numT>
        double as_normalized_double(numT sample) {
            if (bits_per_sample == 16) {
                return static_cast<double>(sample) / 32786.0;
            }
            else if (bits_per_sample == 24) {
                return static_cast<double>(sample) / 8388608.0; //8388608 is 2^24 / 2
            }
            return 0.0;
        }

        double read_as_normalized_double() {
            if (bits_per_sample == 16) {
                int16_t val = next_n_bytes_sizet<int16_t>(bytes_per_sample);
                return static_cast<double>(val) * normalization_multiplier;
            }
            else if (bits_per_sample == 24) {
                int32_t val = next_n_bytes_sizet<int32_t>(bytes_per_sample);
                return static_cast<double>(val) * normalization_multiplier;
            }
            return 0.0;
        }


        WaveHeader read_header() {
            //Read the initial "RIFF" bytes
            string chunk_id = next_n_bytes(4);
            // cout << "chunk_id: " << chunk_id << endl;
            uint64_t header_chunk_size = next_n_bytes_sizet<uint64_t>(4);
            // cout << "header_chunk_size: " << header_chunk_size << endl;
            string format = next_n_bytes(4);
            // cout << "format: " << format << endl;
            // cout << "at end of read_header, file->tell(): " << file->tellg() << endl;
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
            cout << "Stream pos at beginning of ff_to_data: " << file->tellg() << endl;
            string word = "data";
            char c[2];
            c[1] = '\0';
            int16_t i = 0;
            while (c[0] != 'd') {
                // cout << i << endl;
                file->read(c, 1);
                i++;
                if (i > 1000) {
                    cout << "FAILED TO FIND d" << endl;
                    break;
                }
            }
            string s = next_n_bytes(3);
            if (s == "ata") {
                uint32_t datasize;
                file->read(reinterpret_cast<char*>(&datasize), 4);
                return datasize;
            }
            return 0;
        }

        Frame next_frame() {
            uint64_t curr_frame_idx = pos / bytes_per_frame;
            if (curr_frame_idx >= stored_frames.size()) {
                vector<double> vals(num_channels);
                for (int i=0; i<num_channels; ++i) {
                    vals[i] = read_as_normalized_double();
                }
                Frame frame(num_channels, vals);
                stored_frames.push_back(frame);
                pos += bytes_per_frame;
                return frame;

            }
            else {
                pos += bytes_per_frame;
                return stored_frames[curr_frame_idx];
            }
        }

        vector<Frame> next_n_frames(uint64_t n) {
            uint64_t total_bytes = n * bytes_per_frame;
            uint64_t frame_idx = pos / bytes_per_frame;
            uint64_t start_frame_idx = frame_idx;
            uint64_t frames_processed = 0;
            uint64_t start_pos = pos;
            vector<Frame> next_frames(n);

            if (frame_idx < stored_frames.size()) {
                uint64_t start_idx = frame_idx;
                uint64_t stop_idx = min((frame_idx+n)/bytes_per_frame, static_cast<uint64_t>(stored_frames.size()));
                while (frame_idx < stop_idx) {
                    cout << "IN IF STATEMENT IN IF STATEMENT IN IF STATEMENT!!!!!" << endl;
                    next_frames[frame_idx-start_idx] = stored_frames[frame_idx];
                    frame_idx++;
                    pos += bytes_per_frame;
                }
                frames_processed = frame_idx - start_idx;
                if (frames_processed >= n) {
                    return next_frames;
                }
            }
            //Recalculate total_bytes in case there was a partial (but not complete) cached read
            total_bytes = (n-frames_processed) * bytes_per_frame;
            char* buff = next_n_bytes(total_bytes);
            for (int i=0; i<total_bytes; i+=bytes_per_frame) {
                // cout << "in for loop, frame_idx: " << frame_idx << endl;
                vector<double> frame_channels(num_channels);
                for (int c=0; c<num_channels; ++c) {
                    uint64_t offset = i + (c * bytes_per_sample);
                    // cout << "offset: " << offset << endl;
                    frame_channels[c] = n_bytes_to_normalized_double(buff + offset, bytes_per_sample);
                }
                Frame frame = Frame(num_channels, frame_channels);
                next_frames[(i/bytes_per_frame) + frames_processed] = frame;
                stored_frames.push_back(frame);
                // frame_idx++;
                pos += bytes_per_frame;
            }
            delete[] buff;
            return next_frames;
        }


        vector<Frame> read_next_chunk() {
            return next_n_frames(frames_per_chunk);
        }

        //Gets a vector of stored Frames from start_idx to end_idx, indices must be in terms of frames (not bytes or samples)
        //For safety, automatically reads the next chunk to further populate stored_frames if necessary to prevent out of bounds error
        vector<Frame> get_stored_frames_at(uint64_t start_idx, uint64_t end_idx) {
            vector<Frame> chunk(frames_per_chunk);
            uint64_t curr_pos = pos;
            for (uint64_t i=start_idx; i<end_idx; ++i) {
                if (i >= stored_frames.size()) {
                    read_next_chunk();
                }
                chunk[i - start_idx] = stored_frames[i];
            }
            return chunk;
        }

        //idx is a frame index.  Passing in idx=N means getting the chunk BEGINNING at the Nth frame;
        span<Frame> get_chunk_at(uint64_t idx) {
            uint64_t end_idx = min(idx + chunk_size, data_size/bytes_per_frame);
            return span<Frame>(&stored_frames[idx], end_idx-idx);
            // return get_stored_frames_at(idx, end_idx);
        }


        //idx is a frame index.  Passing in idx=N means get the chunk CENTERED at the Nth frame.
        span<Frame> get_chunk_centered_at(uint64_t idx) {
            // uint64_t start_idx = max(idx - (frames_per_chunk / 2), static_cast<uint64_t>(0));
            uint64_t start_idx = (idx > (frames_per_chunk / 2)) ? (idx - (frames_per_chunk / 2)) : 0;
            uint64_t end_idx = min(start_idx + frames_per_chunk, static_cast<uint64_t>(stored_frames.size()));
            return span<Frame>(&stored_frames[start_idx], end_idx - start_idx);
            // // cout << "chunk centered at: " << idx << " starts at: " << start_idx << " and ends at: " << end_idx << endl;
            // vector<Frame> chunk(frames_per_chunk);
            // for (int i=start_idx; i<end_idx; ++i) {
            //     chunk[i-start_idx] = stored_frames[i];
            // }
            // return chunk;
        }

        uint64_t curr_pos() {
            return static_cast<uint64_t>(file->tellg());

        }

        void seek(int64_t n) {

            cout << "[seek] enter n=" << n
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << endl;
            auto before = file->tellg();
            cout << "[seek] tellg before seek=" << before
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << endl;
            file->seekg(n, ios_base::cur);
            auto after = file->tellg();
            cout << "[seek] tellg after seek=" << after
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << endl;
        }

        //Seek forward n bytes from current stream pos in file
        void seek_forward(int64_t n) {
            file->seekg(n, ios_base::cur);
        }

        //Seek backwards n bytes from current stream pos in file
        void seek_backwards(int64_t n) {
            streamoff offset = static_cast<streamoff>(n);
            file->seekg(-offset, ios_base::cur);
        }

        void seek_to_pos(int64_t n) {
            file->seekg(n, ios_base::beg);
        }

        //Fast forward the stream by `seconds` seconds
        uint64_t ff_seconds(double seconds) {

            cout << "[ff_seconds] enter seconds=" << seconds
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << endl;
            cout << "[ff_seconds] tellg before seek=" << pos
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << endl;
            int64_t num_bytes = seconds * sample_rate * bytes_per_frame;
            cout << "[ff_seconds] byte offset=" << num_bytes << endl;
            seek_forward(num_bytes);
            auto after = file->tellg();
            cout << "[ff_seconds] tellg after seek=" << after
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << endl;

            pos = min(pos + static_cast<uint64_t>(num_bytes), static_cast<uint64_t>(data_size));
            return pos;
        }

        //Rewind the stream by `seconds` seconds
        uint64_t rewind_seconds(double seconds) {
            cout << "stored_frames.size(): " << stored_frames.size();

            // auto pos = file->tellg();
            cout << "[rewind_seconds] tellg before seek=" << pos
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << endl;
            int64_t num_bytes = static_cast<int64_t>(seconds * byte_rate);
            cout << "[rewind_seconds] byte offset=" << -num_bytes << endl;
            seek(-num_bytes);
            auto after = file->tellg();
            cout << "[rewind_seconds] tellg after seek=" << after
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << endl;
            pos = max(pos - static_cast<uint64_t>(num_bytes), static_cast<uint64_t>(0));
            for (int i=0; i<20; ++i) {
                cout << "pos: " << pos << endl;
                cout << "audio_stream->pos: " << this->pos << endl;
                cout << "pos frame (pos/bytes_per_frame): " << pos / bytes_per_frame << endl;
            }
            return pos;
        }

        uint64_t num_stored_frames() {
            return stored_frames.size();
        }

        void close() {
            file->close();
        }



    private:
        ifstream *file;
        vector<Frame> stored_frames;



        //Reads n bytes from buff and correctly converts them from a raw sample to a normalized double in the range -1 to 1
        double n_bytes_to_normalized_double(const char* buff, int16_t n, bool little_endian=true) {
            if (n == 2) {
                return static_cast<double>(n_bytes_to_int<int16_t>(buff, n, little_endian)) * normalization_multiplier;
            }
            else if (n == 3) {
                return static_cast<double>(n_bytes_to_int<int32_t>(buff, n, little_endian)) * normalization_multiplier;
            }
            return 0;
        }

        void add_stored_frame(Frame frame, int64_t idx=-1) {
            if (idx == -1) {
                idx = stored_frames.size();
            }
            if (idx >= stored_frames.size()) {
                stored_frames.push_back(frame);
            }
            else {
                stored_frames[idx] = frame;
            }

        }


};


#endif
