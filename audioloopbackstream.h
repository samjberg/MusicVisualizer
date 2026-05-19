#ifndef AUDIOLOOPBACKSTREAM_H
#define AUDIOLOOPBACKSTREAM_H

#include <iostream>
#include <vector>
#include <span>
#include <cstdint>
#include <fstream>
#include "frame.h"
#include <miniaudio/miniaudio.h>
#include "iaudiostream.h"


void data_callback(struct ma_device *device, void *output_buff, const void *input_buff, ma_uint32 frame_count);

//Class for creating a stream to read a .wav file.  Only works for output, not input, and only for .wav files currently
//Does work with all bit depths, sample rates, number of channels, etc

class AudioLoopbackStream : public IAudioStream {
    public:
        ma_pcm_rb ring_buffer;
        uint64_t total_frames_read;
        uint64_t total_frames_consumed;

        AudioLoopbackStream(uint64_t frames_per_chunk); 
        ~AudioLoopbackStream(); 
        //Reads the next n bytes into a char* buffer from the underlying ifstream.  This function uses the ifstream's position
        char* next_n_bytes(uint64_t n, int64_t start=-1); 
        template<typename numT>
        numT next_n_bytes_sizet(uint64_t n, bool little_endian=true); 
        template <typename numT>
        double as_normalized_double(numT sample); 
        double read_as_normalized_double(); 
        WaveHeader read_header(); 
        Frame next_frame(); 
        std::vector<Frame> read_next_chunk() override; 
        //Gets a std::vector of stored Frames from start_idx to end_idx, indices must be in terms of frames (not bytes or samples)
        //For safety, automatically reads the next chunk to further populate stored_frames if necessary to prevent out of bounds error
        std::vector<Frame> get_stored_frames_at(uint64_t start_idx, uint64_t end_idx); 
        //idx is a frame index.  Passing in idx=N means getting the chunk BEGINNING at the Nth frame;
        std::span<Frame> get_chunk_at(uint64_t idx); 
        //idx is a frame index.  Passing in idx=N means get the chunk CENTERED at the Nth frame.
        std::span<Frame> get_chunk_centered_at(uint64_t idx); 
        std::vector<Frame> next_display_chunk() override; 
        uint64_t curr_pos(); 
        void seek(int64_t n); 
        //Seek forward n bytes from current stream pos in file
        void seek_forward(int64_t n); 
        //Seek backwards n bytes from current stream pos in file
        void seek_backwards(int64_t n); 
        void seek_to_pos(int64_t n); 
        //Fast forward the stream by `seconds` seconds
        uint64_t ff_seconds(double seconds); 
        //Rewind the stream by `seconds` seconds
        uint64_t rewind_seconds(double seconds); 
        uint64_t num_stored_frames(); 
        bool next_chunk_ready(); 
        void close() override; 

    private:
        std::fstream *file;
        std::vector<Frame> stored_frames;
        ma_device device;
        ma_context context;
        uint64_t rb_idx;



        //Reads n bytes from buff and correctly converts them from a raw sample to a normalized double in the range -1 to 1
        double n_bytes_to_normalized_double(const char* buff, int16_t n, bool little_endian=true); 

        void add_stored_frame(Frame frame, int64_t idx=-1); 

        uint64_t total_frames_available() override; 

        void init_loopback(); 

        std::vector<Frame> next_n_frames(uint64_t num_frames); 

        //The argument is not used.  It is there just for compatibility so that this can be an inherited function from IAudioStream
        bool update_playhead_should_play(uint64_t queued_bytes=0) override; 
};


#endif
