#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>

using namespace std;

#define ADTS_channel(header) (((header[2] & 0x1) << 2) | ((header[3] & 0xC0) >> 6))
#define ADTS_length(header) (((header[3] & 0x3) << 11) | (header[4] << 3) | ((header[5] & 0xE0) >> 5))
#define BUFFER_LEN (2<<23)

char* name_prefix;
char out_filename[10240] = {0};

size_t total = 0;

void show_progress(size_t progress) {
    printf("%7d/%d MB %.2lf%%     \r", progress >> 20, total >> 20, progress * 100.0 / total);
}

void sync_signal(ifstream& input) {
    unsigned char buffer[1048576] = {0};
    unsigned char* header;
    input.read((char*) buffer, sizeof(buffer));
    for(header = buffer; header < buffer + sizeof(buffer)-(1<<14); header++) {
        // Check current syncword
        if(header[0] != 0xFF || (header[1] & 0xF6) != 0xF0)
            continue;
        int length = ADTS_length(header);
        // Check next syncword by length
        if(header[length] != 0xFF || (header[length+1] & 0xF6) != 0xF0)
            continue;
        if(header > buffer)
            printf("Syncing to %d bytes\n", header - buffer);
        input.seekg(header - buffer, ios::beg);
        return;
    }
    puts("Unable to sync in first megabytes, halt.");
    exit(-1);
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
    char* buffer = new char[BUFFER_LEN];
    int buffer_valid_length;
    int pending_check_begin;
    size_t stream_pos;
    int i = 0;
    // Get total
    auto beginning = input.tellg();
    input.seekg(0, ios::end);
    total = input.tellg() - beginning;
    input.seekg(0, ios::beg);
    sync_signal(input);
    stream_pos = input.tellg() - beginning;
    while(!input.eof() && stream_pos < total) {
        reread_data:
        stream_pos = input.tellg() - beginning;
        buffer_valid_length = BUFFER_LEN;
        if(stream_pos + buffer_valid_length > total)
            buffer_valid_length = total - stream_pos;

        input.read(buffer, buffer_valid_length);
        input.peek();
        pending_check_begin = 0;

        while(true) {
            unsigned char* header = (unsigned char*)buffer + pending_check_begin;
            if(pending_check_begin + 7 > buffer_valid_length)
                break;
            int adts_len = ADTS_length(header);
            if(pending_check_begin + adts_len > buffer_valid_length)
                break;

            if(header[0] != 0xFF || (header[1] & 0xF0) != 0xF0) {
                printf("Data is corrupted at %ld+%d\n", stream_pos, pending_check_begin);
                printf("Sync word is %X%X, unable to sync.\n", header[0], header[1]);
                return;
            }

            if(ADTS_channel(header) != last_channel) {
                // Channel configuration has changed, writing to new destination
                input.seekg(pending_check_begin - buffer_valid_length, ios_base::cur);

                if(output.is_open()) {
                    output.write(buffer, pending_check_begin);
                    output.close();
                }
                last_channel = ADTS_channel(header);
                sprintf(out_filename, "%s_%d_%dch.aac", name_prefix, ++file_index, ADTS_channel(header));
                printf("Saving to %s ...                    \n", out_filename);
                output.open(out_filename, ios::out | ios::binary | ios::trunc);
                goto reread_data;
            }
            pending_check_begin += adts_len;
        }

        if(output.is_open())
            output.write(buffer, pending_check_begin);
        if(input.eof())
            break;
        input.seekg(pending_check_begin - buffer_valid_length, ios_base::cur);

        // if((block_index & 0x1FFF) == 0)
            show_progress(input.tellg() - beginning);
        // block_index++;
    }
    show_progress(total);
    if(output.is_open())
        output.close();
    delete[] buffer;
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
