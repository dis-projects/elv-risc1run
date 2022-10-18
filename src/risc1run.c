#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/times.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <elf.h>
#include <poll.h>
#include <errno.h>
#include <time.h>

#include "syscall.h"
#include "Node.h"

#include "linux/risc1.h"

extern char **environ;

#define RISC1_STACK_SIZE    (4 * 1024)
#define RISC1_ONCD_GRFCPU	0
#define RISC1_ONCD_GCP0		2

typedef struct RegTrace {
    char *name;
    int code;
} RegTrace, *pRegTrace;

static RegTrace sregs[] = {
    {"pc", 0x1ff},
    {"sp", (29 << 3) | RISC1_ONCD_GRFCPU},
};

static char gargs[0x1000];
static char *garg = gargs;
static int debug_enable = 0;
static Node *conf = NULL;
static int trace = 0;
static pRegTrace pRTrace = NULL;
static int rtrace = 0;
static int status_debug = 0;
static int status_always = 0;
static int show_regs = 1;
static uint32_t pcstop = 0;
static uint32_t timeout = -1;
static int tstep = 1000;
static uint32_t catch_mode = 0x00bf0000;
static int map_out = 0;
static int syscall_out = 0;
static int showinfo = 0;

static char *buf = NULL;

static inline uint32_t risc_get_paddr(uint32_t addr)
{
	if (addr >= 0xc0000000)
		return addr;
	if (addr >= 0xa0000000)
		return addr - 0xa0000000;
	return addr & 0x7fffffff;
}

static void checkConfiguration(int id)
{
    struct risc1_caps caps;
    int ret;

    ret = ioctl(id, RISC1_GET_CAPS, &caps);

    if (ret < 0) {
        perror("RISC1_GET_CAPS");
        exit(1);
    }

    if (!showinfo)
        return;

    printf("Name : %s\n", caps.drvname);
    printf("HWID : %08x\n", caps.hw_id);
}

void SetDebugTraceOptions(Node *root)
{
    Node *debug = NodeFind(root, "debug");
    Node *regs = NodeFind(debug, "registers");
    Node *kernel_debug = NodeFind(debug, "kernel_debug");
    Node *reg;
    NodePosition npos;
    int max_trace;
    int code;
    int i;

    if (debug == NULL)
        return;

    status_debug = NodeGetBoolean(debug, "status_debug", 0);
    status_always = NodeGetBoolean(debug, "status_always", 0);
    show_regs = NodeGetBoolean(debug, "show_regs", 1);
    pcstop = NodeGetInteger(debug, "pcstop", 0);
    timeout = NodeGetInteger(debug, "timeout", -1);
    tstep = NodeGetInteger(debug, "tstep", 1000);
    catch_mode = NodeGetInteger(debug, "catch_mode", 0x00bf0100);
    map_out =  NodeGetInteger(debug, "map_out", 0);
    syscall_out = NodeGetInteger(debug, "syscall_out", 0);

    NodeFindStart(kernel_debug, NULL, &npos);
    while (reg = NodeFindNext(&npos)) {
        int value = NodeGetInteger(reg, NULL, 0);
        //char *fname;
        int id, len;

        if (showinfo)
            printf("Set value %d for %s\n", value, reg->element.key);

        asprintf(&buf, "/sys/kernel/debug/risc1/%s", reg->element.key);
        //fname = malloc(sizeof("/sys/kernel/debug/risc1/") + strlen(reg->element.key) + 1);
        //strcat(strcpy(fname, "/sys/kernel/debug/risc1/"), reg->element.key);
        id = open(buf, O_WRONLY);
        len = asprintf(&buf, "%d\n", value);
        write(id, buf, len);
        close(id);
        //free(fname);
    }

    if (!debug_enable && !NodeGetBoolean(debug, "debug_enable", 0))
        return;

    debug_enable = 1;

    if (regs == NULL)
        return;

    max_trace = NodeGetInteger(debug, "max_trace", 256);
    pRTrace = calloc(max_trace, sizeof(pRTrace[0]));
    if (pRTrace == NULL) {
        fprintf(stderr, "Can't allocate trace memory\n");
        exit(1);
    }

    NodeFindStart(regs, NULL, &npos);
    while (reg = NodeFindNext(&npos)) {
        char *name = NodeGetString(reg, NULL, NULL);
        int ok = 0;
        printf("trace register %s\n", name);
        for (i = 0; i < sizeof(sregs)/sizeof(sregs[0]); i++) {
            if (strcmp(sregs[i].name, name))
                continue;

            pRTrace[rtrace].name = name;
            pRTrace[rtrace].code = sregs[i].code;
            rtrace++;
            ok = 1;
            break;
        }
        if (ok) continue;

        if(name[0] == 'r' || name[0] == 'R') {
            pRTrace[rtrace].name = name;
            sscanf(name + 1, "%d", &code);
            printf("regcode %d\n", code);
            code &= 31;
            pRTrace[rtrace].code = RISC1_ONCD_GRFCPU | (code << 3);
            rtrace++;
            continue;
        }

        fprintf(stderr, "Unknown register %s\n", name);
        exit(1);
    }

}

