#include <cstdlib>
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
    size_t len = binstr.size();
    numT result = 0;
    for (int i=len-1; i>=0; --i) {
        int idx = len - i - 1;
        if (binstr[i] == '1') {
            result += pow(2, idx);
        }
    }
    return result;
}

// template<typename numT>
// inline string to_bin(numT num) {
//
// }

template<typename numT>
inline numT bitshift(numT num, int shift = -1) {
    size_t *x = (size_t*)&num;
    if (shift < 0) {
        *x <<= (-shift);
    }
    else if (shift > 0) {
        *x >>= shift;
    }
    return (*(numT*)x);
}

template<typename numT>
inline numT mybit_or(double num, size_t with) {
    size_t *x = (size_t*)&num;
    *x |= with;
    numT *anded_num = (numT*)x;
    return *anded_num;
    // return x & with;
}


template<typename numT>
inline numT mybit_and(double num, size_t with) {
    size_t *x = (size_t*)&num;
    *x &= with;
    numT *anded_num = (numT*)x;
    return *anded_num;
    // return x & with;
}


template <typename numT>
inline numT reverse_bits(numT x, int bits = 32) {
    numT rev = 0;
    for (int i = 0; i < bits; i++) {
        rev = (rev << 1) | (x & 1);
        x >>= 1;
    }
    return rev;
}


inline double reverse_bits(double x, int bits = 32) {
    size_t *x_sizet_p =  (size_t*)&x;
    size_t reversed_sizet = reverse_bits(*x_sizet_p);
    return static_cast<double>(reversed_sizet);

}


template <typename numT>
inline complex<numT> reverse_bits(complex<numT> x, int bits = 32) {
    numT real = x.real();
    numT imag = x.imag();

    numT real_rev = reverse_bits(real, bits);
    numT imag_rev = reverse_bits(imag, bits);
    return complex<numT>(real_rev, imag_rev);
}


template<typename numT>
inline vector<numT> reverse_bits(vector<numT> lst, int bits = 32) {
    vector<numT> reversed_lst(lst.size());
    for (int i=0; i<lst.size(); ++i) {
        reversed_lst[i] = reverse_bits(lst[i], bits);
    }
    return reversed_lst;
}




inline vector<complex<double>> fft(vector<complex<double>> a) {
    vector<complex<double>> A = reverse_bits(a);
    auto n = static_cast<double>(a.size());
    for (size_t s=1; s < log2(n); ++s) {
        size_t m = pow(2, s);
        // pi;
        complex<double> wm = exp((-2.0 * pi * I)/double(m));
        for (size_t k=0; k<n-1; k+=m) {
            complex<double> w(1.0, 0.0);
            for (size_t j = 0; j<m/2; ++j) {
                size_t idx1 = k+j+m/2;
                size_t idx2 = k+j;
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

// template<typename numT>
// vector<numT> generate_random_vector(size_t len, numT mn, numT mx) {
//     vector<numT> lst(len);
//     for (int i=0; i<len; ++i) {
//         lst[i] = random(mn, mx);
//     }
//     return lst;
// }

inline vector<complex<double>> generate_random_vector(size_t len, double mn, double mx) {
    vector<complex<double>> lst(len);
    for (int i=0; i<len; ++i) {
        // double r = random(mn, mx);
        // double im = random(mn, mx);
        lst[i] = complex<double>(random(mn, mx), random(mn, mx));
    }
    return lst;
}

// int main(int argc, char** argv) {
//     size_t n = pow(2, 32);
//     vector<complex<double>> lst = generate_random_vector(pow(2, 20), 0.0, n);
//     // for (int i=0; i<100; ++i) {
//     //     cout << lst[i] << endl;
//     // }
//
//     vector<complex<double>> res = fft(lst);
//     for (int i=0; i<res.size(); i++) {
//         cout << res[i] << " ";
//     }
//     // for (int 
//
//     // Range r{1, 2}
//     return 0;
//
//     double a = 5.0;
//     bitset<64> bs_a(a);
//     // cout << bs_a << endl;
//     // bitshift(&a, 1);
//     size_t *b = (size_t*)&a;
//     size_t c = (size_t)a;
//     // cout << *b << endl;
//     // cout << c << endl;
//
//     // return 0;
//     // *b >>= 1;
//     // b = (size_t*)&a;
//     // cout << a << " " << *b << endl;
//     // mybit_or<double>(&a, 8523413);
//     // cout << a << " " << *b << endl;
//
//     string binstr = argv[1];
//     size_t val = from_bin<size_t>(binstr);
//     // cout << val << endl;
//
//     // return 0;
//
//
//     size_t num1 = 523;
//     size_t rnum1 = reverse_bits(num1);
//
//     complex<double> c1(52.232353232532211533259259324, 2332553295093.9325932582355126);
//     cout << "c1: " << c1 << endl;
//
//     complex<double> rc1 = reverse_bits(c1);
//     cout << "c1: " << c1 << endl << "rc1: " << rc1 << endl;
//
//
//     return 0;
//
// }
