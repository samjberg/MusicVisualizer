#ifndef FRAME_H
#define FRAME_H

#include <cstdint>
#include <vector>
#include <stringtools.h>


template <typename numT>
inline numT n_bytes_to_int(const char* buff, int16_t n, bool little_endian=true) {
    numT a = 0;
    for (int i=0; i<n; i++) {
        numT j = n - i - 1;
        numT shift_amount = 8 * j;
        a |= numT((unsigned char)(little_endian ? buff[j] : buff[i]) << shift_amount);
    }
    return a;
}


//A single frame (a single time slice) of an audio file
template <typename numT>
class Frame {
    public:
        uint16_t num_channels;
        std::vector<numT> channels;
        Frame(int32_t num_channels, char *buff, uint64_t bytes_per_sample) : num_channels(num_channels) {
            // std::vector<char*> buffs = segmentcstr(buff, num_channels, full_buffsize);
            // uint64_t bytes_per_sample = full_buffsize / num_channels;
            // std::cout << "bytes_per_sample: " << bytes_per_sample << std::endl;
            this->channels.reserve(num_channels);
            for (int i=0; i<num_channels; ++i) {
                const char *channel_pointer = buff + (i * bytes_per_sample);
                this->channels[i] = n_bytes_to_int<numT>(channel_pointer, bytes_per_sample, true); 
            }

        }

        //Returns the total number of bytes taken up by the samples stored in channels
        uint64_t size_bytes() {
            return sizeof(numT) * num_channels;
        }

};

template <typename numT>
inline std::ostream& operator<<(std::ostream& out, Frame<numT> frame) {
    for (numT channelval : frame.channels) {
        out << channelval;
    }
    return out;
}

#endif
