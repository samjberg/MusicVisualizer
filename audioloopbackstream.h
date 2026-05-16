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
#include <rtaudio/rtaudio.h>

// using namespace std;



//Class for creating a stream to read a .wav file.  Only works for output, not input, and only for .wav files currently
//Does work with all bit depths, sample rates, number of channels, etc

class AudioLoopbackStream : public IAudioStream {
    public:
        uint64_t total_frames_read;
        uint64_t total_frames_consumed;

        AudioLoopbackStream(uint64_t frames_per_chunk) : IAudioStream(69) {
            // filestream_setup("asdf", frames_per_chunk);
            total_frames_read = 0;
            total_frames_consumed = 0;
            this->frames_per_chunk = frames_per_chunk;
            std::cout << "before init\n";
            init_loopback();
            std::cout << "after init\n";

            stream_type = loopback_stream;
            rb_idx = 0;


        }


        ~AudioLoopbackStream() {
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
            std::string chunk_id = next_n_bytes(4);
            // std::cout << "chunk_id: " << chunk_id << std::endl;
            uint64_t header_chunk_size = next_n_bytes_sizet<uint64_t>(4);
            // std::cout << "header_chunk_size: " << header_chunk_size << std::endl;
            std::string format = next_n_bytes(4);
            // std::cout << "format: " << format << std::endl;
            // std::cout << "at end of read_header, file->tell(): " << file->tellg() << std::endl;
            return WaveHeader{chunk_id, header_chunk_size, format};
        }


