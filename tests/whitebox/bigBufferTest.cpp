////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2010-2021 by Alexander Galanin                          //
//  al@galanin.nnov.ru                                                    //
//  http://galanin.nnov.ru/~al                                            //
//                                                                        //
//  This program is free software: you can redistribute it and/or modify  //
//  it under the terms of the GNU General Public License as published by  //
//  the Free Software Foundation, either version 3 of the License, or     //
//  (at your option) any later version.                                   //
//                                                                        //
//  This program is distributed in the hope that it will be useful,       //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//  GNU General Public License for more details.                          //
//                                                                        //
//  You should have received a copy of the GNU General Public License     //
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.//
////////////////////////////////////////////////////////////////////////////

#include "../config.h"

#include <zip.h>
#include <assert.h>
#include <stdlib.h>
#include <cstring>
#include <cerrno>

// Public Morozoff design pattern :)
#define private public

#include "bigBuffer.h"
#include "common.h"

// global variables

// is libzip functions should be called?
bool use_zip = false;

// libzip stub structures
struct zip {
    bool fail_zip_fopen_index;
    bool fail_zip_fread;
    bool zip_fread_custom_return;
    zip_uint64_t zip_fread_custom_return_length;
    bool fail_zip_fclose;
    bool fail_zip_source_function;
    bool fail_zip_add;
    bool fail_zip_replace;

    struct zip_source *source;

    zip(): zip_fread_custom_return(false) {}
};
struct zip_file {
    struct zip *zip;
};
struct zip_source {
    struct zip *zip;
    void *cbs;
};

// libzip stub functions

zip_int64_t zip_file_add(struct zip *z, const char *, struct zip_source *, zip_flags_t) {
    assert(use_zip);

    return z->fail_zip_add ? -1 : 0;
}

int zip_file_replace(struct zip *z, zip_uint64_t, struct zip_source *, zip_flags_t) {
    assert(use_zip);
    return z->fail_zip_replace ? -1 : 0;
}

struct zip_file *zip_fopen_index(struct zip *z, zip_uint64_t, zip_flags_t) {
    assert(use_zip);
    if (z->fail_zip_fopen_index) {
        return NULL;
    } else {
        struct zip_file *res = (struct zip_file *)malloc(sizeof(struct zip_file));
        res->zip = z;
        return res;
    }
}

zip_int64_t zip_fread(struct zip_file *zf, void *dest, zip_uint64_t size) {
    assert(use_zip);
    if (zf->zip->fail_zip_fread) {
        return -1;
    } else {
        if (zf->zip->zip_fread_custom_return) {
            size = zf->zip->zip_fread_custom_return_length;
        }
        memset(dest, 'X', size);
        return size;
    }
}

int zip_fclose(struct zip_file *zf) {
    assert(use_zip);
    bool fail = zf->zip->fail_zip_fclose;
    free(zf);
    return fail ? -1 : 0;
}

struct zip_source *zip_source_function(struct zip *z, zip_source_callback, void *cbs) {
    assert(use_zip);
    if (z->fail_zip_source_function) {
        return NULL;
    } else {
        struct zip_source *zs = (struct zip_source *)malloc(sizeof(struct zip_source));
        zs->zip = z;
        zs->cbs = cbs;
        z->source = zs;
        return zs;
    }
}

void zip_source_free(struct zip_source *z) {
    assert(use_zip);
    assert(z->zip->fail_zip_add || z->zip->fail_zip_replace);
    free(z);
}

const char *zip_get_name(struct zip *, zip_uint64_t, zip_flags_t) {
    return "file.name";
}

const char *zip_strerror(struct zip *) {
    return "human-readable error (global)";
}

const char *zip_file_strerror(struct zip_file *) {
    return "human-readable error (file-specific)";
}

// only stubs

struct zip *zip_open(const char *, int, int *) {
    assert(false);
    return NULL;
}

int zip_error_to_str(char *, zip_uint64_t, int, int) {
    assert(false);
    return 0;
}

zip_int64_t zip_dir_add(struct zip *, const char *, zip_flags_t) {
    assert(false);
    return 0;
}

int zip_close(struct zip *) {
    assert(false);
    return 0;
}

int zip_delete(struct zip *, int) {
    assert(false);
    return 0;
}

int zip_get_num_files(struct zip *) {
    assert(false);
    return 0;
}

int zip_rename(struct zip *, int, const char *) {
    assert(false);
    return 0;
}

int zip_stat_index(struct zip *, int, int, struct zip_stat *) {
    assert(false);
    return 0;
}

////////////////////////////////////////////////////////////////////////////
// TESTS
////////////////////////////////////////////////////////////////////////////

