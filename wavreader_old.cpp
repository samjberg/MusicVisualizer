
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <bitset>
#include <utility>
#include "frame.h"
#include "fft.h"
#include "audiostream.h"
#include <numbers>

namespace fs = std::filesystem;
using namespace std;


// const string test_fname = "output.wav";
const string test_fname = "footstepswav.wav";
const uint64_t FAILED = string::npos;


struct Sample {
    int16_t x;
    int16_t y;
    int16_t z;

};


streamsize get_file_size(ifstream &f) {
    //For now I simply mandate that the stream must be at position 0 to use this method
    if (f.tellg() != 0) {
        return FAILED;
    }
    f.seekg(0, ios::end);
    streamsize size = f.tellg();
    f.seekg(0, ios::end);
    return size;
}



char* next_n_bytes(ifstream &f, uint64_t n, bool little_endian=true) {
    //we never read more than 16 bytes
    char *buff = new char[n+1];
    f.read(buff, n);
    buff[n] = '\0';
    return buff;
}

int32_t n_bytes_to_int(const char* buff, int16_t n, bool little_endian=true) {
    int32_t a = 0;
    for (int32_t i=0; i<n; i++) {
        int32_t j = n - i - 1;
        int32_t shift_amount = 8 * j;
        a |= int32_t((unsigned char)(little_endian ? buff[j] : buff[i]) << shift_amount);
    }
    return a;
}


uint64_t next_n_bytes_sizet(ifstream &f, uint64_t n, bool little_endian=true) {
    // cout << "converting " << n << " bytes to uint64_t" << endl;
    char* s = next_n_bytes(f, n);
    return n_bytes_to_int(s, n, little_endian);
}




WaveHeader read_header(ifstream &f) {
    //determine the size of the file
    char buff[8];

    //Read the initial "RIFF" bytes
    string chunk_id = next_n_bytes(f, 4, false);
    cout << "chunk_id: " << chunk_id << endl;
    uint64_t chunk_size = next_n_bytes_sizet(f, 4);
    cout << "chunk_size: " << chunk_size << endl;
    string format = next_n_bytes(f, 4, false);
    cout << "format: " << format << endl;
    // string format = next_n_bytes(f, 
    // f.close();
    return WaveHeader{chunk_id, chunk_size, format};
}

Chunk read_fmt_chunk(ifstream &f) {
    string chunk_id = next_n_bytes(f, 4, false);
    uint64_t chunk_size = next_n_bytes_sizet(f, 4, true);
    uint64_t format = next_n_bytes_sizet(f, 2);
    uint64_t num_channels = next_n_bytes_sizet(f, 2);
    uint64_t sample_rate = next_n_bytes_sizet(f, 4);
    uint64_t byte_rate = next_n_bytes_sizet(f, 4);
    uint64_t block_align = next_n_bytes_sizet(f, 2);
    uint64_t bits_per_sample = next_n_bytes_sizet(f, 2);
    return Chunk{chunk_id, chunk_size, format, num_channels, sample_rate, byte_rate, block_align, bits_per_sample};
}

//Skips forward to the data chunk, reading in
uint64_t ff_to_data(ifstream &f) {
    cout << "Stream pos at beginning of ff_to_data: " << f.tellg() << endl;
    string word = "data";
    char c[2];
    c[1] = '\0';
    int16_t i = 0;
    while (c[0] != 'd') {
        cout << i << endl;
        f.read(c, 1);
        i++;
        if (i > 1000) {
            cout << "FAILED TO FIND d" << endl;
            break;
        }
    }
    string s = next_n_bytes(f, 3);
    if (s == "ata") {
        uint32_t data_size;
        f.read(reinterpret_cast<char*>(&data_size), 4);
        return data_size;
    }
    return 0;
}

