#include <iostream>
#include <string>
// #include <vector>
#include <fstream>
#include <filesystem>
#include <stringtools.h>
#include <bitset>
#include <utility>

namespace fs = std::filesystem;
using namespace std;


const string test_fname = "output.wav";
const size_t FAILED = string::npos;

struct WaveHeader {
    string chunk_id; //Big
    size_t chunk_size; //Little
    string format; //Big
};



struct Chunk {
    string chunk_id; //Big
    size_t chunk_size; //Little
    size_t format; //Little
    size_t num_channels; //Little
    size_t sample_rate; //Little
    size_t byte_rate; //Little
    size_t block_align; //Little
    size_t bits_per_sample; //Little
    size_t extra_param_size;
    size_t extra_params; //I'm not sure if this is necessary, and also it may NEED to be removed
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

void swap_positions(string &s, size_t x, size_t y) {
    char tmp = s[x];
    s[x] = s[y];
    s[y] = tmp;
}

void handle_little_endian(string &str) {
    for (int i=0; i<str.size(); i+=2) {
        swap_positions(str, i, i+1);
    }
}

void reverse_buff_inplace(char* buff, size_t len) {
    for (size_t i=0; i<len/2; ++i) {
        size_t j = len - i - 1;
        char tmp = buff[i];
        buff[i] = buff[j];
        buff[j] = tmp;
    }
}

char* next_n_bytes(ifstream &f, size_t n, bool little_endian=true) {
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

int n_bytes_to_int(const char* buff, int n, bool little_endian=true) {
    int a = 0;
    for (int i=0; i<n; i++) {
        int j = n - i - 1;
        int shift_amount = 8 * j;
        a |= int((unsigned char)(little_endian ? buff[j] : buff[i]) << shift_amount);
    }
    return a;
}

int four_bytes_to_int(char* buff, bool little_endian=true) {
    // cout << "Length of buff: " << buff.size() << endl;
    if (little_endian) {
        int a = int((unsigned char)(buff[3]) << 24 |
                (unsigned char)(buff[2]) << 16 |
                (unsigned char)(buff[1]) << 8  |
                (unsigned char)(buff[0]));
        return a;
    }
    else {
        int a = int((unsigned char)(buff[0]) << 24 |
                (unsigned char)(buff[1]) << 16 |
                (unsigned char)(buff[2]) << 8  |
                (unsigned char)(buff[3]));
        return a;
    }
}

size_t next_n_bytes_sizet(ifstream &f, size_t n, bool little_endian=true) {
    // cout << "converting " << n << " bytes to size_t" << endl;
    char* s = next_n_bytes(f, n);
    return n_bytes_to_int(s, n, little_endian);
}

// long next_n_bytes_sizet(ifstream &f, size_t n, bool little_endian=true) {
//     cout << "converting " << n << " bytes to size_t" << endl;
//     char* s = next_n_bytes(f, n);
//     return long(n_bytes_to_int(s, n, little_endian));
// }



WaveHeader read_header(ifstream &f) {
    //determine the size of the file
    char buff[8];

    //Read the initial "RIFF" bytes
    string chunk_id = next_n_bytes(f, 4, false);
    cout << "chunk_id: " << chunk_id << endl;
    size_t chunk_size = next_n_bytes_sizet(f, 4);
    cout << "chunk_size: " << chunk_size << endl;
    string format = next_n_bytes(f, 4, false);
    cout << "format: " << format << endl;
    // string format = next_n_bytes(f, 
    // f.close();
    return WaveHeader{chunk_id, chunk_size, format};
}

Chunk read_fmt_chunk(ifstream &f) {
    string chunk_id = next_n_bytes(f, 4, false);
    size_t chunk_size = next_n_bytes_sizet(f, 4, true);
    size_t format = next_n_bytes_sizet(f, 2);
    size_t num_channels = next_n_bytes_sizet(f, 2);
    size_t sample_rate = next_n_bytes_sizet(f, 4);
    size_t byte_rate = next_n_bytes_sizet(f, 4);
    size_t block_align = next_n_bytes_sizet(f, 2);
    size_t bits_per_sample = next_n_bytes_sizet(f, 2);
    return Chunk{chunk_id, chunk_size, format, num_channels, sample_rate, byte_rate, block_align, bits_per_sample};
}


void ff_to_data(ifstream &f) {
    cout << "Stream pos at beginning of ff_to_data: " << f.tellg() << endl;
    string word = "data";
    char c[2];
    c[1] = '\0';
    int i = 0;
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
vector<numT> read_data_chunk(ifstream &f, Chunk &fmt) {
    size_t sr = fmt.sample_rate;
    size_t bits_per_sample = fmt.bits_per_sample;
    size_t bytes_per_sample = bits_per_sample / 8;
    size_t num_samples = fmt.chunk_size / bytes_per_sample; //This is NOT generally true, but it's true for the simplest case
                                                            //which is all I'm currently supporting

    vector<numT> samples;
    // for (int i=0; i<num_samples; i++) {
    while (!f.eof()) {
        samples.push_back(static_cast<numT>(next_n_bytes_sizet(f, bytes_per_sample)));
    }
    return samples;
}




int main(int argc, char** argv) {
    char buff[] = "hello theres";
    // reverse_buff_inplace(buff, 12);
    // cout << buff << endl;
    // return 0;
    // string teststr = "52314";
    // string revstr = reverse_str(teststr);
    // cout << teststr << endl << revstr << endl;
    // return 0;
    // size_t zero = 0;
    // size_t num = stoull(teststr, &zero);
    // cout << teststr << endl << num << endl;
    // return 0;
    ifstream f(test_fname, ios_base::binary);
    WaveHeader header = read_header(f);
    Chunk fmt = read_fmt_chunk(f);
    size_t bytes_per_sample = fmt.bits_per_sample / 8;
    cout << "bytes_per_sample: " << bytes_per_sample << endl;
    // if (bytes_per_sample == 4) {
    //
    // }
    ff_to_data(f);
    auto samples = read_data_chunk<short>(f, fmt);
    for (int i=0; i<samples.size(); ++i) {
        cout << samples[i] << endl;
    }


    // cout << "chunk_id: " << fmt.chunk_id << endl << "chunk_size: " << fmt.chunk_size << endl << fmt.format << endl << fmt.num_channels << endl;



    // for (int i=0; i<10; i++) {
    //     cout << next_n_bytes(f, 16) << endl;
    // }

    // string riff_hopefully = next_n_bytes(f, 4, false);
    // // string chunk_size = next_n_bytes_size(f, 4, false);
    // size_t chunk_size = next_n_bytes_sizet(f, 4, true);
    // // char *buff2 = next_n_bytes(f, 4, false);
    // // int chunk_size = four_bytes_to_int(buff2);
    // cout << riff_hopefully << endl;
    // cout << "chunk_size: " << chunk_size << endl;


    f.close();
    return 0;
}