static void readSection(int fid, void *mem, int offset, int size)
{
    int pos, ret;

    pos = lseek(fid, 0, SEEK_CUR);
    if (showinfo)
        fprintf(stderr, "pos = %d\n", pos);

    if (pos < 0) {
        fprintf(stderr, "Can't read section data\n");
        exit(1);
    }

    ret = lseek(fid, offset, SEEK_SET);
    if (showinfo)
        fprintf(stderr, "offset = %d seek = %d\n", offset, ret);

    if (ret != offset) {
        fprintf(stderr, "Can't seek section data read\n");
        exit(1);
    }

    ret = read(fid, mem, size);
    if (showinfo)
        fprintf(stderr, "size = %d ret = %d\n", size, ret);

    if (ret != size) {
        fprintf(stderr, "Can't read section data\n");
        exit(1);
    }

    ret = lseek(fid, pos, SEEK_SET);
    if (showinfo)
        fprintf(stderr, "pos = %d ret = %d\n", pos, ret);

    if (ret != pos) {
        fprintf(stderr, "Can't return after section data read\n");
        exit(1);
    }
}

static void createMapper(int id, struct risc1_buf *rbuf)
{
    if (ioctl(id, RISC1_IOC_CREATE_BUFFER, rbuf)) {
        perror("Can't create section buffer");
        exit(1);
    }
    if (ioctl(id, RISC1_IOC_CREATE_MAPPER, rbuf)) {
        perror("Can't create section mapper");
        exit(1);
    }
    close(rbuf->dmabuf_fd);
}

static void processArg(struct risc1_job_instance *job_instance, const char *arg)
{
    int j = job_instance->argc++;
    int l = strlen(arg);

    job_instance->args[j].basic.p = (__u64)garg;

    if (index(arg, '.') != NULL || index(arg, 'e') != NULL || index(arg, 'E') != NULL) {
        // Float constant
        if (arg[l-1] != 'F' && arg[l-1] != 'f' ) {
            double d;
            sscanf(arg, "%lf", &d);
            printf("arg%d is double %lf\n", j, d);
            memcpy(garg, &d, sizeof(d));
            garg += sizeof(d);
            job_instance->args[2].basic.size = sizeof(d);
        } else {
            float d;
            sscanf(arg, "%f", &d);
            printf("arg%d is float %lf\n", j, d);
            memcpy(garg, &d, sizeof(d));
            garg += sizeof(d);
            job_instance->args[2].basic.size = sizeof(d);
        }
        job_instance->args[j].type = RISC1_TYPE_BASIC_FLOAT;
    } else {
        // Integer constant
        if (arg[l-1] == 'L' || arg[l-1] == 'l' ) {
            long l;
            sscanf(arg, "%li", &l);
            printf("arg%d is long %ld\n", j, l);
            memcpy(garg, &l, sizeof(l));
            garg += sizeof(l);
            job_instance->args[2].basic.size = sizeof(l);
        } else {
            int l;
            sscanf(arg, "%i", &l);
            printf("arg%d is int %d\n", j, l);
            memcpy(garg, &l, sizeof(l));
            garg += sizeof(l);
            job_instance->args[2].basic.size = sizeof(l);
        }
        job_instance->args[j].type = RISC1_TYPE_BASIC;
    }
}

