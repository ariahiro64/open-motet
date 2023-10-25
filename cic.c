/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Open Motet: A libre clone of motetIII. motetIII was an anicent tools that allowed
//      you to use a different cic chip than the game originally used.
//  Open Motet does not "crack" games but simply allows them to boot. This means any
//      antipiracy mesures can and will trigger,
//  Everything is open except fo the boot emulators that are packaged as binary blobs.
//      I need to sleep once every 3 days.
//  Reimlimentation by Andre Roberto Futej.
//      to compile for window do: x86_64-w64-mingw32-gcc -O2 *.c -static-libgcc -o cic.exe
//      to compile for linux  do: gcc -O2 *.c -o cic
//
//  This is licenced under GPL2. I just want improvements to be public. 
//  
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ROL(i, b) (((i) << (b)) | ((i) >> (32 - (b))))
#define BYTES2LONG(b) ( (b)[0] << 24 | \
                        (b)[1] << 16 | \
                        (b)[2] <<  8 | \
                        (b)[3] )

#define N64_HEADER_SIZE  0x40
#define N64_BC_SIZE      (0x1000 - N64_HEADER_SIZE)

#define N64_CRC1         0x10
#define N64_CRC2         0x14

#define CHECKSUM_START   0x00001000
#define CHECKSUM_LENGTH  0x00100000
#define CHECKSUM_CIC6102 0xF8CA4DDC
#define CHECKSUM_CIC6103 0xA3886759
#define CHECKSUM_CIC6105 0xDF26F436
#define CHECKSUM_CIC6106 0x1FEA617A

#define Write32(Buffer, Offset, Value)\
    Buffer[Offset] = (Value & 0xFF000000) >> 24;\
    Buffer[Offset + 1] = (Value & 0x00FF0000) >> 16;\
    Buffer[Offset + 2] = (Value & 0x0000FF00) >> 8;\
    Buffer[Offset + 3] = (Value & 0x000000FF);\

unsigned int crc_table[256];

void gen_table() {
    unsigned int crc, poly;
    int i, j;

    poly = 0xEDB88320;
    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 8; j > 0; j--) {
            if (crc & 1)
                crc = (crc >> 1) ^ poly;
            else
                crc >>= 1;
        }
        crc_table[i] = crc;
    }
}

unsigned int crc32(unsigned char *data, int len) {
    unsigned int crc = ~0;
    int i;

    for (i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc_table[(crc ^ data[i]) & 0xFF];
    }

    return ~crc;
}

int N64CalcCRC(unsigned int *crc, unsigned char *data, int cic) {
    int bootcode, i;
    unsigned int seed;

    unsigned int t1, t2, t3;
    unsigned int t4, t5, t6;
    unsigned int r, d;

    switch (cic) {
        case 6101:
        case 6102:
            seed = CHECKSUM_CIC6102;
            break;
        case 6103:
            seed = CHECKSUM_CIC6103;
            break;
        case 6105:
            seed = CHECKSUM_CIC6105;
            break;
        case 6106:
            seed = CHECKSUM_CIC6106;
            break;
        default:
            return 1;
    }

    t1 = t2 = t3 = t4 = t5 = t6 = seed;

    i = CHECKSUM_START;
    while (i < (CHECKSUM_START + CHECKSUM_LENGTH)) {
        d = BYTES2LONG(&data[i]);
        if ((t6 + d) < t6)
            t4++;
        t6 += d;
        t3 ^= d;
        r = ROL(d, (d & 0x1F));
        t5 += r;
        if (t2 > d)
            t2 ^= r;
        else
            t2 ^= t6 ^ d;

        if (cic == 6105)
            t1 += BYTES2LONG(&data[N64_HEADER_SIZE + 0x0710 + (i & 0xFF)]) ^ d;
        else
            t1 += t5 ^ d;

        i += 4;
    }
    if (cic == 6103) {
        crc[0] = (t6 ^ t4) + t3;
        crc[1] = (t5 ^ t2) + t1;
    } else if (cic == 6106) {
        crc[0] = (t6 * t4) + t3;
        crc[1] = (t5 * t2) + t1;
    } else {
        crc[0] = t6 ^ t4 ^ t3;
        crc[1] = t5 ^ t2 ^ t1;
    }

    return 0;
}

