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
#include <numbers>

namespace fs = std::filesystem;
using namespace std;


// const string test_fname = "output.wav";
const string test_fname = "footstepswav.wav";
const uint64_t FAILED = string::npos;

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

void swap_positions(string &s, uint64_t x, uint64_t y) {
    char tmp = s[x];
    s[x] = s[y];
    s[y] = tmp;
}

void handle_little_endian(string &str) {
    for (int16_t i=0; i<str.size(); i+=2) {
        swap_positions(str, i, i+1);
    }
}

void reverse_buff_inplace(char* buff, uint64_t len) {
    for (uint64_t i=0; i<len/2; ++i) {
        uint64_t j = len - i - 1;
        char tmp = buff[i];
        buff[i] = buff[j];
        buff[j] = tmp;
    }
}

char* next_n_bytes(ifstream &f, uint64_t n, bool little_endian=true) {
    //we never read more than 16 bytes
    char *buff = new char[n+1];
    f.read(buff, n);
    buff[n] = '\0';
    // if (little_endian) {
    //     cout << "before reversal, buff: " << buff << endl;
    //     reverse_buff_inplace(buff, n);
    //     cout << "after reversal, buff: " << buff << endl;
    // }
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

int16_t four_bytes_to_int(char* buff, bool little_endian=true) {
    // cout << "Length of buff: " << buff.size() << endl;
    if (little_endian) {
        int16_t a = int((unsigned char)(buff[3]) << 24 |
                (unsigned char)(buff[2]) << 16 |
                (unsigned char)(buff[1]) << 8  |
                (unsigned char)(buff[0]));
        return a;
    }
    else {
        int16_t a = int((unsigned char)(buff[0]) << 24 |
                (unsigned char)(buff[1]) << 16 |
                (unsigned char)(buff[2]) << 8  |
                (unsigned char)(buff[3]));
        return a;
    }
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


void ff_to_data(ifstream &f) {
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
        return;
    }
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
    uint64_t i = fmt.chunk_size;
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



int32_t main(int32_t argc, char** argv) {
    char buff[] = "hello theres";
    // reverse_buff_inplace(buff, 12);
    // cout << buff << endl;
    // return 0;
    // string teststr = "52314";
    // string revstr = reverse_str(teststr);
    // cout << teststr << endl << revstr << endl;
    // return 0;
    // uint64_t zero = 0;
    // uint64_t num = stoull(teststr, &zero);
    // cout << teststr << endl << num << endl;
    // return 0;
    ifstream f(test_fname, ios_base::binary);
    WaveHeader header = read_header(f);
    Chunk fmt = read_fmt_chunk(f);
    uint64_t bytes_per_sample = fmt.bits_per_sample / 8;
    cout << "bytes_per_sample: " << bytes_per_sample << endl;

    ofstream of1("outchannel1.txt");
    ofstream of2("outchannel2.txt");



    // if (bytes_per_sample == 4) {
    //
    // }

    ff_to_data(f);
    vector<Frame<short>> frames = read_data_chunk<short>(f, fmt, header.chunk_size);
    // for (uint64_t i=0; i<frames.size(); ++i) {
    //     of1 << frames[i].channels[0] << endl;
    //     of2 << frames[i].channels[1] << endl;
    // }

    write_channels_to_files(frames, "outchannel");
    // return 0;

    // vector<complex<double>> complex_samples;
    // complex_samples.reserve(samples.size());
    // for (int i=0; i<samples.size(); ++i) {
    //     complex_samples[i] = complex<double>(static_cast<double>(samples[i].channels[0]), 0.0);
    // }
    //
    //
    //
    // vector<complex<double>> freqs = fft(complex_samples);
    // cout << "freqs.size(): " << freqs.size() << endl;


    // cout << "chunk_id: " << fmt.chunk_id << endl << "chunk_size: " << fmt.chunk_size << endl << fmt.format << endl << fmt.num_channels << endl;



    // for (int16_t i=0; i<10; i++) {
    //     cout << next_n_bytes(f, 16) << endl;
    // }

    // string riff_hopefully = next_n_bytes(f, 4, false);
    // // string chunk_size = next_n_bytes_size(f, 4, false);
    // uint64_t chunk_size = next_n_bytes_sizet(f, 4, true);
    // // char *buff2 = next_n_bytes(f, 4, false);
    // // int16_t chunk_size = four_bytes_to_int(buff2);
    // cout << riff_hopefully << endl;
    // cout << "chunk_size: " << chunk_size << endl;


    f.close();

    cout << "block_align: " << fmt.block_align << endl;
    return 0;
}































