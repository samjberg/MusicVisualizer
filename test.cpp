#include <iostream>
#include <string>
#include <string.h>
#include <stringtools.h>
#include "frame.h"



// inline std::string reverse_str(std::string str) {
//     std::string rev_str(std::reverse_iterator(str.end()), std::reverse_iterator(str.begin()));
//     return rev_str;
// }

using namespace std;


int main(int argc, char** argv) {
    cout << "WHAT IS EVEN HAPPENING!?!?!?!?!" << endl;

    string s1 = "hello there";
    string s2 = "james taylor";


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
