


//objective-c imports
/* #import <Foundation/Foundation.h> */
#import <CoreAudio/CoreAudio.h>
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreMedia/CMSampleBuffer.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <uuid/uuid.h>
#include <Security/Security.h>

#ifdef __APPLE__
    #define Size MacTypes_Size;
    #define Rect MacTypes_Rect;
    #define Pos  MacTypes_Pos;
#endif

#include "iaudiostream.h"
#include "audioloopbackstream.h"
//c++ includes
#include <iostream>
#include <vector>
#include <span>
#include <cstdint>
#include <fstream>
#include "frame.h"
#include <miniaudio/miniaudio.h>


SCStream *stream = nil;

@interface AudioBufferReceiver : NSObject <SCStreamOutput> {
    AudioLoopbackStream *inst;

}
- (instancetype)initWithCppInstance:(AudioLoopbackStream *)cpp_instance;
@end

@implementation AudioBufferReceiver

- (instancetype)initWithCppInstance:(AudioLoopbackStream *)cpp_instance {
    self = [super init];
    if (self) {
        inst = cpp_instance;
    }
    return self;
}

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type {
    if (type == SCStreamOutputTypeAudio) {
        
        // 1. Determine the total frame sample count inside this buffer chunk
        CMItemCount numSamples = CMSampleBufferGetNumSamples(sampleBuffer);
        if (numSamples == 0) return;

        // 2. Allocate stack storage for a 2-channel AudioBufferList mapping
        char bufferListStorage[sizeof(AudioBufferList) + sizeof(AudioBuffer)];
        AudioBufferList *bufferList = (AudioBufferList *)bufferListStorage;
        CMBlockBufferRef blockBuffer = nullptr;

        // 3. Populate our structure with pointers to the underlying audio frames
        OSStatus status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
            sampleBuffer,
            nullptr,
            bufferList,
            sizeof(bufferListStorage),
            nullptr,
            nullptr,
            kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment,
            &blockBuffer
        );

        if (status == noErr) {
            // 4. Cast the raw pointers directly to standard C++ float arrays
            // mBuffers[0] handles Left channel samples; mBuffers[1] handles Right channel samples
            float *leftChannel  = (float *)bufferList->mBuffers[0].mData;
            float *rightChannel = (float *)bufferList->mBuffers[1].mData;

            // 5. Instantiate your flat sequential processing target vector
            /* std::vector<float> interleavedData(numSamples * 2); */
            float *float_buff = new float[numSamples * 2];
            ma_uint32 frames_to_write = numSamples;

            // 6. Transpose the planar tracks into your expected [L, R, L, R...] configuration
            for (CMItemCount i = 0; i < numSamples; ++i) {
                /* interleavedData[i * 2]     = leftChannel[i]; */
                /* interleavedData[i * 2 + 1] = rightChannel[i]; */
                float_buff[i * 2]     = leftChannel[i];
                float_buff[i * 2 + 1] = rightChannel[i];
                /* std::cout << leftChannel[i] << "\t" << rightChannel[i] << std::endl; */
                /* inst->ring_buffer */
            }

            const float *buff = (const float*)float_buff;
            ma_uint32 frame_count = numSamples;
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







            /* ins */

            // --- PASSTHROUGH BOUNDARY ---
            // The data inside interleavedData is now packed perfectly. 
            // You can pass raw memory access pointers (&interleavedData[0]) directly
            // into your existing visualization or FFT processing structures here.
        }

        // 7. CoreMedia memory stewardship requirement
        if (blockBuffer) {
            CFRelease(blockBuffer);
        }
    }
}
@end




//Class for creating a stream to read a .wav file.  Only works for output, not input, and only for .wav files currently
//Does work with all bit depths, sample rates, number of channels, etc

// class AudioLoopbackStream : public IAudioStream {
    // public:
        // uint64_t total_frames_read;
        // uint64_t total_frames_consumed;


