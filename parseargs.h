#ifndef PARSEARGS_H
#define PARSEARGS_H

#include <iostream>
#include <string>
#include <type_traits>
#include <vector>
#include <cstdint>
#include <unordered_map>

enum Types {
    int_type,
    float_type,
    string_type
};

struct Flag {
    std::string name;
    bool is_long = false;
};

inline std::ostream& operator<<(std::ostream& out, Flag f) {
    std::string prefix = f.is_long ? "--" : "-";
    std::string fullname = prefix + f.name;
    out << fullname;
    return out;
}

inline bool is_alpha(uint8_t c) {
    return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
}

inline bool is_alpha(std::string s) {
    for (char c : s) {
        if (!is_alpha(c))
            return false;
    }
    return true;
}



inline bool is_flag(std::string s) {
    if (s == "--") return false; //return false if s is the separator between flags and plain args
    return s.starts_with('-');
}

inline bool is_long_flag(std::string s) {
    if (is_flag(s)) return s[1] == '-';
    return false;
}

inline bool is_short_flag(std::string s) {
    return is_flag(s) && !is_long_flag(s);
}

// inline bool 

struct Value {
    std::string val;

    template <typename T>
    T as() const {
        if constexpr (std::is_same_v<T, int>) {
            return std::stoi(val);
        }
        else if constexpr(std::is_same_v<T, float>) {
            return std::stof(val);

        }
        else if constexpr(std::is_same_v<T, std::string>) {
            return val;
        }
        else {
            static_assert(sizeof(T) == 0, "Unsupported type for Value conversion");
        }

    }
};


inline void testfunc(int argc, char** argv) {
    Flag flg{"flagname"};
    std::vector<Value> values;
    for (int i=0; i<argc; ++i) {
        values.emplace_back(Value{argv[i]});
    }
}

template <typename T>
bool contains(std::vector<T> vec, T val) {
    for (T x : vec) {
        if (val == x) {
            return true;
        }
    }
    return false;
}

struct ParsedArgs {
    std::vector<std::string> short_flag_names;
    std::vector<std::string> long_flag_names;
    std::unordered_map<std::string, std::string> short_flags;
    std::unordered_map<std::string, std::string> long_flags;
    std::vector<std::string> plain_args;

    ParsedArgs(int32_t argc, char** argv) {
        int32_t last_flag_idx = 0;
        bool found_sep = false;
        for (int i=0; i<argc; ++i) {
            std::string s = argv[i];
            if (is_flag(s)) {
                std::string flag_val = i<(argc-1) ? argv[i+1] : "";
                if (is_long_flag(s)) {
                    std::string flag_name = s.substr(2);
                    long_flag_names.push_back(flag_name);
                    long_flags[flag_name] = flag_val;
                }
                else {
                    std::string flag_name = s.substr(1);
                    short_flag_names.push_back(flag_name);
                    short_flags[flag_name] = flag_val;
                }
                if (!found_sep) {
                    last_flag_idx = i;
                }
            }
            //if s is the separator
            else if (s == "--") {
                last_flag_idx = i;
                found_sep = true;
            }
        }

        std::cout << "Last flag index: " << last_flag_idx << std::endl;

        for (int i = last_flag_idx + 1; i < argc; ++i) {
            plain_args.push_back(argv[i]);
        }
    }
};


#endif