void chunkLocators() {
    // static functions test
    assert(BigBuffer::chunksCount(0) == 0);
    assert(BigBuffer::chunksCount(1) == 1);
    assert(BigBuffer::chunksCount(BigBuffer::chunkSize) == 1);
    assert(BigBuffer::chunksCount(BigBuffer::chunkSize - 1) == 1);
    assert(BigBuffer::chunksCount(BigBuffer::chunkSize + 1) == 2);
    assert(BigBuffer::chunksCount(BigBuffer::chunkSize * 2 - 1) == 2);

    assert(BigBuffer::chunkNumber(0) == 0);
    assert(BigBuffer::chunkNumber(1) == 0);
    assert(BigBuffer::chunkNumber(BigBuffer::chunkSize) == 1);
    assert(BigBuffer::chunkNumber(BigBuffer::chunkSize - 1) == 0);
    assert(BigBuffer::chunkNumber(BigBuffer::chunkSize + 1) == 1);
    assert(BigBuffer::chunkNumber(BigBuffer::chunkSize * 2 - 1) == 1);

    assert(BigBuffer::chunkOffset(0) == 0);
    assert(BigBuffer::chunkOffset(1) == 1);
    assert(BigBuffer::chunkOffset(BigBuffer::chunkSize) == 0);
    assert(BigBuffer::chunkOffset(BigBuffer::chunkSize - 1) == BigBuffer::chunkSize - 1);
    assert(BigBuffer::chunkOffset(BigBuffer::chunkSize + 1) == 1);
    assert(BigBuffer::chunkOffset(BigBuffer::chunkSize * 2 - 1) == BigBuffer::chunkSize - 1);
}

void createDelete() {
    BigBuffer bb;
    assert(bb.len == 0);
}

void truncate() {
    BigBuffer bb;

    bb.truncate(22);
    assert(bb.len == 22);

    bb.truncate(2);
    assert(bb.len == 2);

    bb.truncate(BigBuffer::chunkSize);
    assert(bb.len == BigBuffer::chunkSize);

    bb.truncate(BigBuffer::chunkSize + 1);
    assert(bb.len == BigBuffer::chunkSize + 1);

    bb.truncate(0);
    assert(bb.len == 0);
}

void readFile() {
    char buf[0xff];
    char empty[0xff];
    memset(empty, 0, 0xff);
    int nr;
    BigBuffer bb;

    nr = bb.read(buf, 100, 0);
    assert(nr == 0);

    nr = bb.read(buf, 100, 100);
    assert(nr == 0);

    bb.truncate(10);
    nr = bb.read(buf, 10, 0);
    assert(nr == 10);
    assert(memcmp(buf, empty, nr) == 0);

    bb.truncate(BigBuffer::chunkSize);
    nr = bb.read(buf, 10, BigBuffer::chunkSize - 5);
    assert(nr == 5);
    assert(memcmp(buf, empty, nr) == 0);
}

// read (size > chunkSize)
void readFileOverChunkSize() {
    int n = BigBuffer::chunkSize * 3 + 15;
    char buf[n];
    char empty[n];
    memset(empty, 0, n);
    int nr;
    BigBuffer bb;

    nr = bb.read(buf, n, 0);
    assert(nr == 0);

    nr = bb.read(buf, n, 100);
    assert(nr == 0);

    bb.truncate(10);
    nr = bb.read(buf, 10, 0);
    assert(nr == 10);
    assert(memcmp(buf, empty, nr) == 0);

    bb.truncate(BigBuffer::chunkSize);
    nr = bb.read(buf, n, BigBuffer::chunkSize - 5);
    assert(nr == 5);
    assert(memcmp(buf, empty, nr) == 0);

    bb.truncate(BigBuffer::chunkSize * 2 - 12);
    nr = bb.read(buf, n, 1);
    assert(nr == BigBuffer::chunkSize * 2 - 12 - 1);
    assert(memcmp(buf, empty, nr) == 0);

    bb.truncate(BigBuffer::chunkSize * 10);
    nr = bb.read(buf, n, 1);
    assert(nr == n);
    assert(memcmp(buf, empty, nr) == 0);
}

// read data created by truncate
void truncateRead() {
    char buf[BigBuffer::chunkSize];
    char empty[BigBuffer::chunkSize];
    memset(empty, 0, BigBuffer::chunkSize);
    BigBuffer b;
    b.truncate(BigBuffer::chunkSize);
    assert(b.len == BigBuffer::chunkSize);
    int nr = b.read(buf, BigBuffer::chunkSize, 0);
    assert((unsigned)nr == BigBuffer::chunkSize);
    assert(memcmp(buf, empty, BigBuffer::chunkSize) == 0);
}

