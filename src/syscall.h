// Copyright 2021 RnD Center "ELVEES", JSC

#ifndef _SYSCALL_H
#define _SYSCALL_H

struct __attribute__((packed)) stat_compat{
    int16_t     dev;
    uint16_t    ino;
    uint32_t    mode;
    uint16_t    nlink;
    uint16_t    uid;
    uint16_t    gid;
    int16_t     rdev;
    int32_t     size;
    int32_t     atime;
    int32_t     spare1;
    int32_t     mtime;
    int32_t     spare2;
    int32_t     ctime;
    int32_t     spare3;
    int32_t     blksize;
    int32_t     blocks;

    int32_t     spare4[2];
};

struct __attribute__((packed)) tms_compat {
    uint64_t tms_utime;
    uint64_t tms_stime;
    uint64_t tms_cutime;
    uint64_t tms_cstime;
};

#define O_RDWR_COMPAT 2
#define O_WRONLY_COMPAT 1
#define O_RDONLY_COMPAT 0
#define O_CREAT_COMPAT 64
#define O_EXCL_COMPAT 128
#define O_NOCTTY_COMPAT 256
#define O_TRUNC_COMPAT 512
#define O_APPEND_COMPAT 1024
#define O_NONBLOCK_COMPAT 2048
#define O_SYNC_COMPAT 1052672

void ElcoreSyscallHandler(int job_instance_fd, int job_fd);
void ElcoreFlushPreamble(int elcore_fd);

#endif