        Frame next_frame() {
            uint64_t curr_frame_idx = pos / bytes_per_frame;
            if (curr_frame_idx >= stored_frames.size()) {
                std::vector<double> vals(num_channels);
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



        std::vector<Frame> read_next_chunk() override {
            return next_n_frames(frames_per_chunk);
        }

        //Gets a std::vector of stored Frames from start_idx to end_idx, indices must be in terms of frames (not bytes or samples)
        //For safety, automatically reads the next chunk to further populate stored_frames if necessary to prevent out of bounds error
        std::vector<Frame> get_stored_frames_at(uint64_t start_idx, uint64_t end_idx) {
            std::vector<Frame> chunk(frames_per_chunk);
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
        std::span<Frame> get_chunk_at(uint64_t idx) {
            uint64_t end_idx = std::min(idx + chunk_size, data_size/bytes_per_frame);
            return std::span<Frame>(&stored_frames[idx], end_idx-idx);
            // return get_stored_frames_at(idx, end_idx);
        }


        //idx is a frame index.  Passing in idx=N means get the chunk CENTERED at the Nth frame.
        std::span<Frame> get_chunk_centered_at(uint64_t idx) {
            // uint64_t start_idx = max(idx - (frames_per_chunk / 2), static_cast<uint64_t>(0));
            uint64_t start_idx = (idx > (frames_per_chunk / 2)) ? (idx - (frames_per_chunk / 2)) : 0;
            uint64_t end_idx = std::min(start_idx + frames_per_chunk, static_cast<uint64_t>(stored_frames.size()));
            return std::span<Frame>(&stored_frames[start_idx], end_idx - start_idx);
            // // std::cout << "chunk centered at: " << idx << " starts at: " << start_idx << " and ends at: " << end_idx << std::endl;
            // std::vector<Frame> chunk(frames_per_chunk);
            // for (int i=start_idx; i<end_idx; ++i) {
            //     chunk[i-start_idx] = stored_frames[i];
            // }
            // return chunk;
        }

        std::vector<Frame> next_display_chunk() override {
            return read_next_chunk();
        }

        uint64_t curr_pos() {
            return static_cast<uint64_t>(file->tellg());

        }

        void seek(int64_t n) {

            std::cout << "[seek] enter n=" << n
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << std::endl;
            auto before = file->tellg();
            std::cout << "[seek] tellg before seek=" << before
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << std::endl;
            file->seekg(n, std::ios_base::cur);
            auto after = file->tellg();
            std::cout << "[seek] tellg after seek=" << after
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << std::endl;
        }

        //Seek forward n bytes from current stream pos in file
        void seek_forward(int64_t n) {
            file->seekg(n, std::ios_base::cur);
        }

        //Seek backwards n bytes from current stream pos in file
        void seek_backwards(int64_t n) {
            std::streamoff offset = static_cast<std::streamoff>(n);
            file->seekg(-offset, std::ios_base::cur);
        }

        void seek_to_pos(int64_t n) {
            file->seekg(n, std::ios_base::beg);
        }

        //Fast forward the stream by `seconds` seconds
        uint64_t ff_seconds(double seconds) {

            std::cout << "[ff_seconds] enter seconds=" << seconds
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << std::endl;
            std::cout << "[ff_seconds] tellg before seek=" << pos
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << std::endl;
            int64_t num_bytes = seconds * sample_rate * bytes_per_frame;
            std::cout << "[ff_seconds] byte offset=" << num_bytes << std::endl;
            seek_forward(num_bytes);
            auto after = file->tellg();
            std::cout << "[ff_seconds] tellg after seek=" << after
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << std::endl;

            pos = std::min(pos + static_cast<uint64_t>(num_bytes), static_cast<uint64_t>(data_size));
            return pos;
        }

        //Rewind the stream by `seconds` seconds
        uint64_t rewind_seconds(double seconds) {
            std::cout << "stored_frames.size(): " << stored_frames.size();

            // auto pos = file->tellg();
            std::cout << "[rewind_seconds] tellg before seek=" << pos
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << std::endl;
            int64_t num_bytes = static_cast<int64_t>(seconds * byte_rate);
            std::cout << "[rewind_seconds] byte offset=" << -num_bytes << std::endl;
            seek(-num_bytes);
            auto after = file->tellg();
            std::cout << "[rewind_seconds] tellg after seek=" << after
                 << " good=" << file->good()
                 << " eof=" << file->eof()
                 << " fail=" << file->fail()
                 << " bad=" << file->bad()
                 << std::endl;
            pos = std::max(pos - static_cast<uint64_t>(num_bytes), static_cast<uint64_t>(0));
            for (int i=0; i<20; ++i) {
                std::cout << "pos: " << pos << std::endl;
                std::cout << "audio_stream->pos: " << this->pos << std::endl;
                std::cout << "pos frame (pos/bytes_per_frame): " << pos / bytes_per_frame << std::endl;
            }
            return pos;
        }

        uint64_t num_stored_frames() {
            return stored_frames.size();
        }

        bool next_chunk_ready() {
            return true;
        }

        void close() override {
            audio.closeStream();
            // file->close();
        }



    private:
        std::fstream *file;
        std::vector<Frame> stored_frames;
        RtAudio audio;
        ma_pcm_rb ring_buffer;
        ma_device device;
        ma_context context;
        uint64_t rb_idx;
        unsigned int buffer_frames = 512; //Number of frames in the actual RtAudio stream buffer
        RtAudio::DeviceInfo info;
        RtAudio::StreamParameters parameters;



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

        uint64_t total_frames_available() override {
            uint64_t frames_available = total_frames_read - total_frames_consumed;
            // std::cout << "frames_available: " << frames_available << std::endl;
            return frames_available;
            // return total_frames_read - total_frames_consumed;
        }

        void init_loopback_rt() {

            std::vector<uint32_t> ids = audio.getDeviceIds();
            unsigned int device_id = audio.getDefaultOutputDevice();
            RtAudio::DeviceInfo info = audio.getDeviceInfo(device_id);

            std::unordered_map<unsigned int, std::string> errmap;
            errmap[RTAUDIO_NO_ERROR] = "RTAUDIO_NO_ERROR";
            errmap[RTAUDIO_WARNING] = "RTAUDIO_WARNING";
            errmap[RTAUDIO_UNKNOWN_ERROR] = "RTAUDIO_UNKNOWN_ERROR";
            errmap[RTAUDIO_NO_DEVICES_FOUND] = "RTAUDIO_NO_DEVICES_FOUND";
            errmap[RTAUDIO_INVALID_DEVICE] = "RTAUDIO_INVALID_DEVICE";
            errmap[RTAUDIO_DEVICE_DISCONNECT] = "RTAUDIO_DEVICE_DISCONNECT";
            errmap[RTAUDIO_MEMORY_ERROR] = "RTAUDIO_MEMORY_ERROR";
            errmap[RTAUDIO_INVALID_PARAMETER] = "RTAUDIO_INVALID_PARAMETER";
            errmap[RTAUDIO_INVALID_USE] = "RTAUDIO_INVALID_USE";
            errmap[RTAUDIO_DRIVER_ERROR] = "RTAUDIO_DRIVER_ERROR";
            errmap[RTAUDIO_SYSTEM_ERROR] = "RTAUDIO_SYSTEM_ERROR";
            errmap[RTAUDIO_THREAD_ERROR] = "RTAUDIO_THREAD_ERROR";
            unsigned int sample_rate = info.currentSampleRate;
            unsigned int num_channels = info.outputChannels;
            unsigned int bytes_per_frame = 4 * num_channels;
            RtAudio::StreamParameters parameters;
            parameters.deviceId = device_id;
            parameters.nChannels = num_channels;

            // audio.getS

            // RtAudioErrorType e;

            RtAudioErrorType e = audio.openStream(&parameters, NULL, RTAUDIO_FLOAT32, static_cast<unsigned int>(sample_rate), &buffer_frames, data_callback_rt, this);

            if (e != RTAUDIO_NO_ERROR) {
                std::cout << "ERROR: " << e << std::endl;

            }

            std::cout << "Using audio output source: " << info.name << std::endl;


        


        }

        void init_loopback() {

            ma_backend backends[] = { ma_backend_wasapi };
            if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
                std::cout << "Error 1\n";
                // Error.
            }
            ma_device_info* pPlaybackInfos;
            ma_uint32 playbackCount;
            ma_device_info* pCaptureInfos;
            ma_uint32 captureCount;
            // ma_get_ch
            if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
                std::cout << "error 2\n";
                // Error.
            }

            for (int i=0; i<captureCount; ++i) {
                std::cout << "Capture Device " << i << ": " << pCaptureInfos[i].name << std::endl;
            }

            for (int i=0; i<playbackCount; ++i) {
                std::cout << "Playback Device " << i << ": " << pPlaybackInfos[i].name << std::endl;
            }

            ma_device_config config = ma_device_config_init(ma_device_type_loopback);
            config.capture.pDeviceID = &pPlaybackInfos[0].id;
            num_channels = config.playback.channels;
            config.capture.format = ma_format_f32;
            config.capture.channels = 2;
            config.sampleRate = 44100;
            config.dataCallback = data_callback;





            // ma_device device;
            ma_result dev_init_res = ma_device_init(&context, &config, &device);
            for (int i=0; i<10; ++i) {
                std::cout << "device init result!!!!!!!!!!!!!!!!!!!!!!!!:   " << dev_init_res;
            }
            num_channels = device.capture.channels;
            sample_rate = device.capture.internalSampleRate;
            std::cout << "num_channels: " << num_channels << std::endl;

            num_channels = 2;
            // std::cout << "num_
            // bytes_per_frame = ma_get_bytes_per_frame(ma_format_f32, num_channels);
            bytes_per_frame = ma_get_bytes_per_frame(ma_format_f32, num_channels);
            std::cout << "bytes_per_frame: " << bytes_per_frame;
            bytes_per_sample = bytes_per_frame / num_channels;
            std::cout << "bytes_per_sample: " << bytes_per_sample;
            bits_per_frame = bytes_per_frame * 8;
            bits_per_sample = bytes_per_sample * 8;
            // sample_rate = config.sampleRate;
            // sample_rate = 44100;
            std::cout << "sample_rate: " << sample_rate << std::endl;
            byte_rate = sample_rate * bytes_per_frame;
            chunk_size = frames_per_chunk * bytes_per_frame;
            uint64_t normalization_divisor = 2 << (bits_per_sample - 2);
            normalization_multiplier = 1 / static_cast<double>(normalization_divisor);
            // data_size = 1024 * 1024 * 20;
            data_size = frames_per_chunk * 4;
            stored_frames.reserve(data_size);

            std::cout << "device.capture.format: " << device.capture.format << std::endl;
            std::cout << "num_channels: " << num_channels << std::endl;
            // std::cout << "frames_per_chunk


            auto res = ma_pcm_rb_init(device.capture.format, static_cast<ma_uint32>(num_channels), static_cast<ma_uint32>(frames_per_chunk * 5), NULL, NULL, &ring_buffer);
            std::cout << "ma_pcm_rb_init result: " << res << std::endl;

            device.pUserData = this;

            auto dev_res = ma_device_start(&device);
            std::cout << "dev start result: " << dev_res << std::endl;


        }

        std::vector<Frame> next_n_frames(uint64_t num_frames) {
            void *read_buffer;
            ma_uint32 frames_to_read = num_frames;
            // std::cout << "FRAMES_TO_READ BEFORE: " << frames_to_read << std::endl;
            ma_pcm_rb_acquire_read(&ring_buffer, &frames_to_read, &read_buffer);
            // for (int i=0; i<10; i++) {
            //     std::cout << "num_channels: " << num_channels << std::endl;
            //     std::cout << "data_size: " << data_size << std::endl;
            //     std::cout << "stored_frames.size(): " << stored_frames.size() << std::endl;
            // }
            // std::cout << "FRAMES_TO_READ AFTER: " << frames_to_read << std::endl;

            float* f32_read_buffer = (float*)read_buffer;

            std::vector<Frame> frames(num_frames);
            //Using frames_to_read instead of frames_per_chunk is necessary in case we hit the end of the ring buffer
            //In which case we will perform another read after this first one already performed above


            for (int i=0; i<frames_to_read; ++i) {
                // std::cout << i << std::endl;
                // std::cout << "stored_frames.size(): " << stored_frames.size() << std::endl;
                // std::cout << "num_channels: " << num_channels << std::endl;
                std::vector<double> channel_vals(num_channels);
                for (int ch=0; ch<num_channels; ++ch) {
                    channel_vals[ch] = f32_read_buffer[(i*num_channels) + ch];
                }
                Frame frame(num_channels, channel_vals);
                frames[i] = frame;
                if (stored_frames.size() < data_size) {
                    stored_frames.push_back(frame);
                }
                else {
                    //Copy the last chunk of frames to the beginning of new cleared stored_frames
                    std::vector<Frame> tmp(stored_frames.begin() + (stored_frames.size() - frames_per_chunk), stored_frames.end());
                    stored_frames.clear();
                    stored_frames.reserve(data_size);
                    for (int i=0; i<tmp.size(); ++i) {
                        stored_frames.push_back(tmp[i]);
                    }
                    //And now push back the new frame
                    stored_frames.push_back(frame);
                }
            }
            ma_pcm_rb_commit_read(&ring_buffer, frames_to_read);
            total_frames_consumed += frames_to_read;

            ma_uint32 frames_remaining = num_frames - frames_to_read;
            //If we read <frames_per_chunk frames (due to being near the end of the ring_buffer), perform another read to get a full chunk
            if (frames_remaining > 0) {
                void *read_buffer;
                ma_uint32 start_frame_idx = num_frames - frames_remaining;
                ma_uint32 frames_to_read = frames_remaining;
                ma_pcm_rb_acquire_read(&ring_buffer, &frames_remaining, &read_buffer);
                float* f32_read_buffer = (float*)read_buffer;
                for (int i=0; i<frames_remaining; ++i) {
                    std::vector<double> channel_vals(num_channels);
                    for (int ch=0; ch<num_channels; ++ch) {
                        channel_vals[ch] = f32_read_buffer[(i*num_channels)+ch];
                    }
                    Frame frame(num_channels, channel_vals);
                    frames[i+start_frame_idx] = frame;
                    if (stored_frames.size() < data_size) {
                        stored_frames.push_back(frame);
                    }
                    else {
                        //Copy the last chunk of frames to the beginning of new cleared stored_frames
                        std::vector<Frame> tmp(stored_frames.begin() + (stored_frames.size() - frames_per_chunk), stored_frames.end());
                        stored_frames.clear();
                        stored_frames.reserve(data_size);
                        for (int i=0; i<tmp.size(); ++i) {
                            stored_frames.push_back(tmp[i]);
                        }
                        //And now push back the new frame
                        stored_frames.push_back(frame);
                    }
                }
                ma_pcm_rb_commit_read(&ring_buffer, frames_remaining);
                total_frames_consumed += frames_remaining;
            }
            return frames;
        }

        //The argument is not used.  It is there just for compatibility so that this can be an inherited function from IAudioStream
        bool update_playhead_should_play(uint64_t queued_bytes=0) override {
            RtAudioCallback c;
            return total_frames_available() >= frames_per_chunk;
        }
//int (*)(void *, void *, unsigned int, double, RtAudioStreamStatus, void *) (aka int (*)(void *, void *, unsigned int, double, unsigned int, void *))
        static int data_callback_rt(void *output_buff, void *input_buff, unsigned int frame_count,
                                    double stream_time, unsigned int status, void *user_data) {
            RtAudioCallback a;
            auto *inst = static_cast<AudioLoopbackStream*>(user_data);
            ma_uint32 frames_to_write = frame_count;
            uint64_t num_channels = inst->num_channels;
            const float *buff = (const float*)input_buff;
            std::vector<float> samples;
            std::vector<Frame> frames(frame_count);
            void *write_buffer;
            ma_pcm_rb_acquire_write(&(inst->ring_buffer), &frames_to_write, &write_buffer);
            memcpy(write_buffer, buff, frames_to_write * static_cast<ma_uint32>(inst->bytes_per_frame));
            ma_pcm_rb_commit_write(&(inst->ring_buffer), frames_to_write);
            inst->total_frames_read += frames_to_write;
            //
            ma_uint32 offset = frames_to_write * inst->num_channels;
            ma_uint32 remaining_frames = frame_count - frames_to_write;
            if (remaining_frames > 0) {
                void *write_buffer;
                ma_uint32 frames_to_write = remaining_frames;
                ma_pcm_rb_acquire_write(&(inst->ring_buffer), &frames_to_write, &write_buffer);
                memcpy(write_buffer, buff+offset, frames_to_write * static_cast<ma_uint32>(inst->bytes_per_frame));
                ma_pcm_rb_commit_write(&(inst->ring_buffer), frames_to_write);
                inst->total_frames_read += frames_to_write;
            }

            return 0;
        }

        static void data_callback(struct ma_device *device, void *output_buff, const void *input_buff, ma_uint32 frame_count) {
            auto *inst = static_cast<AudioLoopbackStream*>(device->pUserData);
            ma_uint32 frames_to_write = frame_count;
            uint64_t num_channels = inst->num_channels;
            const float *buff = (const float*)input_buff;
            std::vector<float> samples;
            std::vector<Frame> frames(frame_count);
            void *write_buffer;
            ma_pcm_rb_acquire_write(&(inst->ring_buffer), &frames_to_write, &write_buffer);
            memcpy(write_buffer, buff, frames_to_write * static_cast<ma_uint32>(inst->bytes_per_frame));
            ma_pcm_rb_commit_write(&(inst->ring_buffer), frames_to_write);
            inst->total_frames_read += frames_to_write;

            ma_uint32 offset = frames_to_write * inst->num_channels;
            ma_uint32 remaining_frames = frame_count - frames_to_write;
            if (remaining_frames > 0) {
                void *write_buffer;
                ma_uint32 frames_to_write = remaining_frames;
                ma_pcm_rb_acquire_write(&(inst->ring_buffer), &frames_to_write, &write_buffer);
                memcpy(write_buffer, buff+offset, frames_to_write * static_cast<ma_uint32>(inst->bytes_per_frame));
                ma_pcm_rb_commit_write(&(inst->ring_buffer), frames_to_write);
                inst->total_frames_read += frames_to_write;
            }
        }

};


#endif
