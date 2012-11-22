/**
 * lunpack.c -- unpacker for packfile format used by AIMS
 * written on 2012-11-21 by K
 * 
 * This packfile format resembles the classic Quake PACK format, with some
 * small differences. 8-byte header, list of file entries, then file data.
 * Each entry is 80 bytes long, and each blob of data starts at an offset like
 * 0x***00, with padding added if necessary. See the packheader and packitem
 * structs below.
 *
 * AIMS toolkit download: http://aims.dna-softwares.com/?page_id=14
 * Serves as a companion to lpack.exe
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>


/** A Few Globals **/
char packname[MAX_PATH + 1], outputfolder[MAX_PATH + 1]; // file/path strings
FILE *packfile, *outputfile; // file handles, of course
char *buff; // buffer used for reading file data from the packfile
char using_decompressor = 0; // defaults to 0/false, set to 1/true to automatically
                             // decompress each file using LLZSS.exe if available.
                             // This uses system(), so it carries a performance penalty.


/** Structs **/
#pragma pack(1)
typedef struct {
    char magic[4]; // "PACK", just the way id used to make em
    uint32_t itemcount; // number of items contained in this packfile
} packheader;

#pragma pack(1)
typedef struct { 
    char name[64]; // name of file
    uint32_t unknown1; // unknown value, doesn't appear to be crucial for unpacking
    uint32_t unknown2; // ditto
    uint32_t start; // offset in packfile where this file begins
    uint32_t length; // length of file in bytes
} packitem;


/** Functions **/

/* Takes a path, strips the filename using dirname(), and creates directories
 * from the top down.
 * Similar to mkdir -p
 */
void prepare_directory_tree(char *file) {
    // duplicate...
    char path[MAX_PATH + 1], built_path[MAX_PATH + 1];
    char *tokensplit;
    strncpy(path, file, MAX_PATH + 1);
    dirname(path);
    
    // ...and split
    tokensplit = strtok(path, "/");
    strcpy(built_path, ".");
    while(tokensplit != NULL) {
        strcat(built_path, "/");
        strcat(built_path, tokensplit);
        mkdir(built_path);

        tokensplit = strtok(NULL, "/");
    }
}

/* Replace instances of old_char with new_char in str.
 * Similar to s/old_char/new_char/g
 */
void substitute_chars(char *str, int old_char, int new_char) {
    char *test = strchr(str, old_char);
    
    while (test) {
        *test = (char) new_char;
        test++;
        test = strchr(test, old_char);
    }
}

/* Ensure we actually have a handle for the packfile, exit if not.
 * Used at the start of any function that relies on the packfile.
 */
void check_handle() {
    if(packfile == NULL) {
        printf("FATAL: packfile handle not defined.\n");
        exit(EXIT_FAILURE);
    }
}

/* Returns size of packfile in bytes.
 */
size_t get_filesize() {
    check_handle();

    fseek(packfile, 0, SEEK_END);
    return ftell(packfile);
}

/* Returns the number of files inside the packfile. Uses the value at 0x04.
 */
uint32_t get_itemcount() {
    check_handle();

    rewind(packfile);
    packheader head;
    fread(&head, sizeof(head), 1, packfile);
    
    return head.itemcount;
}

/* Parses packfile and writes out all files within it.
 * Preserves directory structure.
 */
int unpack_file() {
    check_handle();
    
    printf("Packfile size: %d bytes (%d MB)\n", get_filesize(), get_filesize()/1024/1024);
    
    uint32_t itemcount = get_itemcount();
    printf("%d (0x%04x) items in packfile.\n\n", itemcount, itemcount);

    packitem files[itemcount]; // list of file entries
    
    // populate list of items
    uint32_t i;
    for(i=0; i<itemcount; ++i) {
        packitem p;
        fread(&p, sizeof(p), 1, packfile);
        files[i] = p;
    }
    
    // write out each file
    for(i=0; i<itemcount; ++i) {
        fseek(packfile, files[i].start, SEEK_SET);
        printf("[%4d] %08x .. %s\n", i + 1, files[i].start, files[i].name);
        buff = (char*) malloc(sizeof(char) * files[i].length); // set buffer to size of file
        
        size_t result = fread(buff, 1, files[i].length, packfile);
        if(result != files[i].length) {
            printf("  - WARNING: Expected %d bytes, only read %d bytes.\n", files[i].length, result);
        }
        
        substitute_chars(files[i].name, '\\', '/'); // packfile uses \ for its path separator, change to /
        char outputname[MAX_PATH + 1];
        strncpy(outputname, outputfolder, MAX_PATH + 1);
        strcat(outputname, "/");
        strcat(outputname, files[i].name);
        
        prepare_directory_tree(outputname);
        
        printf("  - Writing to %s...\n", outputname);
        
        outputfile = fopen(outputname, "wb");
        if(outputfile == NULL) {
            perror("  - ERROR: ");
        }
        fwrite(buff, 1, files[i].length, outputfile);
        fclose(outputfile);
        
        if(using_decompressor != 0) { // decompress output file if enabled
            char command[MAX_PATH * 2 + 14];
            strcpy(command, "LLZSS.exe ");
            strncat(command, outputname, MAX_PATH + 1);
            strcat(command, " ");
            strncat(command, outputname, MAX_PATH + 1);
            strcat(command, " -d");
            printf("  - Decompressing %s...\n", outputname);
            system(command);
        }
        
        free(buff);
    }
    
    return 0;
}

/* Main, of course. Checks arguments, sets up paths and handles, etc.
 */
int main(int argc, char *argv[]) {
    if(argc <= 1) {
        printf("lunpack: Unpacks files made by lpack. \n");
        printf("Usage: lunpack packfile.p [-d]\n\n");
        printf("-d: Decompress files using LLZSS.exe if available. Uses system(), which\n");
        printf("    slows the process down. Do not use this switch with .mus files, since \n");
        printf("    the data in them is already decompressed.\n");
        return EXIT_FAILURE;
    }
    
    if((strcpy(packname, argv[1]), (packfile = fopen(packname, "rb")) == NULL)) {
        printf("ERROR: %s could not be read or does not exist.\n", packname);
        return EXIT_FAILURE;
    }
    
    strcpy(outputfolder, packname);
    strtok(outputfolder, "."); // split off before first .
    if(strcmp(strtok(NULL, "."), "mus") == 0) { // check text after first .
        printf(".mus file detected, appending -music to folder name.\n");
        strcat(outputfolder, "-music");
    }
    
    if(argc >= 3 && (strcmp(argv[2], "-d") == 0)) { // -d option specified, turn on decompression
        printf("-d switch given, decompressing files with LLZSS.exe.\n");
        using_decompressor = 1;
    }
    if(using_decompressor != 0 && access("LLZSS.exe", F_OK) == -1) {
        printf("LLZSS.exe missing, disabling decompression.\n");
        using_decompressor = 0;
    }
    
    if(errno == EACCES) {
        printf("ERROR: Could not create output folder %s.\n", outputfolder);
        return EXIT_FAILURE;
    }
    
    printf("Unpacking %s into %s/\n\n", packname, outputfolder);
    unpack_file();
    fclose(packfile);
    
    return EXIT_SUCCESS;
}