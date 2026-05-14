#ifndef AUDIOSTREAM_H
#define AUDIOSTREAM_H

#include <iostream>
#include <vector>
#include <span>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include "iaudiostream.h"
#include "frame.h"
#include <SDL3/SDL_audio.h>

namespace fs = std::filesystem;
using namespace std;


//Class for creating a stream to read a .wav file.  Only works for output, not input, and only for .wav files currently
//Does work with all bit depths, sample rates, number of channels, etc
class AudioStream : public IAudioStream {
    public:
        AudioStream(fs::path path, uint64_t frames_per_chunk) : IAudioStream(42) {
            this->frames_per_chunk = frames_per_chunk;
            stream_type = file_stream;
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
            total_frames_consumed = 0; //total number of frames SENT to the audio device (not necessarily played yet)
            last_update_pos = 0;
            current_playhead = 0;
        }

        AudioStream() : IAudioStream(69) {}


        ~AudioStream() {
            if (file->is_open()) {
                file->close();
            }
        }


        //Reads the next n bytes into a char* buffer from the underlying ifstream.  This function uses the ifstream's position
        inline char* next_n_bytes(uint64_t n, int64_t start=-1) {
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


        inline Chunk read_fmt_chunk() {
            std::string chunk_id_var = _next_n_bytes(file, 4);
            uint64_t chunk_size_var = _next_n_bytes_sizet<uint64_t>(file, 4, true);
            uint64_t format_var = _next_n_bytes_sizet<uint64_t>(file, 2);
            uint64_t num_channels_var = _next_n_bytes_sizet<uint64_t>(file, 2);
            uint64_t sample_rate_var = _next_n_bytes_sizet<uint64_t>(file, 4);
            uint64_t byte_rate_var = _next_n_bytes_sizet<uint64_t>(file, 4);
            uint64_t block_align_var = _next_n_bytes_sizet<uint64_t>(file, 2);
            uint64_t bits_per_sample_var = _next_n_bytes_sizet<uint64_t>(file, 2);
            return Chunk{chunk_id_var, chunk_size_var, format_var, num_channels_var, sample_rate_var, byte_rate_var, block_align_var, bits_per_sample_var};
        }



        inline WaveHeader read_header() {
                //Read the initial "RIFF" bytes
                std::string chunk_id = _next_n_bytes(file, 4);
                // cout << "chunk_id: " << chunk_id << endl;
                uint64_t header_chunk_size = _next_n_bytes_sizet<uint64_t>(file, 4);
                // cout << "header_chunk_size: " << header_chunk_size << endl;
                std::string format = _next_n_bytes(file, 4);
                // cout << "format: " << format << endl;
                // cout << "at end of read_header, file->tell(): " << file->tellg() << endl;
                return WaveHeader{chunk_id, header_chunk_size, format};
        }



        inline uint64_t ff_to_data() {
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

        uint64_t total_frames_available() override {
            return stored_frames.size() - (pos * bytes_per_frame);
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


        vector<Frame> read_next_chunk() override {
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

        vector<Frame> next_display_chunk() override {
            vector<Frame> vec_chunk;
            span<Frame> span_chunk = get_chunk_centered_at(current_playhead);
            vec_chunk.assign(span_chunk.begin(), span_chunk.end());
            return vec_chunk;
            // return get_chunk_centered_at(current_playhead);

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
            int64_t num_bytes = seconds * sample_rate * bytes_per_frame;
            cout << "[ff_seconds] byte offset=" << num_bytes << endl;
            seek_forward(num_bytes);
            auto after = file->tellg();
            pos = min(pos + static_cast<uint64_t>(num_bytes), static_cast<uint64_t>(data_size));
            return pos;
        }

        //Rewind the stream by `seconds` seconds
        uint64_t rewind_seconds(double seconds) {
            cout << "stored_frames.size(): " << stored_frames.size();

            // auto pos = file->tellg();
            int64_t num_bytes = static_cast<int64_t>(seconds * byte_rate);
            seek(-num_bytes);
            auto after = file->tellg();
            pos = max(pos - static_cast<uint64_t>(num_bytes), static_cast<uint64_t>(0));
            return pos;
        }

        uint64_t num_stored_frames() {
            return stored_frames.size();
        }

        bool next_chunk_ready() {
            return true;
        }

        bool update_playhead_should_play(uint64_t queued_bytes) override {
            uint64_t queued_frames = queued_bytes / (sizeof(float) * num_channels);
            current_playhead = total_frames_consumed - queued_frames;
            bool should_play = ((current_playhead - last_update_pos) >= frames_per_chunk);
            if (should_play) {
                last_update_pos = current_playhead;
                return true;
            }
            return false;
        }

        bool put_audiostream_data(vector<Frame>& chunk) {
            float* buff = chunk_to_float32_buff(chunk);
            bool res = SDL_PutAudioStreamData(sdl_audio_stream, buff, chunk.size() * sizeof(float) * num_channels);
            delete[] buff;
            total_frames_consumed += chunk.size();
            return res;
        }

        void set_sdl_audio_stream(SDL_AudioStream* sdl_as) {
            sdl_audio_stream = sdl_as;
        }




        void close() override {
            file->close();
        }



    private:
        SDL_AudioStream *sdl_audio_stream; //pointer to the SDL_AudioStream, used for putting audio data to the sound card
        uint64_t last_update_pos = 0;

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
