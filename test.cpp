#include <iostream>
#include <string>
#include <stringtools.h>



// inline std::string reverse_str(std::string str) {
//     std::string rev_str(std::reverse_iterator(str.end()), std::reverse_iterator(str.begin()));
//     return rev_str;
// }

using namespace std;


int main(int argc, char** argv) {


    std::string s = "hello there";
    std::string r = reverse_str(s);
    std::string ss = slicestr(s, 3, 12, 2);
    cout << s << endl << r << endl;
    cout << ss << endl;

    return 0;
}
