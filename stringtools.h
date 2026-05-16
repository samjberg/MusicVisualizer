#ifndef STRINGTOOLS_H
#define STRINGTOOLS_H

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <string.h>


inline std::vector<std::string> splitstr(std::string str, char delim='\n') {
    int i = 0;
    int prev_i = 0;
    int l = str.size();
    size_t length_count = 0;
    std::vector<std::string> results;
    while (i < l) {
        i = str.find(delim, prev_i);
        if (i == std::string::npos || i <= prev_i) {
          break;
        }

        results.emplace_back(str.substr(prev_i, i - prev_i + 1));
        length_count += i-prev_i;
        prev_i = i + 1;
    }
    if (length_count < str.size()) {
        results.emplace_back(str.substr(prev_i));
    }
    return results;
}

inline std::vector<std::string> splitstr(std::string str, std::string delim) {
    int i = 0;
    int prev_i = 0;
    int l = str.size();
    size_t length_count = 0;
    std::vector<std::string> results;
    while (i < l) {
        i = str.find(delim, prev_i);
        if (i == std::string::npos || i <= prev_i) {
          break;
        }
        results.emplace_back(str.substr(prev_i, i - prev_i + 1));
        prev_i = i + 1;
    }
    if (length_count < str.size()) {
        results.emplace_back(str.substr(prev_i));
    }
    return results;
}

inline std::string joinstr(std::vector<std::string> lst, std::string delim="\n") {
    std::string str = lst.size() > 0 ? lst[0] : "";
    for (int i=1; i<lst.size(); ++i) {
        str += delim + lst[i];
    }
    return str;
}

inline std::string joinstr(std::vector<std::string> lst, char delim='\n') {
    std::string str = lst.size() > 0 ? lst[0] : "";
    for (int i=1; i<lst.size(); ++i) {
        str += delim + lst[i];
    }
    return str;
}

inline std::string reverse_str(std::string str) {
    return std::string(std::reverse_iterator(str.end()), std::reverse_iterator(str.begin()));
}


inline std::string slicestr(std::string str, size_t start, size_t stop=0, size_t step=1) {
    if (stop == 0)
        stop = str.size();

    if (step == 1) 
        return std::string(str.begin() + start, str.begin() + stop);

    std::string s;
    for (int i=start; i<stop; i+=step) {
        s += str[i];
    }
    return s;
}


inline std::vector<std::string> segmentstr(std::string str, size_t num_segments) {
    size_t len = str.size();
    size_t stride = len / num_segments;
    std::vector<std::string> segmented_str;
    for (int i=0; i<len; i+=stride) {
        int rem_len = len - i;
        if (rem_len < stride) {
            if ((static_cast<float>(stride-rem_len)) < (static_cast<float>(stride)/2.0)) {
                segmented_str.emplace_back(str.substr(i, rem_len));
            }
            else{
                segmented_str.back() += str.substr(i, rem_len);
            }
            break;
        }
        size_t substr_len = std::min<size_t>(stride, rem_len);
        segmented_str.emplace_back(str.substr(i, substr_len));
    }
    return segmented_str;
}

inline char* subcstr(char* cstr, uint64_t start, uint64_t stop) {
    char* substr = new char[stop - start + 1];
    for (int i=start; i<stop; ++i) {
        substr[i-start] = cstr[i];
    }
    substr[start-stop] = '\0';
    return substr;

}

//Note, cstr1 and cstr2 MUST be null terminated
inline char* concat_cstr(const char* cstr1, const char* cstr2) {
    uint64_t l1 = strlen(cstr1);
    uint64_t l2 = strlen(cstr2);
    char* combined = new char[l1 + l2 + 1];
    memcpy(combined, cstr1, l1);
    memcpy(combined+l1, cstr2, l2);
    combined[l1+l2] = '\0';
    return combined;
}

inline std::vector<char*> segmentcstr(char* str, uint64_t num_segments, uint64_t len) {
    std::vector<char*> segmented_str;
    uint64_t seg_len = len / num_segments;
    for (int i=0; i<len; i+=seg_len) {
        char* buff = new char[seg_len];
        memcpy(buff, str+i, seg_len);
        segmented_str.push_back(buff);
    }
    return segmented_str;
}

// inline std::vector<char*> segmentstr(char* str, uint64_t num_segments, uint64_t len) {
//     uint64_t stride = len / num_segments;
//     std::vector<char*> segmented_str;
//     for (int i=0; i<len; i+=stride) {
//         int32_t rem_len = len - i;
//         if (rem_len < stride) {
//             if ((static_cast<float>(stride-rem_len)) < (static_cast<float>(stride)/2.0)) {
//                 segmented_str.emplace_back(subcstr(str, i, rem_len));
//             }
//             else {
//                 segmented_str.back() = concat_cstr(segmented_str.back(), subcstr(str, i, rem_len));
//
//         }
//
//     }
// }

// template <typename _numT>
// std::string bitstr(_numT num, size_t bits=0) {
//     if (bits == 0) {
//         bits = static_cast<size_t>(std::log2(num)) + 1;
//         std::cout << "bits: " << bits  << std::endl;
//     }
//     std::vector<char> stk;
//     while (num > 0) {
//         div_t divres = div(num, 2);
//         num = divres.quot;
//         stk.emplace_back(std::to_string(divres.rem)[0]);
//
//     }
//     std::string s;
//     size_t len = stk.size();
//     for (int i=0; i<len; ++i) {
//         s += stk.back();
//         stk.pop_back();
//     }
//     size_t bits_remaining = bits - s.size();
//     std::string prefix(bits - s.size(), '0');
//     return prefix + s;
// }
//
// template <typename _numT>
// std::string bitstr(std::complex<_numT> num, std::complex<_numT> bits=std::complex<_numT>(0, 0)) {
//     std::string rstr = bitstr<_numT>(num.real());
//     std::string istr = bitstr<_numT>(num.imag());
//     return rstr + istr;
// }
//
// template <typename _numT>
// _numT bitrev(_numT num, size_t bits=0) {
//     if (bits == 0) {
//         bits = static_cast<size_t>(std::log2(num)) + 1;
//     }
//     _numT rev = 0;
//     for (int i=0; i<bits; ++i) {
//         rev = (rev << 1) | (num & 1);
//         num >>= 1;
//     }
//     return rev;
// }
//
// template <typename _numT>
// std::complex<_numT> bitrev(std::complex<_numT> num, std::complex<_numT> bits=std::complex<_numT>(0, 0)) {
//     _numT real = bitrev<_numT>(num.real());
//     _numT imag = bitrev<_numT>(num.imag());
//     return std::complex<_numT>(real, imag);
// }
//

// inline std::vector<std::string> parseargs(int argc, char** argv) {
//
//
// }




#endif
