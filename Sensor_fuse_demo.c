#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

// Simulated hardware/application states
static char acq_trigger[4] = "0\n";
static char exp_lines[16]   = "100\n";
static char gain_level[16]  = "1\n";

static int cam_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    
    // Define virtual files and their access permissions
    if (strcmp(path, "/command_acquisition") == 0 ||
        strcmp(path, "/exposure_lines") == 0 ||
        strcmp(path, "/gain_level") == 0) {
        stbuf->st_mode = S_IFREG | 0666; // Read/Write for everyone
        stbuf->st_nlink = 1;
        stbuf->st_size = 32;
        return 0;
    }
    return -ENOENT;
}

static int cam_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    if (strcmp(path, "/") != 0) return -ENOENT;
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, "command_acquisition", NULL, 0, 0);
    filler(buf, "exposure_lines", NULL, 0, 0);
    filler(buf, "gain_level", NULL, 0, 0);
    return 0;
}

static int cam_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char *source = NULL;
    if (strcmp(path, "/command_acquisition") == 0) source = acq_trigger;
    else if (strcmp(path, "/exposure_lines") == 0) source = exp_lines;
    else if (strcmp(path, "/gain_level") == 0)     source = gain_level;
    else return -ENOENT;

    size_t len = strlen(source);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;
    memcpy(buf, source + offset, size);
    return size;
}

static int cam_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/command_acquisition") == 0) {
        // Core Logic: User space intercepts the write call directly!
        if (buf[0] == '1') {
            printf("[SpaceWire Daemon] Trigger caught! Reading Exp: %s, Gain: %s\n", exp_lines, gain_level);
            // Construct and fire your RMAP packet over your UIO memory map here
        }
    } else if (strcmp(path, "/exposure_lines") == 0) {
        snprintf(exp_lines, sizeof(exp_lines), "%.*s", (int)size, buf);
    } else if (strcmp(path, "/gain_level") == 0) {
        snprintf(gain_level, sizeof(gain_level), "%.*s", (int)size, buf);
    } else {
        return -ENOENT;
    }
    return size;
}

static const struct fuse_operations cam_oper = {
    .getattr = cam_getattr,
    .readdir = cam_readdir,
    .read    = cam_read,
    .write   = cam_write,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &cam_oper, NULL);
}
