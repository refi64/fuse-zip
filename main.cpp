////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2019 by Alexander Galanin                          //
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

#define KEY_HELP (0)
#define KEY_VERSION (1)
#define KEY_RO (2)
#define KEY_FORCE_PRECISE_TIME (3)

#include "config.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include <fuse.h>
#include <fuse_opt.h>

#pragma GCC diagnostic pop

#include <libgen.h>
#include <limits.h>
#include <syslog.h>

#include <cassert>
#include <cerrno>

#include "fuse-zip.h"
#include "fuseZipData.h"

#if (LIBZIP_VERSION_MAJOR < 1)
    #error "libzip >= 1.0 is required!"
#endif

/**
 * Print usage information
 */
void print_usage() {
    fprintf(stderr, "usage: %s [options] <zip-file> <mountpoint>\n\n", PROGRAM);
    fprintf(stderr,
            "general options:\n"
            "    -o opt,[opt...]        mount options\n"
            "    -h   --help            print help\n"
            "    -V   --version         print version\n"
            "    -r   -o ro             open archive in read-only mode\n"
            "    -f                     don't detach from terminal\n"
            "    -d                     turn on debugging, also implies -f\n"
            "\n");
}

/**
 * Print version information (fuse-zip and FUSE library)
 */
void print_version() {
    fprintf(stderr, "%s version: %s\n", PROGRAM, VERSION);
    fprintf(stderr, "libzip version: %s\n", LIBZIP_VERSION);
}

/**
 * Parameters for command-line argument processing function
 */
struct fusezip_param {
    // help shown
    bool help;
    // version information shown
    bool version;
    // number of string arguments
    int strArgCount;
    // zip file name
    const char *fileName;
    // read-only flag
    bool readonly;
    // force precise time
    bool force_precise_time;
};

/**
 * Function to process arguments (called from fuse_opt_parse).
 *
 * @param data  Pointer to fusezip_param structure
 * @param arg is the whole argument or option
 * @param key determines why the processing function was called
 * @param outargs the current output argument list
 * @return -1 on error, 0 if arg is to be discarded, 1 if arg should be kept
 */
static int process_arg(void *data, const char *arg, int key, struct fuse_args *outargs) {
    struct fusezip_param *param = static_cast<fusezip_param*>(data);

    (void)outargs;

    // 'magic' fuse_opt_proc return codes
    const static int KEEP = 1;
    const static int DISCARD = 0;
    const static int ERROR = -1;

    switch (key) {
        case KEY_HELP: {
            print_usage();
            param->help = true;
            return DISCARD;
        }

        case KEY_VERSION: {
            print_version();
            param->version = true;
            return KEEP;
        }

        case KEY_RO: {
            param->readonly = true;
            return KEEP;
        }

        case KEY_FORCE_PRECISE_TIME: {
            param->force_precise_time = true;
            return DISCARD;
        }

        case FUSE_OPT_KEY_NONOPT: {
            ++param->strArgCount;
            switch (param->strArgCount) {
                case 1: {
                    // zip file name
                    param->fileName = arg;
                    return DISCARD;
                }
                case 2: {
                    // mountpoint
                    // keep it and then pass to FUSE initializer
                    return KEEP;
                }
                default:
                    fprintf(stderr, "%s: only two arguments allowed: filename and mountpoint\n", PROGRAM);
                    return ERROR;
            }
        }

        default: {
            return KEEP;
        }
    }
}

/**
 * Check that we can write results to an archive file:
 * * file must be writable;
 * * parent directory must be writable (because the last step of archive saving
 *   is rename-and-replace).
 */
bool isFileWritable(const char *fileName) {
    bool writable = true;
    if (access(fileName, F_OK) == 0 && access(fileName, W_OK) != 0) {
        // file exists but not writable
        writable = false;
    } else {
        char *fileNameDup = strdup(fileName);
        if (fileNameDup == NULL)
            throw std::bad_alloc();
        const char *dirName = dirname(fileNameDup);
        if (access(dirName, F_OK) == 0 && access(dirName, W_OK) != 0) {
            // parent directory is not writable
            writable = false;
        }
        free(fileNameDup);
    }
    return writable;
}

static const struct fuse_opt fusezip_opts[] = {
    FUSE_OPT_KEY("-h",                  KEY_HELP),
    FUSE_OPT_KEY("--help",              KEY_HELP),
    FUSE_OPT_KEY("-V",                  KEY_VERSION),
    FUSE_OPT_KEY("--version",           KEY_VERSION),
    FUSE_OPT_KEY("-r",                  KEY_RO),
    FUSE_OPT_KEY("ro",                  KEY_RO),
    FUSE_OPT_KEY("force_precise_time",  KEY_FORCE_PRECISE_TIME),
    {NULL, 0, 0}
};

