#include <string>
#include <fcntl.h>
#include <fstream>
#include <iostream>
using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Need file for writing\n");
        exit(0);
    }
    string file_name = argv[1];
    string str;
    FILE *fd = fopen(file_name.c_str(), "w");
    setbuf(fd, NULL);
    while (cin >> str) {
        fwrite(str.c_str(), 1, str.size(), fd);
        fflush(fd);
        cout << "Try to write " << str.size() << " bytes" << std::endl;
    }
    fclose(fd);
}