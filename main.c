/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Jeff Epler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "oofatfs/src/ff.h"
#include "oofatfs/src/diskio.h"

void *ff_memalloc(UINT size) {
    return calloc(1,size);
}

void ff_memfree(void *ptr) {
    free(ptr);
}

int fd;
size_t blockcount;

void perror_fatal(const char *msg) {
    perror(msg);
    exit(1);
}

DWORD fattime;
DWORD get_fattime (void) { return fattime; }

DSTATUS disk_initialize (BYTE pdrv) {
    return RES_OK;
}

DSTATUS disk_status (BYTE pdrv) {
    return 0;
}

DRESULT disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
    ssize_t result = pread(fd, buff, count*512, sector*512);    
    if(result < 0) perror_fatal("pread");
    return RES_OK;
}

DRESULT disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
    ssize_t result = pwrite(fd, buff, count*512, sector*512);    
    if(result < 0) perror_fatal("pwrite");
    return RES_OK;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff) {
    switch(cmd) {
        case CTRL_TRIM:
            return RES_OK;
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512; return RES_OK;
        case GET_SECTOR_COUNT:
            *(DWORD*)buff = blockcount; return RES_OK;
        default:
            return RES_ERROR;
    }
}


void usage(const char *argv0, int exitcode) {
    printf("%s: Create a populated FAT filesystem\n", argv0);
    printf("Usage: %s DEST LABEL BLOCKCOUNT FILE...\n", argv0);
    if(exitcode >= 0) exit(exitcode);
}

const char *fresult_string(FRESULT result, char *buf, size_t bufsize) {
    switch(result) {
        case FR_OK:
            return "(0) Succeeded";
        case FR_DISK_ERR:
            return "(1) A hard error occurred in the low level disk I/O layer";
        case FR_INT_ERR:
            return "(2) Assertion failed";
        case FR_NOT_READY:
            return "(3) The physical drive cannot work";
        case FR_NO_FILE:
            return "(4) Could not find the file";
        case FR_NO_PATH:
            return "(5) Could not find the path";
        case FR_INVALID_NAME:
            return "(6) The path name format is invalid";
        case FR_DENIED:
            return "(7) Access denied due to prohibited access or directory full";
        case FR_EXIST:
            return "(8) Access denied due to prohibited access";
        case FR_INVALID_OBJECT:
            return "(9) The file/directory object is invalid";
        case FR_WRITE_PROTECTED:
            return "(10) The physical drive is write protected";
        case FR_INVALID_DRIVE:
            return "(11) The logical drive number is invalid";
        case FR_NOT_ENABLED:
            return "(12) The volume has no work area";
        case FR_NO_FILESYSTEM:
            return "(13) There is no valid FAT volume";
        case FR_MKFS_ABORTED:
            return "(14) The f_mkfs() aborted due to any problem";
        case FR_TIMEOUT:
            return "(15) Could not get a grant to access the volume within defined period";
        case FR_LOCKED:
            return "(16) The operation is rejected according to the file sharing policy";
        case FR_NOT_ENOUGH_CORE:
            return "(17) LFN working buffer could not be allocated";
        case FR_TOO_MANY_OPEN_FILES:
            return "(18) Number of open files > FF_FS_LOCK";
        case FR_INVALID_PARAMETER:
            return "(19) Given parameter is invalid";
        default:
            if(buf) snprintf(buf, bufsize, "(%d) Unknown error", (int)result);
            return buf;
    }
}

void fresult_fatal(const char *msg, FRESULT result) {
    char buf[64];
    fprintf(stderr, "%s: %s\n", msg, fresult_string(result, buf, sizeof(buf)));
    exit(2);
}

bool endswith(const char *haystack, char needle) {
    haystack = haystack + strlen(haystack) - 1;
    return *haystack == needle;
}

void do_one_file(const char *filename_ro) {
    printf("%s\n", filename_ro); fflush(stdout);

    char filename[1024];
    snprintf(filename, sizeof(filename), "%s", filename_ro);
    if(endswith(filename, '/')) {
        filename[strlen(filename)-1] = 0;
        FRESULT fr = f_mkdir(filename);
        if(fr) fresult_fatal("f_mkdir", fr);
        return;
    }
    char *eq = strchr(filename, '=');
    char *src = filename;
    char *dst = filename;
    if(eq) {
        *eq = 0;
        src = eq + 1;
    }

    if(!strcmp(dst, "--circuitpython")) {
        if(src != dst) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "main.py=%s", src); 
            do_one_file(buf);
        }
        do_one_file(".fseventsd/");
        do_one_file(".fseventsd/no_log=");
        do_one_file(".metadata_never_index=");
        do_one_file(".Trashes=");
        return;
    }

    FIL fp;
    FRESULT fr = f_open(&fp, dst, FA_WRITE | FA_CREATE_ALWAYS);
    if(fr) fresult_fatal("f_open", fr);

    if(!*src) {
        f_close(&fp);
        return;
    }

    int ifd = open(src, O_RDONLY);
    if(ifd < 0) perror("open");

    while(1) {
        char buf[512];
        ssize_t nread = read(ifd, buf, sizeof(buf));
        if(nread == 0) break;
        if(nread < 0) perror("read");

        UINT nwritten;
        fr = f_write(&fp, buf, (UINT)nread, &nwritten);
        if(fr) fresult_fatal("f_write", fr);
    }
    close(ifd);
    f_close(&fp);
}

int main(int argc, char **argv) {
    const char *SOURCE_DATE_EPOCH_STRING = getenv("SOURCE_DATE_EPOCH");
    time_t unix_timestamp;
    if(SOURCE_DATE_EPOCH_STRING)
        unix_timestamp = (time_t)strtoull(SOURCE_DATE_EPOCH_STRING, NULL, 0);
    else
        unix_timestamp = time(NULL);

    struct tm *t = localtime(&unix_timestamp);
    fattime =
        (((t->tm_year - 80) & 127) << 25) |
        ((t->tm_mon+1) << 21) |
        ((t->tm_mday) << 16) |
        ((t->tm_hour) << 11) |
        ((t->tm_min) << 5) |
        (t->tm_sec / 2);

    if(argc < 4) 
        usage(argv[0], 1);
    const char *dest = argv[1];
    const char *label = argv[2];
    blockcount = strtoul(argv[3], NULL, 0);
    
    fd = open(dest, O_RDWR | O_CREAT, 0666);
    if(fd < 0) perror_fatal("open dest");

    int res = ftruncate(fd, 512 * blockcount);
    if(res < 0) perror_fatal("truncate");

    uint8_t working_buf[512];

    FATFS fs;
    memset(&fs, 0, sizeof(fs));

    FRESULT fr;
    fr = f_mount(&fs, "", 0);
    if(fr) fresult_fatal("f_mount (1)", fr);

    fr = f_mkfs("", FM_FAT | FM_SFD, 0, working_buf, sizeof(working_buf));
    if(fr) fresult_fatal("f_mkfs", fr);

    fr = f_mount(&fs, "", 1);
    if(fr) fresult_fatal("f_mount (2)", fr);

    fr = f_setlabel(label);
    if(fr) fresult_fatal("f_setlabel", fr);

    for(int i=4; i<argc; i++) {
        do_one_file(argv[i]);
    }
}
