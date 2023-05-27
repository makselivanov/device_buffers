#include <string>
#include <fstream>
#include <iostream>
#include <fcntl.h>
using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Need file for reading\n");
        exit(0);
    }
    string file_name = argv[1];
    int cur_size = 1024;
    char *buffer = (char *)malloc(cur_size);
    int offset, size;
    FILE *fd = fopen(file_name.c_str(), "r");
    setbuf(fd, NULL);
    while (cin >> size >> offset) {
        if (size == 0) {
            break;
        }
        if (offset != 0) {
            fseek(fd, offset, SEEK_CUR);
        }
        if (cur_size < size + 1) {
            free(buffer);
            cur_size = size + 1;
            buffer = (char *)malloc(cur_size);
        }
        fread(buffer, 1, size, fd);
        buffer[size] = '\0';
        cout << ">: " << buffer << std::endl;
    }
    free(buffer);
    fclose(fd);
}