static void processSyscall(int job_instance_fd)
{
    struct stat ret_stat;
    struct stat_compat *stat_compat;
    struct tms ret_tms;
    struct tms_compat *tms_compat;
    struct risc1_message message;
    struct pollfd fds;
    char *risc1_env, *env_buf;
    uint32_t *risc1_env_size, full_needed_size;
    void *tmp = NULL;
    int flags;
    int ret;
    char c;

    ret = read(job_instance_fd, &message, sizeof(struct risc1_message));

    if (ret != sizeof(struct risc1_message)) {
        perror("Read job file failed");
        exit(1);
    }

    if (syscall_out) {
        fprintf(stderr, "Syscall message type %d\n", message.type);
        fprintf(stderr, "Syscall message num %d\n", message.num);
        fprintf(stderr, "Syscall message arg0 %lld\n", message.arg0);
        fprintf(stderr, "Syscall message arg1 %lld 0x%llx\n", message.arg1, message.arg1);
        fprintf(stderr, "Syscall message arg2 %lld\n", message.arg2);
    }

    switch(message.num) {
    case SC_GETTIMEOFDAY:
        message.retval = gettimeofday((struct timeval *)message.arg0,
                                      (struct timezone *) message.arg1);
        break;
    case SC_WRITE:
        ret = write((int) message.arg0, (char *) message.arg1, message.arg2);
        break;
    case SC_READ:
        ret = read((int) message.arg0, (char *) message.arg1, message.arg2);
        break;
    case SC_OPEN:
        flags = 0;
        if (message.arg1 & O_CREAT_COMPAT) flags |= O_CREAT;
        if (message.arg1 & O_EXCL_COMPAT) flags |= O_EXCL;
        if (message.arg1 & O_NOCTTY_COMPAT) flags |= O_NOCTTY;
        if (message.arg1 & O_TRUNC_COMPAT) flags |= O_TRUNC;
        if (message.arg1 & O_APPEND_COMPAT) flags |= O_APPEND;
        if (message.arg1 & O_NONBLOCK_COMPAT) flags |= O_NONBLOCK;
        if (message.arg1 & O_SYNC_COMPAT) flags |= O_SYNC;
        if (message.arg1 & O_RDWR_COMPAT) flags |= O_RDWR;
        if (message.arg1 & O_RDONLY_COMPAT) flags |= O_RDONLY;
        if (message.arg1 & O_WRONLY_COMPAT) flags |= O_WRONLY;
        ret = open((char *) message.arg0, flags, message.arg2);
        break;
    case SC_CLOSE:
        if (message.arg0 > 2)
            ret = close((int) message.arg0);
        else {
            ret = -1;
            errno = EINVAL;
        }
        break;
    case SC_FSTAT:
        ret = fstat((int)message.arg0, &ret_stat);
        stat_compat =  (struct stat_compat *)message.arg1;
        stat_compat->dev = ret_stat.st_dev;
        stat_compat->ino = ret_stat.st_ino;
        stat_compat->mode = ret_stat.st_mode;
        stat_compat->nlink = ret_stat.st_nlink;
        stat_compat->uid = ret_stat.st_uid;
        stat_compat->gid = ret_stat.st_gid;
        stat_compat->rdev = ret_stat.st_rdev;
        stat_compat->size = ret_stat.st_size;
        stat_compat->blksize = ret_stat.st_blksize;
        stat_compat->blocks = ret_stat.st_blocks;
        stat_compat->atime = ret_stat.st_atime;
        stat_compat->mtime = ret_stat.st_mtime;
        stat_compat->ctime = ret_stat.st_ctime;
        break;
    case SC_LSEEK:
        ret = lseek((int)message.arg0, (int) message.arg1, (int)message.arg2);
        break;
    case SC_ISATTY:
        ret = isatty((int) message.arg0);
        break;
    case SC_CHDIR:
        ret = chdir((char *)message.arg0);
        break;
    case SC_STAT:
        ret = stat((const char *)message.arg0, &ret_stat);
        stat_compat =  (struct stat_compat *)(message.arg1);
        stat_compat->dev = ret_stat.st_dev;
        stat_compat->ino = ret_stat.st_ino;
        stat_compat->mode = ret_stat.st_mode;
        stat_compat->nlink = ret_stat.st_nlink;
        stat_compat->uid = ret_stat.st_uid;
        stat_compat->gid = ret_stat.st_gid;
        stat_compat->rdev = ret_stat.st_rdev;
        stat_compat->size = ret_stat.st_size;
        stat_compat->blksize = ret_stat.st_blksize;
        stat_compat->blocks = ret_stat.st_blocks;
        stat_compat->atime = ret_stat.st_atime;
        stat_compat->mtime = ret_stat.st_mtime;
        stat_compat->ctime = ret_stat.st_ctime;
        break;
    case SC_TIMES:
        ret = times(&ret_tms);
        tms_compat = (struct tms_compat *)(message.arg0);
        tms_compat->tms_utime = ret_tms.tms_utime;
        tms_compat->tms_stime = ret_tms.tms_stime;
        tms_compat->tms_cutime = ret_tms.tms_cutime;
        tms_compat->tms_cstime = ret_tms.tms_cstime;
        break;
    case SC_LINK:
        ret = link((char *)message.arg0, (char *)message.arg1);
        break;
    case SC_UNLINK:
        ret = unlink((char *)message.arg0);
        break;
    case SC_GET_ENV:
        risc1_env = (char*)message.arg0;
        risc1_env_size = (uint32_t *)message.arg1;
        if (!risc1_env_size) { ret = -1; errno = EINVAL; break; }
        full_needed_size = 0; env_buf = NULL;
        for (char **host_env = environ; *host_env; host_env++) {
            ret = full_needed_size;
            full_needed_size += strlen(*host_env);
            tmp = realloc(env_buf, full_needed_size + 1);
            if (!tmp)  { ret = -1; errno = EINVAL; break; }
            env_buf = (char *) tmp;
            strcpy(env_buf + ret, *host_env);
            *strchr(env_buf + ret, '=') = '\0';  // replace the first "="
            // Insert env variables separator
            env_buf[full_needed_size] = '\0';
            full_needed_size += 1;
        }
        if (ret == -1) break;
        // Insert '\0' separator for end of line;
        full_needed_size += 1;
        tmp = realloc(env_buf, full_needed_size);
        if (!tmp)  { ret = -1; errno = EINVAL; break; }
        env_buf = (char *) tmp;
        env_buf[full_needed_size - 1] = '\0';
        if (risc1_env) {
            memcpy(risc1_env, env_buf,
                (*risc1_env_size > full_needed_size) ? full_needed_size : *risc1_env_size);
        }
        *risc1_env_size = full_needed_size;
        ret = 0;
        break;
    case EVENT_VCPU_PUTCHAR:
        c = message.arg0;
        putchar(c);
        ret = 0;
        break;
    case EVENT_VCPU_PUTSTR:
        puts((char*)message.arg0);
        ret = 0;
        break;
    default:
        fprintf(stderr, "incorrect syscall number %d\n", message.num);
        exit(1);
    }
    message.retval = (ret < 0) ? -errno : ret;
    message.type = RISC1_MESSAGE_SYSCALL_REPLY;
    ret = write(job_instance_fd, &message, sizeof(struct risc1_message));
    if (ret < 0) {
        perror("syscall reply problem");
        exit(1);
    }
}

