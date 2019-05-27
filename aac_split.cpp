#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>

using namespace std;

#define ADTS_channel(header) (((header[2] & 0x1) << 2) | ((header[3] & 0xC0) >> 6))
#define ADTS_length(header) (((header[3] & 0x3) << 11) | (header[4] << 3) | ((header[5] & 0xE0) >> 5))

char* name_prefix;
char out_filename[10240] = {0};

size_t total = 0;

void show_progress(size_t progress) {
    printf("%7d/%d MB %.2lf%%     \r", progress >> 20, total >> 20, progress * 100.0 / total);
}

void split_aac(const char * aac_filename) {
    unsigned int file_index = 0;
    unsigned int block_index = 0;
    int last_channel = -1;
    int len = strlen(aac_filename) - 3;
    name_prefix = new char[len];
    strncpy(name_prefix, aac_filename, len - 1);
    name_prefix[len - 1] = 0;
    printf("Opening %s\n", aac_filename);
    ifstream input(aac_filename, ios::in | ios::binary);
    ofstream output;
    unsigned char header[7];
    char buffer[1<<13];
    int i = 0;
    // Get total
    auto beginning = input.tellg();
    input.seekg(0, ios::end);
    total = input.tellg() - beginning;
    input.seekg(0, ios::beg);
    while(!input.eof()) {
        input.read((char*) header, 7);
        if(header[0] != 0xFF || header[1] & 0xF0 != 0xF0) {
            printf("Data is corrupted at %ld\n", input.tellg() - beginning);
            printf("Sync word is %X%X, unable to sync.\n", header[0], header[1]);
            return;
        }
        input.read(buffer, ADTS_length(header) - 7);
        if(input.eof())
            break;
        if(ADTS_channel(header) != last_channel) {
            // Channel configuration has changed, writing to new destination
            if(output.is_open())
                output.close();
            last_channel = ADTS_channel(header);
            sprintf(out_filename, "%s_%d_%dch.aac", name_prefix, ++file_index, ADTS_channel(header));
            printf("Saving to %s ...                    \n", out_filename);
            output.open(out_filename, ios::out | ios::binary | ios::trunc);
        }
        output.write((char*) header, 7);
        output.write(buffer, ADTS_length(header) - 7);
        if((block_index & 0x1FFF) == 0)
            show_progress(input.tellg() - beginning);
        block_index++;
    }
    show_progress(total);
    if(output.is_open())
        output.close();
}

void help(const char * argv[]) {
    printf("%s <file.aac> ...\n", argv[0]);
    puts("");
    puts("Split AAC with multiple channel configurations into segments.");
    puts("");
    puts("CAUTION! 'file_X_Ych.aac' will be overwritten without confirmation!");
    puts("");
}

int main(int argc, const char * argv[]) {
    if (argc < 2) {
        help(argv);
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        const char* aac_filename = argv[i];
        const char* extension = aac_filename + strlen(aac_filename) - 4;
        if (strcmp(extension, ".aac") != 0) {
            printf("Only .aac file is accepted, %s given.\n", aac_filename);
            return -2;
        }

        split_aac(aac_filename);
    }

    return 0;
}