// writing to file
void writeFile() {
    char buf[0xff];
    char buf2[0xff];
    int nr, nw;
    BigBuffer bb;

    nw = bb.write(buf, 0, 0);
    assert(nw == 0);
    assert(bb.len == 0);

    memset(buf, 1, 10);
    memset(buf+10, 2, 10);
    nw = bb.write(buf, 20, 0);
    assert(nw == 20);
    assert(bb.len == 20);
    nr = bb.read(buf2, 30, 0);
    assert(nr == 20);
    assert(memcmp(buf, buf2, 20) == 0);

    bb.truncate(0);
    nw = bb.write(buf, 20, 0);
    assert(nw == 20);
    assert(bb.len == 20);
    nr = bb.read(buf2, 20, 10);
    assert(nr == 10);
    assert(memcmp(buf + 10, buf2, 10) == 0);
}

// read data from file expanded by write
void readExpanded() {
    int n = BigBuffer::chunkSize * 2;
    char buf[n];
    char expected[n];
    memset(expected, 0, n);
    BigBuffer b;

    memset(buf, 'a', 10);
    memset(expected, 'a', 10);
    b.write(buf, 10, 0);
    assert(b.len == 10);

    memset(buf, 'z', 10);
    memset(expected + BigBuffer::chunkSize + 10, 'z', 10);
    b.write(buf, 10, BigBuffer::chunkSize + 10);
    assert(b.len == BigBuffer::chunkSize + 20);

    int nr = b.read(buf, n, 0);
    assert((unsigned)nr == BigBuffer::chunkSize + 20);
    assert(memcmp(buf, expected, nr) == 0);
}

// read data from file expanded by write (at buffer boundary)
// Issue #50: Out of bounds write on sparse file
void readExpandedBufferBoundary() {
    int n = BigBuffer::chunkSize * 3;
    char buf[n];
    char expected[n];
    memset(expected, 0, n);
    BigBuffer b;

    memset(buf, 'a', BigBuffer::chunkSize);
    memset(expected, 'a', BigBuffer::chunkSize);
    b.write(buf, BigBuffer::chunkSize, 0);
    assert(b.len == BigBuffer::chunkSize);

    memset(buf, 'z', BigBuffer::chunkSize);
    memset(expected + BigBuffer::chunkSize * 2, 'z', BigBuffer::chunkSize);
    b.write(buf, BigBuffer::chunkSize, BigBuffer::chunkSize * 2);
    assert(b.len == BigBuffer::chunkSize * 3);

    int nr = b.read(buf, n, 0);
    assert((unsigned)nr == BigBuffer::chunkSize * 3);
    assert(memcmp(buf, expected, nr) == 0);
}

// Test zip user function callback with empty file
void zipUserFunctionCallBackEmpty() {
    BigBuffer bb;
    struct BigBuffer::CallBackStruct *cbs = new BigBuffer::CallBackStruct();
    cbs->buf = &bb;
    cbs->mtime = 12345;

    struct zip_stat stat;
    assert(BigBuffer::zipUserFunctionCallback(cbs, &stat, 0, ZIP_SOURCE_STAT)
            == sizeof(struct zip_stat));
    assert(stat.size == 0);
    assert(stat.mtime == 12345);

    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_OPEN)
            == 0);
    char buf[0xff];
    assert(BigBuffer::zipUserFunctionCallback(cbs, buf, 0xff, ZIP_SOURCE_READ)
            == 0);
    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_CLOSE)
            == 0);
    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_FREE)
            == 0);
}

// Test zip user function callback with non-empty file
void zipUserFunctionCallBackNonEmpty() {
    zip_uint64_t n = BigBuffer::chunkSize*2;
    char buf[n];
    memset(buf, 'f', n);

    BigBuffer bb;
    bb.write(buf, n, 0);

    struct BigBuffer::CallBackStruct *cbs = new BigBuffer::CallBackStruct();
    cbs->buf = &bb;
    cbs->mtime = 0;

    struct zip_stat stat;
    assert(BigBuffer::zipUserFunctionCallback(cbs, &stat, 0, ZIP_SOURCE_STAT)
            == sizeof(struct zip_stat));
    assert(stat.size == n);
    assert(stat.mtime == 0);

    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_OPEN)
            == 0);
    assert(BigBuffer::zipUserFunctionCallback(cbs, buf, BigBuffer::chunkSize,
                ZIP_SOURCE_READ) == BigBuffer::chunkSize);
    assert(BigBuffer::zipUserFunctionCallback(cbs, buf, BigBuffer::chunkSize,
                ZIP_SOURCE_READ) == BigBuffer::chunkSize);
    assert(BigBuffer::zipUserFunctionCallback(cbs, buf, BigBuffer::chunkSize,
                ZIP_SOURCE_READ) == 0);
    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_CLOSE)
            == 0);
    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_FREE)
            == 0);
}

