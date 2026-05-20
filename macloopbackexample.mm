#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreMedia/CMSampleBuffer.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <uuid/uuid.h>
#include <iostream>

SCStream *stream = nil;


@interface AudioBufferReceiver : NSObject <SCStreamOutput>
@end
@implementation AudioBufferReceiver
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
            std::vector<float> interleavedData(numSamples * 2);

            // 6. Transpose the planar tracks into your expected [L, R, L, R...] configuration
            for (CMItemCount i = 0; i < numSamples; ++i) {
                interleavedData[i * 2]     = leftChannel[i];
                interleavedData[i * 2 + 1] = rightChannel[i];
                std::cout << leftChannel[i] << "\t" << rightChannel[i] << std::endl;
            }

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


int main() {
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
        kAudioFormatLinearPCM;



        AudioBufferReceiver *receiver = [[AudioBufferReceiver alloc] init];

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
    return 0;
}



