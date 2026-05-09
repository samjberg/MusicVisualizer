#ifndef FFT_H
#define FFT_H
#include <cstdint>
#include <iostream>
// #include <numbers>
#include <string>
#include <bitset>
#include <vector>
#include <complex>
#include <cmath>
#include <numbers>

const double pi = std::numbers::pi;
using namespace std;
const complex<double> pi_c(pi, 0);
const complex<double> I(0, 1);





inline double random() {
    return double(rand()) / RAND_MAX;
}

inline double random(double mn, double mx) {
    double range = mx - mn;
    double r = random();
    return mn + (r * range);
}


template<typename numT>
inline numT from_bin(string binstr) {
    auto it = binstr.end() - 1;
    uint64_t len = binstr.size();
    numT result = 0;
    for (int32_t i=len-1; i>=0; --i) {
        int32_t idx = len - i - 1;
        if (binstr[i] == '1') {
            result += pow(2, idx);
        }
    }
    return result;
}

template<typename numT>
inline string to_reversed_bin(numT num) {
    string bitstr;
    while (num >= 1) {
        int32_t digit = num % 2;
        bitstr += to_string(digit);
        num >>= 1;
    }
    return bitstr;
}

template<typename numT>
inline string to_bin(numT num) {
    string bitstr;
    vector<char> stack;
    while (num >= 1) {
        int32_t digit = num % 2;
        stack.push_back(to_string(digit)[0]);
        num >>= 1;
    }
    while (stack.size() > 0) {
        bitstr += stack.back();
        stack.pop_back();
    }
    return bitstr;
}


template<typename numT>
inline numT bitshift(numT num, int32_t shift = -1) {
    uint64_t *x = (uint64_t*)&num;
    if (shift < 0) {
        *x <<= (-shift);
    }
    else if (shift > 0) {
        *x >>= shift;
    }
    return (*(numT*)x);
}

template<typename numT>
inline numT mybit_or(double num, uint64_t with) {
    uint64_t *x = (uint64_t*)&num;
    *x |= with;
    numT *anded_num = (numT*)x;
    return *anded_num;
    // return x & with;
}


template<typename numT>
inline numT mybit_and(double num, uint64_t with) {
    uint64_t *x = (uint64_t*)&num;
    *x &= with;
    numT *anded_num = (numT*)x;
    return *anded_num;
    // return x & with;
}



template<typename numT>
inline numT get_bitreversed_index(numT idx, uint64_t bit_count) {
    //CAUTION.  numT here must be a normal integral numerical type (i.e. a type of int).  DO NOT use complex for numT for this function
    numT result = 0;
    for (uint64_t i=0; i<bit_count; ++i) {
        result <<= 1;
        result |= (idx & 1);
        idx >>= 1;
    }
    return result;
}

inline vector<complex<double>> do_bit_reversal(vector<complex<double>>& lst) {
    uint64_t n = lst.size();
    uint64_t bit_count = static_cast<uint64_t>(log2(n));
    vector<complex<double>> res(n);
    for (uint64_t i=0; i<n; i++) {
        uint64_t new_i = get_bitreversed_index<uint64_t>(i, bit_count);
        res[new_i] = lst[i];
    }
    return res;
}




//This function ONLY performs the actual cooley-turkey fft algorithm, it does NOTHING else
//It does not create any bins, it just produces the raw output.
inline vector<complex<double>> fft(vector<complex<double>>& a) {
    uint64_t n = a.size();
    vector<complex<double>> A = do_bit_reversal(a);

    for (uint64_t m=2; m<=n; m*=2) {
        complex<double> wm = exp((-2.0 * pi * I)/static_cast<double>(m));
        for (uint64_t k=0; k<n; k+=m) {
            complex<double> w(1.0, 0.0);
            for (uint64_t j=0; j<m/2; ++j) {
                uint64_t idx1 = k + j + (m / 2);
                uint64_t idx2 = k + j;
                complex<double> t = w * A[idx1];
                complex<double> u = A[idx2];
                A[idx2] = u + t;
                A[idx1] = u - t;
                w = w * wm;
            }
        }
    }
    return A;
}

inline double calculate_power(complex<double> c) {
    return pow(c.real(), 2.0) + pow(c.imag(), 2.0);
}

inline double calculate_decibels(complex<double> c) {
    return 10 * log10(calculate_power(c));
}



inline vector<complex<double>> generate_random_vector(uint64_t len, double mn, double mx) {
    vector<complex<double>> lst(len);
    for (int32_t i=0; i<len; ++i) {
        // double r = random(mn, mx);
        // double im = random(mn, mx);
        lst[i] = complex<double>(random(mn, mx), random(mn, mx));
    }
    return lst;
}


#endif