void copyFileData(unsigned char *buffer, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Unable to open file: %s\n", filename);
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    fread(buffer, 1, fileSize, file);
    fclose(file);
}

void appendRestOfFile(FILE *fin, FILE *fout) {
    unsigned char buffer[1024];
    size_t bytesRead;

    // Move the file pointer to the end of the written data
    fseek(fin, (CHECKSUM_START + CHECKSUM_LENGTH), SEEK_SET);

    // Read and append the remaining data from the input file to the output file
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), fin)) > 0) {
        fwrite(buffer, 1, bytesRead, fout);
    }
}

int main(int argc, char **argv) {
    FILE *fin, *fout;
    int cic;
    unsigned int crc[2];
    unsigned char *buffer;

    // Init CRC algorithm
    gen_table();

    // Check args
    if (argc != 4) {
        printf("Open Motet: A libre clone of motetIII.\nUsage: cic <infile> <outfile> <cic>\nvalid cics are 6101 6102 6103 6105 6106 (old parameters have been replaced)\n");
        return 1;
    }

    // Open input file
    if (!(fin = fopen(argv[1], "r+b"))) {
        printf("Unable to open \"%s\" in mode \"%s\"\n", argv[1], "r+b");
        return 1;
    }

    // Open output file
    if (!(fout = fopen(argv[2], "w+b"))) {
        printf("Unable to open \"%s\" in mode \"%s\"\n", argv[2], "w+b");
        fclose(fin);
        return 1;
    }

    // Allocate memory
    if (!(buffer = (unsigned char *) malloc((CHECKSUM_START + CHECKSUM_LENGTH)))) {
        printf("Unable to allocate %d bytes of memory\n", (CHECKSUM_START + CHECKSUM_LENGTH));
        fclose(fin);
        fclose(fout);
        return 1;
    }

    // Read data
    if (fread(buffer, 1, (CHECKSUM_START + CHECKSUM_LENGTH), fin) != (CHECKSUM_START + CHECKSUM_LENGTH)) {
        printf("Unable to read %d bytes of data (invalid N64 image?)\n", (CHECKSUM_START + CHECKSUM_LENGTH));
        fclose(fin);
        fclose(fout);
        free(buffer);
        return 1;
    }

    // Get CIC BootChip
    cic = atoi(argv[3]);
    printf("BootChip: ");
    printf((cic ? "CIC-NUS-%d\n" : "Unknown\n"), cic);

    // Calculate CRC
    if (N64CalcCRC(crc, buffer, cic)) {
        printf("Unable to calculate CRC\n");
    } else {
        printf("CRC 1: 0x%08X  ", BYTES2LONG(&buffer[N64_CRC1]));
        printf("Calculated: 0x%08X ", crc[0]);
        if (crc[0] == BYTES2LONG(&buffer[N64_CRC1]))
            printf("(Good)\n");
        else {
            Write32(buffer, N64_CRC1, crc[0]);
            printf("(Bad, fixed)\n");
        }

        printf("CRC 2: 0x%08X  ", BYTES2LONG(&buffer[N64_CRC2]));
        printf("Calculated: 0x%08X ", crc[1]);
        if (crc[1] == BYTES2LONG(&buffer[N64_CRC2]))
            printf("(Good)\n");
        else {
            Write32(buffer, N64_CRC2, crc[1]);
            printf("(Bad, fixed)\n");
        }
    }

    // Modify Entry Point based on CIC type
    if (cic == 6103)
        buffer[0x09] += 0x10;
    else if (cic == 6106)
        buffer[0x09] += 0x20;

    // Copy data from specified Boot Emulator
    char romFileName[10];
    sprintf(romFileName, "%d.ROM", cic);
    copyFileData(&buffer[N64_HEADER_SIZE], romFileName);

    // Write modified data to output file
    fwrite(buffer, 1, (CHECKSUM_START + CHECKSUM_LENGTH), fout);

    // Append the rest of the input file to the output file
    appendRestOfFile(fin, fout);

    fclose(fin);
    fclose(fout);
    free(buffer);

    return 0;
}

