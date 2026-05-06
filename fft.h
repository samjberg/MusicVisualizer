#include <cstdint>
#include <iostream>
// #include <numbers>
#include <string>
#include <bitset>
#include <vector>
#include <complex>
#include <cmath>

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
    uint64_t *x = (size_t*)&num;
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
    uint64_t *x = (size_t*)&num;
    *x |= with;
    numT *anded_num = (numT*)x;
    return *anded_num;
    // return x & with;
}


template<typename numT>
inline numT mybit_and(double num, uint64_t with) {
    uint64_t *x = (size_t*)&num;
    *x &= with;
    numT *anded_num = (numT*)x;
    return *anded_num;
    // return x & with;
}


template <typename numT>
inline numT reverse_bits(numT x, int32_t bits = 32) {
    numT rev = 0;
    for (int32_t i = 0; i < bits; i++) {
        rev = (rev << 1) | (x & 1);
        x >>= 1;
    }
    return rev;
}


inline double reverse_bits(double x, int32_t bits = 32) {
    uint64_t *x_sizet_p =  (size_t*)&x;
    uint64_t reversed_sizet = reverse_bits(*x_sizet_p);
    return static_cast<double>(reversed_sizet);

}


template <typename numT>
inline complex<numT> reverse_bits(complex<numT>& x, int32_t bits = 32) {
    numT real = x.real();
    numT imag = x.imag();

    numT real_rev = reverse_bits(real, bits);
    numT imag_rev = reverse_bits(imag, bits);
    return complex<numT>(real_rev, imag_rev);
}


template<typename numT>
inline vector<numT> reverse_bits(vector<numT>& lst, int32_t bits = 32) {
    vector<numT> reversed_lst(lst.size());
    for (int32_t i=0; i<lst.size(); ++i) {
        reversed_lst[i] = reverse_bits(lst[i], bits);
    }
    return reversed_lst;
}


template<typename numT>
inline numT get_bitreversed_index(numT idx) {
    //CAUTION.  numT here must be a normal integral numerical type (i.e. a type of int).  DO NOT use complex for numT for this function
    string rev_binstr = to_reversed_bin(idx);
    return from_bin<numT>(rev_binstr);
}

inline vector<complex<double>> do_bit_reversal(vector<complex<double>>& lst) {
    vector<complex<double>> old_values(lst);
    vector<int32_t> new_indices;
    // old_values.reserve(lst.size());
    new_indices.reserve(lst.size());
    for (int i=0; i<lst.size(); i++) {
        int32_t new_i = get_bitreversed_index<int32_t>(i);
        new_indices[i] = new_i;
        // old_values[i] = lst[i];
    }
    for (int i=0; i<lst.size(); i++) {
        lst[new_indices[i]] = old_values[i]; 
        lst[i] = old_values[new_indices[i]];
    }
    return lst;
}




//This function ONLY performs the actual cooley-turkey fft algorithm, it does NOTHING else
//It does not create any bins, it just produces the raw output.
inline vector<complex<double>> fft(vector<complex<double>>& a) {
    auto n = static_cast<double>(a.size());
    // vector<complex<double>> A(ni);
    // for (int32_t i=0; i<ni; ++i) {
    //     int32_t j = reverse_bits<int32_t>(i, log_n); 
    //     A[j] = a[i];
    // }

    vector<complex<double>> A = do_bit_reversal(a);
    for (int i=0; i<10; ++i) {
        cout << "n: " << n << endl;
    }
    for (uint64_t s=1; s < log2(n); ++s) {
        // cout << "s: " << s << endl;
        double m = pow(2, s);
        // pi;
        complex<double> wm = exp((-2.0 * pi * I)/double(m));
        for (uint64_t k=0; k<n-1; k+=size_t(m)) {
            complex<double> w(1.0, 0.0);
            for (uint64_t j = 0; j<m/2; ++j) {
                uint64_t idx1 = k+j+size_t(m)/2;
                uint64_t idx2 = k+j;
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
    return sqrt(pow(c.real(), 2.0) + pow(c.imag(), 2.0));
}

inline double calculate_decibels(complex<double> c) {
    return 20 * log10(calculate_power(c));
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

