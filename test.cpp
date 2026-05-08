#include <iostream>
#include <string>
#include <stringtools.h>
#include <unordered_map>
// #include "frame.h"
// #include "barsdisplay.h"
#include "colorutils.h"
#include "parseargs.h"


// inline std::string reverse_str(std::string str) {
//     std::string rev_str(std::reverse_iterator(str.end()), std::reverse_iterator(str.begin()));
//     return rev_str;
// }

using namespace std;

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
inline string to_reversed_bin(numT num) {
    string bitstr;
    while (num >= 1) {
        int32_t digit = num % 2;
        bitstr += to_string(digit);
        num >>= 1;
    }
    return bitstr;
}



inline int32_t get_bitreversed_index(int32_t idx) {
    string rev_binstr = to_reversed_bin(idx);
    return from_bin<int32_t>(rev_binstr);
}


int main(int argc, char** argv) {

    ParsedArgs args(argc, argv);
    vector<string> short_flag_names = args.short_flag_names;
    vector<string> long_flag_names = args.long_flag_names;
    vector<string> plain_args = args.plain_args;
    unordered_map<string, string> short_flags = args.short_flags;
    unordered_map<string, string> long_flags = args.long_flags;

    cout << "Short flags:\n";
    for (auto kv : short_flags) {
        string name = kv.first;
        string val = kv.second;
        cout << name << ": " << val << endl;
    }
    cout << "long flags:\n";
    for (auto kv : long_flags) {
        string name = kv.first;
        string val = kv.second;
        cout << name << ": " << val << endl;
    }
    cout << "plain args:\n";
    for (string arg : plain_args) {
        cout << arg << endl;
    }

    return 0;

    vector<Color> gradient = create_gradient(Color{255, 0, 0}, Color{0, 0, 255}, 100);
    for (Color color : gradient) {
        cout << color << endl;
    }
    return 0;

    cout << "WHAT IS EVEN HAPPENING!?!?!?!?!" << endl;

    string s1 = "hello there";
    string s2 = "james taylor";

    int x = 6;
    string bsx = to_bin(x);
    int ridx = get_bitreversed_index(x);
    cout << x << endl << bsx << endl;
    cout << ridx << endl;
    return 0;


    const char *cstr1 = s1.c_str();
    const char *cstr2 = " james taylor";
    char *cstr = concat_cstr(cstr1,  cstr2);
    cout << strlen(cstr) << endl;
    cout << cstr << endl;
    // cout << subcstr(cstr, 3, 8) << endl;
    //

    vector<char*> segs = segmentcstr(cstr, 3, strlen(cstr));
    for (char* seg : segs) {
        cout << seg << endl;
    }


    cout << "SDLKJHGSKLDGJLSDGKJLSDGJLSDJDKLGGDS" << endl;
    // cout << segs.size() << endl;


    // std::string s = "hello there";
    // std::string r = reverse_str(s);
    // std::string ss = slicestr(s, 3, 12, 2);
    // cout << s << endl << r << endl;
    // cout << ss << endl;

    return 0;
}