AudioLoopbackStream::AudioLoopbackStream(uint64_t frames_per_chunk) : IAudioStream(69) {
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


AudioLoopbackStream::~AudioLoopbackStream() {
    close();
}


//Reads the next n bytes into a char* buffer from the underlying ifstream.  This function uses the ifstream's position
char* AudioLoopbackStream::next_n_bytes(uint64_t n, int64_t start) {
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
numT AudioLoopbackStream::next_n_bytes_sizet(uint64_t n, bool little_endian) {
    char* s = next_n_bytes(n);
    return n_bytes_to_int<numT>(s, n);
}

template <typename numT>
double AudioLoopbackStream::as_normalized_double(numT sample) {
    if (bits_per_sample == 16) {
        return static_cast<double>(sample) / 32786.0;
    }
    else if (bits_per_sample == 24) {
        return static_cast<double>(sample) / 8388608.0; //8388608 is 2^24 / 2
    }
    return 0.0;
}

double AudioLoopbackStream::read_as_normalized_double() {
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


WaveHeader AudioLoopbackStream::read_header() {
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


Frame AudioLoopbackStream::next_frame() {
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



std::vector<Frame> AudioLoopbackStream::read_next_chunk() {
    return next_n_frames(frames_per_chunk);
}

//Gets a std::vector of stored Frames from start_idx to end_idx, indices must be in terms of frames (not bytes or samples)
//For safety, automatically reads the next chunk to further populate stored_frames if necessary to prevent out of bounds error
std::vector<Frame> AudioLoopbackStream::get_stored_frames_at(uint64_t start_idx, uint64_t end_idx) {
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
std::span<Frame> AudioLoopbackStream::get_chunk_at(uint64_t idx) {
    uint64_t end_idx = std::min(idx + chunk_size, data_size/bytes_per_frame);
    return std::span<Frame>(&stored_frames[idx], end_idx-idx);
    // return get_stored_frames_at(idx, end_idx);
}


//idx is a frame index.  Passing in idx=N means get the chunk CENTERED at the Nth frame.
std::span<Frame> AudioLoopbackStream::get_chunk_centered_at(uint64_t idx) {
    // uint64_t start_idx = std::max(idx - (frames_per_chunk / 2), static_cast<uint64_t>(0));
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

std::vector<Frame> AudioLoopbackStream::next_display_chunk() {
    return read_next_chunk();
}

uint64_t AudioLoopbackStream::curr_pos() {
    return static_cast<uint64_t>(file->tellg());

}

void AudioLoopbackStream::seek(int64_t n) {

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
void AudioLoopbackStream::seek_forward(int64_t n) {
    file->seekg(n, std::ios_base::cur);
}

//Seek backwards n bytes from current stream pos in file
void AudioLoopbackStream::seek_backwards(int64_t n) {
    std::streamoff offset = static_cast<std::streamoff>(n);
    file->seekg(-offset, std::ios_base::cur);
}

void AudioLoopbackStream::seek_to_pos(int64_t n) {
    file->seekg(n, std::ios_base::beg);
}

//Fast forward the stream by `seconds` seconds
uint64_t AudioLoopbackStream::ff_seconds(double seconds) {

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
uint64_t AudioLoopbackStream::rewind_seconds(double seconds) {
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

uint64_t AudioLoopbackStream::num_stored_frames() {
    return stored_frames.size();
}

bool AudioLoopbackStream::next_chunk_ready() {
    return true;
}

void AudioLoopbackStream::close() {
    // file->close();
}



// private:
// std::fstream *file;
// std::vector<Frame> stored_frames;
// ma_pcm_rb ring_buffer;
// ma_device device;
// ma_context context;
// uint64_t rb_idx;
//


//Reads n bytes from buff and correctly converts them from a raw sample to a normalized double in the range -1 to 1
double AudioLoopbackStream::n_bytes_to_normalized_double(const char* buff, int16_t n, bool little_endian) {
    if (n == 2) {
        return static_cast<double>(n_bytes_to_int<int16_t>(buff, n, little_endian)) * normalization_multiplier;
    }
    else if (n == 3) {
        return static_cast<double>(n_bytes_to_int<int32_t>(buff, n, little_endian)) * normalization_multiplier;
    }
    return 0;
}

void AudioLoopbackStream::add_stored_frame(Frame frame, int64_t idx) {
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

uint64_t AudioLoopbackStream::total_frames_available() {
    uint64_t frames_available = total_frames_read - total_frames_consumed;
    // std::cout << "frames_available: " << frames_available << std::endl;
    return frames_available;
    // return total_frames_read - total_frames_consumed;
}

void AudioLoopbackStream::init_loopback() {

    //Initialize class variables
    num_channels = 2;
    sample_rate = 48000;
    std::cout << "num_channels: " << num_channels << std::endl;

    /*--------------------------------------------------*/
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
    /*--------------------------------------------------*/


    auto res = ma_pcm_rb_init(ma_format_f32, static_cast<ma_uint32>(num_channels), static_cast<ma_uint32>(frames_per_chunk * 5), NULL, NULL, &ring_buffer);
    std::cout << "ma_pcm_rb_init result: " << res << std::endl;


    // Objective-C memory management scope
    @autoreleasepool {
        std::cout << "Starting ScreenCaptureKitTest Build Verification..." << std::endl;

        AudioObjectID aggDeviceID;
        AudioDeviceIOProcID adiopid;
        std::cout << "Requesting available screen/audio content from macOS..." << std::endl;
        SCStreamConfiguration *conf = [[SCStreamConfiguration alloc] init];
        conf.capturesAudio = YES;
        conf.captureMicrophone = NO;
        conf.excludesCurrentProcessAudio = YES;
        conf.minimumFrameInterval = CMTime{10, 1};
        conf.channelCount = 2;
        conf.sampleRate = 48000;

        AudioBufferReceiver *receiver = [[AudioBufferReceiver alloc] initWithCppInstance:this];

        dispatch_queue_t audioProcessingQueue = dispatch_queue_create("com.sam.AudioCaptureQueue", DISPATCH_QUEUE_SERIAL);


        // This is a standard asynchronous call in ScreenCaptureKit
        [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
            if (error) {
                std::cout << "Error retrieving content: " << error.localizedDescription.UTF8String << std::endl;
            } else {
                std::cout << "Successfully connected to ScreenCaptureKit!" << std::endl;
                /* content.displays; */
                NSArray<SCDisplay*> *displays =  content.displays;
                SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:displays[0] excludingApplications:@[] exceptingWindows:@[]];
                stream = [[SCStream alloc] initWithFilter:filter configuration:conf delegate:NULL];

                NSError *outputError = nil;
                [stream addStreamOutput:receiver type:SCStreamOutputTypeAudio sampleHandlerQueue:audioProcessingQueue error:&outputError];
                if (outputError) {
                    std::cout << "Failed to attach audio output destination: " << outputError.localizedDescription.UTF8String << std::endl;

                }
                /* [stream addStreamOutput:(nonnull id<SCStreamOutput>) type:(SCStreamOutputType) sampleHandlerQueue:(dispatch_queue_t _Nullable) error:(NSError * _Nullable * _Nullable) */
                /* [stream startCaptureWithCompletionHandler:NULL] */
                /* stream->startCapture; */
                /* stream.addStream */
                NSUInteger count = [displays count];
                std::cout << "Number of displays: " << count << std::endl;
                for (SCDisplay *disp in displays) {
                    uint32_t displayID = [disp displayID];
                    std::cout << "displayID: " << displayID << std::endl;
                    NSArray<NSString*> *attribute_keys = [disp attributeKeys];
                }


                [stream startCaptureWithCompletionHandler:^(NSError *error) {
                    if (error) {
                        std::cout << "Failed to initiate stream capture: " << error.localizedDescription.UTF8String << std::endl;

                    }
                    else {
                        std::cout << "ScreenCaptureKit pipeline is actively recording loopback audio!" << std::endl;
                    }

                }];
            }
        }];

        // Keep the command line app alive briefly to allow the async call to respond
        [NSThread sleepForTimeInterval:4.0];
    }

    
    
    std::cout << "num_channels: " << num_channels << std::endl;
    // std::cout << "frames_per_chunk
    //

    /* ma_format_f32 */



    /* device.pUserData = this; */



}

std::vector<Frame> AudioLoopbackStream::next_n_frames(uint64_t num_frames) {
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
bool AudioLoopbackStream::update_playhead_should_play(uint64_t queued_bytes) {
    return total_frames_available() >= frames_per_chunk;
}

void data_callback(struct ma_device *device, void *output_buff, const void *input_buff, ma_uint32 frame_count) {
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

// };