static void processArgs(int id, int jobfd, const char **args)
{
    struct risc1_job_instance job_instance;
    struct risc1_job_instance_status status;
    struct risc1_dbg_mem mem;
    struct risc1_dbg_stop_reason sreason;
    uint32_t pcbuf;
    int  ret;
    int  steps;
    int  i;

    job_instance.argc = 0;
    job_instance.launcher_virtual_address = 0;
    job_instance.debug_enable = debug_enable;
    job_instance.entry_point_virtual_address = 0x10000000;

    if (showinfo)
        printf("catch_mode 0x%x\n", catch_mode);

    job_instance.catch_mode = catch_mode;
    job_instance.job_fd = jobfd;
    job_instance.debug_fd = -1;

    strcpy(job_instance.name, args[0]);

    // args[2] ... - arguments
    for (int i = 2; args[i]; i++) {
        processArg(&job_instance, args[i]);
    }

    ret = ioctl(id, RISC1_IOC_ENQUEUE_JOB, &job_instance);
    if (ret != 0) {
        perror("enqueue job failed");
        exit(1);
    }

    status.job_instance_fd = job_instance.job_instance_fd;
    status.state = -1;
    status.error = -1;

    for (;;) {
        struct pollfd fds[2];
        int state = status.state;
        int error = status.error;
        time_t t = time(NULL);
        int nfds;

        fds[0].fd = job_instance.job_instance_fd;
        fds[0].events = POLLIN;
        fds[1].fd = job_instance.debug_fd;
        fds[1].events = POLLIN;
        fds[1].revents = 0;
        nfds = (job_instance.debug_fd < 0) ? 1 : 2;

        for (;;) {
            ret = poll(fds, nfds, tstep);
            if (ret == 0) {
                if (time(NULL) - t >= timeout) {
                    fprintf(stderr, "Timeout !!!\n");
                    exit(1);
                }
                continue;
            }
            if (ret < 0) {
                perror("poll error/timout\n");
                exit(1);
            }
            if ((fds[0].revents | fds[1].revents) & (POLLERR | POLLNVAL)) {
                fprintf(stderr, "poll returned error\n");
                exit(1);
            }
            break;
        }

        ret = ioctl(id, RISC1_IOC_GET_JOB_STATUS, &status);
        if (ret != 0) {
            perror("status query failed\n");
            exit(1);
        }

        if (status_debug && (status_always || state != status.state || error != status.error))
            fprintf(stderr, "error %d, state %d\n", status.error, status.state);

        // Select a source
        if (fds[0].revents & (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM)) {
            switch(state) {
            case RISC1_JOB_STATUS_DONE:
                return;
            case RISC1_JOB_STATUS_SYSCALL:
                if (syscall_out)
                    fprintf(stderr, "risc1run syscall\n");
#if 0
            if (ioctl(job_instance.debug_fd, RISC1_IOC_DBG_GET_STOP_REASON, &sreason) < 0)
            {
                perror("RISC1_IOC_DBG_GET_STOP_REASON");
                exit(1);
            }
            fprintf(stderr, "stop reason %d\n", sreason.reason);
#endif
                processSyscall(job_instance.job_instance_fd);
                break;
            case -1:    /* can be in initial state */
                break;
            default:
                fprintf(stderr, "unknown state for job %d\n", state);
            }
        }

        if (fds[1].revents & (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM)) {
            static int prev_reason = -1;
            switch (state) {
            case RISC1_JOB_STATUS_RUN:
                usleep(1);
                break;
            case RISC1_JOB_STATUS_INTERRUPTED:

                if (ioctl(job_instance.debug_fd, RISC1_IOC_DBG_GET_STOP_REASON, &sreason) < 0) {
                    perror("RISC1_IOC_DBG_GET_STOP_REASON");
                    exit(1);
                }

                if (sreason.reason != prev_reason /*RISC1_STOP_REASON_DBG_INTERRUPT*/) {
                    fprintf(stderr, "stop reason %d\n", sreason.reason);
                    prev_reason = sreason.reason;
                }

                if (sreason.reason == RISC1_STOP_REASON_APP_EXCEPTION)
                    return;

                if (map_out) {
                    ret = ioctl(job_instance.debug_fd, RISC1_IOC_DUMP, RISC1_DUMP_VMMU);
                    if (ret) {
                        perror("map dump");
                        exit(1);
                    }
                    map_out = 0;
                }
                if (show_regs) {
                    /* Output registers */
                    mem.size = 4;
                    mem.data = &pcbuf;
                    for (i = 0; i < rtrace; i++) {
                        // mem.vaddr = 0x1ff; // PC
                        mem.vaddr = pRTrace[i].code;
                        ret = ioctl(job_instance.debug_fd, RISC1_IOC_DBG_REGISTER_READ, &mem);
                        if (ret) {
                            perror("registers read");
                            exit(1);
                        }
                        fprintf(stderr, "%s 0x%08x\n", pRTrace[i].name, pcbuf);
                    }
                }

                if (pcstop != 0) {
                    mem.size = 4;
                    mem.data = &pcbuf;
                    mem.vaddr = 0x1ff; // PC
                    ret = ioctl(job_instance.debug_fd, RISC1_IOC_DBG_REGISTER_READ, &mem);
                    if (ret) {
                        perror("pc registers read");
                        exit(1);
                    }
                    if (pcbuf == pcstop) {
                        ioctl(id, RISC1_IOC_DUMP, RISC1_IOC_DUMP, RISC1_DUMP_ONCD | RISC1_DUMP_REG);
                        exit(0);
                    }
                }

                /* Execute one step */
                steps = 1;
                ret = ioctl(job_instance.debug_fd, RISC1_IOC_DBG_STEP, &steps);
                if (ret) {
                    perror("dbg step");
                    return;
                }
                break;
            case RISC1_STOP_REASON_APP_EXCEPTION:
                return;
            default:
                fprintf(stderr, "unknown state for dbg %d\n", state);
            }
        }
    }
}