// Read from zip file
void readZip() {
    int size = 100;
    struct zip z;
    z.fail_zip_fopen_index = true;
    // invalid file
    {
        bool thrown = false;
        try {
            BigBuffer bb(&z, 1, size);
        }
        catch (const std::exception &e) {
            thrown = true;
        }
        assert(thrown);
    }
    z.fail_zip_fopen_index = false;
    z.fail_zip_fread = true;
    // read error
    {
        bool thrown = false;
        try {
            BigBuffer bb(&z, 2, size);
        }
        catch (const std::exception &e) {
            thrown = true;
        }
        assert(thrown);
    }
    z.fail_zip_fread = false;
    z.fail_zip_fclose = true;
    // close error
    {
        bool thrown = false;
        try {
            BigBuffer bb(&z, 3, size);
        }
        catch (const std::exception &e) {
            thrown = true;
        }
        assert(thrown);
    }
    z.fail_zip_fclose = false;
    // normal case
    {
        BigBuffer bb(&z, 0, size);
        char *buf = (char *)malloc(size);
        assert(bb.read(buf, size, 0) == size);
        for (int i = 0; i < size; ++i) {
            assert(buf[i] == 'X');
        }
        free(buf);
    }
}

// Save file to zip
// Check that saveToZip correctly working
void writeZip() {
    // new file
    {
        BigBuffer bb;
        struct zip z;
        zip_int64_t id;
        z.fail_zip_source_function = true;
        id = -1;
        assert(bb.saveToZip(time(NULL), &z, "bebebe.txt", true, id) == -ENOMEM);

        z.fail_zip_source_function = false;
        z.fail_zip_add = true;
        id = -1;
        assert(bb.saveToZip(time(NULL), &z, "bebebe.txt", true, id) == -ENOMEM);

        z.fail_zip_add = false;
        z.source = NULL;
        id = -1;
        assert(bb.saveToZip(time(NULL), &z, "bebebe.txt", true, id) == 0);
        delete (BigBuffer::CallBackStruct *)z.source->cbs;
        free(z.source);
    }
    // existing file
    {
        int size = 11111;
        zip_int64_t id = 11;
        struct zip z;
        z.fail_zip_fopen_index = false;
        z.fail_zip_fread = false;
        z.fail_zip_fclose = false;
        BigBuffer bb(&z, 0, size);

        z.fail_zip_source_function = true;
        assert(bb.saveToZip(time(NULL), &z, "bebebe.txt", false, id) == -ENOMEM);
        assert(id == 11);

        z.fail_zip_source_function = false;
        z.fail_zip_replace = true;
        assert(bb.saveToZip(time(NULL), &z, "bebebe.txt", false, id) == -ENOMEM);
        assert(id == 11);

        z.fail_zip_replace = false;
        z.source = NULL;
        assert(bb.saveToZip(time(NULL), &z, "bebebe.txt", false, id) == 0);
        assert(id == 11);
        delete (BigBuffer::CallBackStruct *)z.source->cbs;
        free(z.source);
    }
}

void zipFReadLengthFailure() {
    BigBuffer bb;
    struct zip z;
    z.fail_zip_fopen_index = false;
    z.fail_zip_fread = false;
    z.zip_fread_custom_return = true;
    z.zip_fread_custom_return_length = 22;
    // data is longer that specified in header
    {
        bool thrown = false;
        try {
            BigBuffer bb(&z, 2, 10);
        }
        catch (const std::exception &e) {
            thrown = true;
        }
        assert(thrown);
    }
    // zero read length
    z.zip_fread_custom_return_length = 0;
    {
        bool thrown = false;
        try {
            BigBuffer bb(&z, 2, 10);
        }
        catch (const std::exception &e) {
            thrown = true;
        }
        assert(thrown);
    }
}

int main(int, char **) {
    initTest();

    chunkLocators();
    createDelete();
    truncate();
    readFile();
    readFileOverChunkSize();
    truncateRead();
    writeFile();
    readExpanded();
    readExpandedBufferBoundary();
    zipUserFunctionCallBackEmpty();
    zipUserFunctionCallBackNonEmpty();

    use_zip = true;
    readZip();
    writeZip();

    zipFReadLengthFailure();

    return EXIT_SUCCESS;
}

