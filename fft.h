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
const std::complex<double> pi_c(pi, 0);
const std::complex<double> I(0, 1);




//Name changed to randomdouble for compatibility reasons
inline double randomdouble() {
    return double(rand()) / RAND_MAX;
}

//Name changed to randomdouble for compatility reasons
inline double randomdouble(double mn, double mx) {
    double range = mx - mn;
    double r = randomdouble();
    return mn + (r * range);
}





template<typename numT>
inline numT get_bitreversed_index(numT idx, uint64_t bit_count) {
    //CAUTION.  numT here must be a normal integral numerical type (i.e. a type of int).  DO NOT use std::complex for numT for this function
    numT result = 0;
    for (uint64_t i=0; i<bit_count; ++i) {
        result <<= 1;
        result |= (idx & 1);
        idx >>= 1;
    }
    return result;
}

inline std::vector<std::complex<double>> do_bit_reversal(std::vector<std::complex<double>>& lst) {
    uint64_t n = lst.size();
    uint64_t bit_count = static_cast<uint64_t>(log2(n));
    std::vector<std::complex<double>> res(n);
    for (uint64_t i=0; i<n; i++) {
        uint64_t new_i = get_bitreversed_index<uint64_t>(i, bit_count);
        res[new_i] = lst[i];
    }
    return res;
}




//This function ONLY performs the actual cooley-turkey fft algorithm, it does NOTHING else
//It does not create any bins, it just produces the raw output.
inline std::vector<std::complex<double>> fft(std::vector<std::complex<double>>& a) {
    uint64_t n = a.size();
    std::vector<std::complex<double>> A = do_bit_reversal(a);

    for (uint64_t m=2; m<=n; m*=2) {
        std::complex<double> wm = exp((-2.0 * pi * I)/static_cast<double>(m));
        for (uint64_t k=0; k<n; k+=m) {
            std::complex<double> w(1.0, 0.0);
            for (uint64_t j=0; j<m/2; ++j) {
                uint64_t idx1 = k + j + (m / 2);
                uint64_t idx2 = k + j;
                std::complex<double> t = w * A[idx1];
                std::complex<double> u = A[idx2];
                A[idx2] = u + t;
                A[idx1] = u - t;
                w = w * wm;
            }
        }
    }
    return A;
}

inline double calculate_power(std::complex<double> c) {
    return pow(c.real(), 2.0) + pow(c.imag(), 2.0);
}

inline double calculate_decibels(std::complex<double> c) {
    return 10 * log10(calculate_power(c));
}


inline double calculate_db_from_power(double power) {
    return 10 * log10(power);
}



inline std::vector<std::complex<double>> generate_random_vector(uint64_t len, double mn, double mx) {
    std::vector<std::complex<double>> lst(len);
    for (int32_t i=0; i<len; ++i) {
        // double r = random(mn, mx);
        // double im = random(mn, mx);
        lst[i] = std::complex<double>(randomdouble(mn, mx), randomdouble(mn, mx));
    }
    return lst;
}


#endif

