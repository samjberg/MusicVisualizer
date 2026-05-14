#ifndef LIVEAUDIO_H
#define LIVEAUDIO_H

#include <cstdint>
#include <miniaudio/miniaudio.h>
#include <iostream>
// ma_device_data_proc void (*)(struct ma_device *, void *, const void *, unsigned int)

inline void audio_callback(struct ma_device* device, void* output_buff, const void* input_buff, uint32_t frame_count) {

}


inline void testfunc() {
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        // Error.
    }
    
    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;
    if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        // Error.
    }

    for (int i=0; i<captureCount; ++i) {
        std::cout << "Device " << i << ": " << pCaptureInfos[i].name << std::endl;

    }
    ma_device_config config = ma_device_config_init(ma_device_type_loopback);

    std::cout << "a" << std::endl;

    // config.capture.pDeviceID = &pPlaybackInfos[chosenPlaybackDeviceIndex].id;
    // config.playback.format    = MY_FORMAT;
    // config.playback.channels  = MY_CHANNEL_COUNT;
    // config.sampleRate         = MY_SAMPLE_RATE;
    // config.dataCallback       = data_callback;
    // config.pUserData          = pMyCustomData;

    // ma_decoder_init(ma_decoder_read_proc onRead, ma_decoder_seek_proc onSeek, void *pUserData, const ma_decoder_config *pConfig, ma_decoder *pDecoder)
    // ma_decoder_get_data_format(&decoder, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
    config.dataCallback = &audio_callback;
}


#endif