int main(int argc, char *argv[]) {
    if (sizeof(void*) > sizeof(uint64_t)) {
        fprintf(stderr,"%s: This program cannot be run on your system because of FUSE design limitation\n", PROGRAM);
        return EXIT_FAILURE;
    }
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    FuseZipData *data = NULL;
    struct fusezip_param param;
    param.help = false;
    param.version = false;
    param.readonly = false;
    param.force_precise_time = false;
    param.strArgCount = 0;
    param.fileName = NULL;

    if (fuse_opt_parse(&args, &param, fusezip_opts, process_arg)) {
        fuse_opt_free_args(&args);
        return EXIT_FAILURE;
    }

    // if all work is done inside options parsing...
    if (param.help) {
        fuse_opt_free_args(&args);
        return EXIT_SUCCESS;
    }
    
    // pass version switch to HELP library to see it's version
    if (!param.version) {
        // no file name passed
        if (param.fileName == NULL) {
            print_usage();
            fuse_opt_free_args(&args);
            return EXIT_FAILURE;
        }

        if (!param.readonly && !isFileWritable(param.fileName)) {
            assert(args.allocated && "FUSE args must be reallocated because at least one argument (archive file name) is discarded in [process_arg]");
            // add -r flag to make file system read-only
            if (fuse_opt_add_arg(&args, "-r") != 0) {
                fuse_opt_free_args(&args);
                return EXIT_FAILURE;
            }
            param.readonly = true;
        }

        openlog(PROGRAM, LOG_PID, LOG_USER);
        if ((data = initFuseZip(PROGRAM, param.fileName, param.readonly, param.force_precise_time))
                == NULL) {
            fuse_opt_free_args(&args);
            return EXIT_FAILURE;
        }
    }

    static struct fuse_operations fusezip_oper;
    fusezip_oper.init       =   fusezip_init;
    fusezip_oper.destroy    =   fusezip_destroy;
    fusezip_oper.readdir    =   fusezip_readdir;
    fusezip_oper.getattr    =   fusezip_getattr;
    fusezip_oper.statfs     =   fusezip_statfs;
    fusezip_oper.open       =   fusezip_open;
    fusezip_oper.read       =   fusezip_read;
    fusezip_oper.write      =   fusezip_write;
    fusezip_oper.release    =   fusezip_release;
    fusezip_oper.unlink     =   fusezip_unlink;
    fusezip_oper.rmdir      =   fusezip_rmdir;
    fusezip_oper.mkdir      =   fusezip_mkdir;
    fusezip_oper.rename     =   fusezip_rename;
    fusezip_oper.create     =   fusezip_create;
    fusezip_oper.mknod      =   fusezip_mknod;
    fusezip_oper.chmod      =   fusezip_chmod;
    fusezip_oper.chown      =   fusezip_chown;
    fusezip_oper.flush      =   fusezip_flush;
    fusezip_oper.fsync      =   fusezip_fsync;
    fusezip_oper.fsyncdir   =   fusezip_fsyncdir;
    fusezip_oper.opendir    =   fusezip_opendir;
    fusezip_oper.releasedir =   fusezip_releasedir;
    fusezip_oper.access     =   fusezip_access;
    fusezip_oper.utimens    =   fusezip_utimens;
    fusezip_oper.ftruncate  =   fusezip_ftruncate;
    fusezip_oper.truncate   =   fusezip_truncate;
    fusezip_oper.setxattr   =   fusezip_setxattr;
    fusezip_oper.getxattr   =   fusezip_getxattr;
    fusezip_oper.listxattr  =   fusezip_listxattr;
    fusezip_oper.removexattr=   fusezip_removexattr;
    fusezip_oper.readlink   =   fusezip_readlink;
    fusezip_oper.symlink    =   fusezip_symlink;

#if FUSE_VERSION >= 28
    // don't allow NULL path
    fusezip_oper.flag_nullpath_ok = 0;
#   if FUSE_VERSION == 29
    fusezip_oper.flag_utime_omit_ok = 1;
#   endif
#endif

    struct fuse *fuse;
    char *mountpoint;
    // this flag ignored because libzip does not supports multithreading
    int multithreaded;
    int res;

    fuse = fuse_setup(args.argc, args.argv, &fusezip_oper, sizeof(fusezip_oper), &mountpoint, &multithreaded, data);
    fuse_opt_free_args(&args);
    if (fuse == NULL) {
        delete data;
        return EXIT_FAILURE;
    }
    res = fuse_loop(fuse);
    fuse_teardown(fuse, mountpoint);
    return (res == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