static void addConfSections(int id, int *isect, struct risc1_job *job)
{
    Node *memory, *region;
    NodePosition np;
    struct risc1_buf rbuf;
    void *mem;
    int nsect = *isect;

    memory = NodeFind(conf, "memory");
    if (memory == NULL)
        return;

    NodeFindStart(memory, "region", &np);
    while (region = NodeFindNext(&np)) {
        uint32_t addr = NodeGetInteger(region, "addr", 0);
        uint32_t size = NodeGetInteger(region, "size", 0);
        if (size == 0)
            continue;

        if (posix_memalign(&mem, 0x1000, size)) {
            perror("Can't allocate memory for segment");
            exit(1);
        }

        rbuf.type = RISC1_CACHED_BUFFER_FROM_UPTR;
        rbuf.p = (__u64)mem;
        rbuf.size = size;
        createMapper(id, &rbuf);

        job->elf_sections[nsect].mapper_fd = rbuf.mapper_fd;
        job->elf_sections[nsect].type = RISC1_ELF_SECTION_DATA;
        job->elf_sections[nsect].size = size;
        job->elf_sections[nsect].risc1_virtual_address = risc_get_paddr(addr);
        nsect++;
    }

    *isect = nsect;
}

static void loadFile(int id, const char **args)
{
    struct risc1_job job;
    struct risc1_buf rbuf;
    Elf32_Ehdr ehdr;
    Elf32_Shdr shdr;
    Elf32_Phdr phdr;
    void *mem;
    const char *fname = args[1];
    int fid;
    int size;
    int ret;
    int nsect = 0;

    fid = open(fname, O_RDONLY);
    if (fid < 0) {
        perror(fname);
        exit(1);
    }

    size = read(fid, &ehdr, sizeof(ehdr));
    if (size != sizeof(ehdr)) {
        perror("Can't read ELF header");
        exit(1);
    }

    /* Check file type */
    if (strncmp(ehdr.e_ident, ELFMAG, SELFMAG)) {
        fprintf(stderr, "Illegal file type %s\n", fname);
        exit(1);
    }

    if (ehdr.e_machine != EM_MIPS && ehdr.e_machine != EM_MIPS_RS3_LE) {
        fprintf(stderr, "Illegal architecture in the file %s\n", fname);
        exit(1);
    }

    if (lseek(fid, ehdr.e_phoff, SEEK_SET) < 0) {
        perror("Can't read headers");
        exit(1);
    }

    /* Read all pheaders */
    for (int i = 0; i < ehdr.e_phnum; i++) {
        enum risc1_job_elf_section_type type;

        if (read(fid, &phdr, sizeof(phdr)) != sizeof(phdr)) {
            perror("Can't read program header");
            exit(1);
        }

        /* Process header */
        if (showinfo)
            fprintf(stderr, "Header %d has type %d\n", i, phdr.p_type);

        if (phdr.p_type != PT_LOAD)
            continue;

        if (phdr.p_flags & PF_X) {
            type = RISC1_ELF_SECTION_CODE;
        } else if (phdr.p_flags & PF_W) {
            type = RISC1_ELF_SECTION_DATA;
        } else {
            type = RISC1_ELF_SECTION_DATA_CONST;
        }

        if (posix_memalign(&mem, 0x1000, phdr.p_memsz)) {
            perror("Can't allocate memory for segment");
            exit(1);
        }

        /* Read segment */
        readSection(fid, mem, phdr.p_offset, phdr.p_filesz);

        if (phdr.p_filesz != phdr.p_memsz) {
            memset((char*)mem + phdr.p_filesz, 0, phdr.p_memsz - phdr.p_filesz);
        }

        rbuf.type = RISC1_CACHED_BUFFER_FROM_UPTR;
        rbuf.p = (__u64)mem;
        rbuf.size = phdr.p_memsz;
        createMapper(id, &rbuf);

        job.elf_sections[nsect].mapper_fd = rbuf.mapper_fd;
        job.elf_sections[nsect].type = type;
        job.elf_sections[nsect].size = phdr.p_memsz;
        job.elf_sections[nsect].risc1_virtual_address = risc_get_paddr(phdr.p_vaddr);
        nsect++;
    }

    if (conf != NULL) {
        addConfSections(id, &nsect, &job);
    }

    /* Create stack buffer */
    if (posix_memalign(&mem, 0x1000, RISC1_STACK_SIZE)) {
        perror("Can't allocate memory for stack");
        exit(1);
    }
    rbuf.type = RISC1_CACHED_BUFFER_FROM_UPTR;
    rbuf.p = (__u64)mem;
    rbuf.size = RISC1_STACK_SIZE;
    createMapper(id, &rbuf);

    job.stack_fd = rbuf.mapper_fd;

    job.num_elf_sections = nsect;
    job.hugepages = 1;
    if (ioctl(id, RISC1_IOC_CREATE_JOB, &job)) {
        perror("Can't create job");
        exit(1);
    }

    if (showinfo)
        fprintf(stderr, "Job fd is %d\n", job.job_fd);

    processArgs(id, job.job_fd, args);
}

int main(int argc, const char **argv)
{
    int id;

    /* Open device */
    id = open("/dev/risc1", O_RDWR);
    if (id < 0) {
        perror("Can't open /dev/risc1");
        return 1;
    }

    /* Check flags */
    while (argv[1] != NULL && argv[1][0] == '-') {
        switch(argv[1][1]){
        case 'i':
        {
            showinfo = 1;
            argv++;
        }
        break;

        case 's':
        {
            /* Check status */
            int arg = RISC1_DUMP_MAIN;
            if (argv[2] != NULL)
                sscanf(argv[2], "%i", &arg);
            ioctl(id, RISC1_IOC_DUMP, arg);
            goto finish;
        }
        break;

        case 'd':
        {
            debug_enable = 1;
            argv++;
        }
        break;

        case 'c':
        {
            conf = ReadConfiguration(argv[2]);
            SetDebugTraceOptions(conf);
            argv += 2;
        }
        break;

        default:
            fprintf(stderr, "Unknown option %s\n", argv[1]);
        }
    }

    /* Check configuration */
    checkConfiguration(id);

    /* Check file */
    if (argv[1] != NULL) {
        loadFile(id, argv);
    }

finish:
    close(id);
    return 0;
}