template<typename numT>
vector<Frame<numT>> read_data_chunk(ifstream &f, Chunk &fmt, uint64_t chunk_size) {
    uint64_t sr = fmt.sample_rate;
    uint64_t bits_per_sample = fmt.bits_per_sample;
    uint64_t bytes_per_sample = bits_per_sample / 8;
    uint64_t bytes_per_frame = bytes_per_sample * fmt.num_channels;
    uint64_t num_samples = chunk_size / bytes_per_sample; //This is NOT generally true, but it's true for the simplest case
                                                            //which is all I'm currently supporting

    uint64_t block_align = fmt.block_align;

    cout << "bytes_per_frame: " << bytes_per_frame << endl;
    cout << "block_align: " << block_align << endl;
    cout << "num_samples: " << num_samples << endl;
    cout << "chunk_size: " << chunk_size << endl;

    vector<Frame<numT>> samples;
    // for (int16_t i=0; i<num_samples; i++) {
    // while (!f.eof()) {
    // uint64_t i = fmt.chunk_size;
    uint64_t i = 0;
    while (i < chunk_size && !f.eof()) {
        char* buff = next_n_bytes(f,  bytes_per_frame);
        samples.emplace_back(Frame<numT>(fmt.num_channels, buff, bytes_per_sample)); 
        i += bytes_per_frame;
        // samples.push_back(static_cast<numT>(next_n_bytes_sizet(f, bytes_per_sample)));
    }
    return samples;
}


template <typename numT>
void write_channels_to_files(vector<Frame<numT>> &frames, string base_fname) {
    int num_channels = frames[0].num_channels;
    cout << "Writing samples to " << num_channels << " channels\n";
    for (int i=0; i<num_channels; ++i) {
        string fname = base_fname + to_string(i) + ".txt";
        cout << "Writing samples for channel: " << i << " to: " << fname << endl;
        ofstream f;
        f.open(fname);
        for (int idx=0; idx<frames.size(); ++idx) {
            f << frames[idx].channels[i] << endl;
        }
        f.close();
        cout << "Finished writing samples for channel " << i << " to " << fname << endl;
    }
}

uint64_t closest_pow2(uint64_t x) {
    uint64_t curr = 2;
    for (uint64_t i=1; i<50; ++i) {
        curr = pow(2, i);
        if (curr > x) {
            return curr;
        }
    }
    return curr;
}

template<typename numT>
vector<complex<double>> channel_to_complex(vector<Frame<numT>>& frames, int channel) {
    vector<complex<double>> lst;
    uint64_t curr_len = frames.size();
    uint64_t closest_pow = closest_pow2(curr_len);
    // lst.reserve(closest_pow);
    int i=0;
    double half_pow2_16 = pow(2.0, 16) / 2;
    for (; i<curr_len; ++i) {
        double val = static_cast<double>(frames[i].channels[channel]) / half_pow2_16;
        lst.push_back(complex<double>(val, 0.0));
    }

    //zero pad lst until we reach the closest power of 2 in length.  Necessary for fft
    while (i < closest_pow) {
        lst.push_back(complex<double>(0.0, 0.0));
        i += 1;
    }
    return lst;
}


int32_t main(int32_t argc, char** argv) {
    AudioStream stream(test_fname, 4096);
    vector<Frame<short>> frames = stream.read_next_chunk();

}


// int32_t main(int32_t argc, char** argv) {
//     char buff[] = "hello theres";
//     ifstream f(test_fname, ios_base::binary);
//     WaveHeader header = read_header(f);
//     Chunk fmt = read_fmt_chunk(f);
//     uint64_t bytes_per_sample = fmt.bits_per_sample / 8;
//     cout << "bytes_per_sample: " << bytes_per_sample << endl;
//
//     ofstream of1("outchannel1.txt");
//     ofstream of2("outchannel2.txt");
//
//
//
//     uint32_t data_size = ff_to_data(f);
//     vector<Frame<short>> frames = read_data_chunk<short>(f, fmt, header.chunk_size);
//     uint64_t closest_pow = closest_pow2(frames.size());
//
//     vector<complex<double>> channel1 = channel_to_complex<short>(frames, 0);
//     vector<complex<double>> fft_res = fft(channel1);
//
//
//
//     // write_channels_to_files(frames, "outchannel");
//
//
//     f.close();
//
//
//     for (auto val : fft_res) {
//         cout << val.real() << ", " << val.imag() << endl;
//     }
//
//
//
//
//
//
//     cout << "block_align: " << fmt.block_align << endl;
//     cout << "data_size: " << data_size << endl;
//     cout << "closest (lower) power of 2 to size: " << frames.size() << " is: " << closest_pow << endl;
//     cout << "frames.size(): " << frames.size() << endl;
//     // cout << "len
//     return 0;
// }





























