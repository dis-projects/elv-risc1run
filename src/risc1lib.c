#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <elf.h>

#define RISC1_LIB

#include "syscall.h"
#include "linux/risc1.h"
#include "risc1lib.h"
#include "Node.h"

#define RISC1_STACK_SIZE    (4 * 1024)

extern char **environ;
extern int risc1Bind(int id);

int risc1_rmode = 0; // 0 - risc1, 1 - risc1-rproc
int risc1_showinfo = 0;
int risc1_syscall_out = 0;
int risc1_debug_enable = 0;
int risc1_rtrace = 0;
int risc1_show_regs = 0;
int risc1_map_out = 0;
int risc1_status_always = 0;
int risc1_status_debug = 0;
int risc1_tstep = 1000;
uint32_t risc1_pcstop = 0;
uint32_t risc1_timeout = -1;
uint32_t risc1_catch_mode = 0x00bb0003;
pRisc1RegTrace pRisc1Trace = NULL;
Node *risc1_conf = NULL;

static char gargs[0x1000];
static char *garg = gargs;

static int risc1ProcessSyscall(int job_instance_fd)
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
        return -1;
    }

    if (risc1_syscall_out) {
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
    case SC_EXIT:
        return 1;
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
        return -1;
    }
    message.retval = (ret < 0) ? -errno : ret;
    message.type = RISC1_MESSAGE_SYSCALL_REPLY;
    ret = write(job_instance_fd, &message, sizeof(struct risc1_message));
    if (ret < 0) {
        perror("syscall reply problem");
        return -1;
    }

    return 0;
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

int risc1ProcessArgs(pRisc1Job pRJob, const char **args)
{
    struct risc1_job_instance job_instance;
    struct risc1_job_instance_status status;
    struct risc1_dbg_mem mem;
    struct risc1_dbg_stop_reason sreason;
    int id = pRJob->id;
    int jobfd = pRJob->job.job_fd;
    uint32_t pcbuf;
    int  ret;
    int  steps;
    int  i;

    job_instance.argc = 0;
    job_instance.launcher_virtual_address = 0;
    job_instance.debug_enable = risc1_debug_enable;
    job_instance.entry_point_virtual_address = 0x10000000;

    if (risc1_showinfo)
        printf("catch_mode 0x%x\n", risc1_catch_mode);

    job_instance.catch_mode = risc1_catch_mode;
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
        return -1;
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
            ret = poll(fds, nfds, risc1_tstep);
            if (ret == 0) {
                if (time(NULL) - t >= risc1_timeout) {
                    fprintf(stderr, "Timeout !!!\n");
                    return -1;
                }
                continue;
            }
            if (ret < 0) {
                perror("poll error/timout\n");
                return -1;
            }
            if ((fds[0].revents | fds[1].revents) & (POLLERR | POLLNVAL)) {
                fprintf(stderr, "poll returned error\n");
                return -1;
            }
            break;
        }

        ret = ioctl(id, RISC1_IOC_GET_JOB_STATUS, &status);
        if (ret != 0) {
            perror("status query failed\n");
            return -1;
        }

        if (risc1_status_debug
                && (risc1_status_always || state != status.state
                     || error != status.error))
            fprintf(stderr, "error %d, state %d\n", status.error, status.state);

        // Select a source
        if (fds[0].revents & (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM)) {
            switch(state) {
            case RISC1_JOB_STATUS_DONE:
                return 0;
            case RISC1_JOB_STATUS_SYSCALL:
                if (risc1_syscall_out)
                    fprintf(stderr, "risc1run syscall\n");
                ret = risc1ProcessSyscall(job_instance.job_instance_fd);
                if (ret)
                    return ret;
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
                    return -1;
                }

                if (sreason.reason != prev_reason /*RISC1_STOP_REASON_DBG_INTERRUPT*/) {
                    fprintf(stderr, "stop reason %d\n", sreason.reason);
                    prev_reason = sreason.reason;
                }

                if (sreason.reason == RISC1_STOP_REASON_APP_EXCEPTION)
                    return -1;

                if (risc1_map_out) {
                    ret = ioctl(job_instance.debug_fd, RISC1_IOC_DUMP, RISC1_DUMP_VMMU);
                    if (ret) {
                        perror("map dump");
                        return -1;
                    }
                    risc1_map_out = 0;
                }
                if (risc1_show_regs) {
                    /* Output registers */
                    mem.size = 4;
                    mem.data = &pcbuf;
                    for (i = 0; i < risc1_rtrace; i++) {
                        // mem.vaddr = 0x1ff; // PC
                        mem.vaddr = pRisc1Trace[i].code;
                        ret = ioctl(job_instance.debug_fd, RISC1_IOC_DBG_REGISTER_READ, &mem);
                        if (ret) {
                            perror("registers read");
                            return -1;
                        }
                        fprintf(stderr, "%s 0x%08x\n", pRisc1Trace[i].name, pcbuf);
                    }
                }

                if (risc1_pcstop != 0) {
                    mem.size = 4;
                    mem.data = &pcbuf;
                    mem.vaddr = 0x1ff; // PC
                    ret = ioctl(job_instance.debug_fd, RISC1_IOC_DBG_REGISTER_READ, &mem);
                    if (ret) {
                        perror("pc registers read");
                        return -1;
                    }
                    if (pcbuf == risc1_pcstop) {
                        ioctl(id, RISC1_IOC_DUMP, RISC1_IOC_DUMP, RISC1_DUMP_ONCD | RISC1_DUMP_REG);
                        return 0;
                    }
                }

                /* Execute one step */
                steps = 1;
                ret = ioctl(job_instance.debug_fd, RISC1_IOC_DBG_STEP, &steps);
                if (ret) {
                    perror("dbg step");
                    return -1;
                }
                break;
            case RISC1_STOP_REASON_APP_EXCEPTION:
                return -1;
            default:
                fprintf(stderr, "unknown state for dbg %d\n", state);
            }
        }
    }
    return 0;
}

static int createMapper(int id, struct risc1_buf *rbuf)
{
    if (ioctl(id, RISC1_IOC_CREATE_BUFFER, rbuf)) {
        perror("Can't create section buffer");
        return -1;
    }
    if (ioctl(id, RISC1_IOC_CREATE_MAPPER, rbuf)) {
        perror("Can't create section mapper");
        return -1;
    }
    close(rbuf->dmabuf_fd);

    return 0;
}

int risc1AddSecton(pRisc1Job pRJob, void *amem, uint32_t addr, uint32_t size)
{
    struct risc1_buf rbuf;
    struct risc1_job_elf_section *esect
        = &pRJob->job.elf_sections[pRJob->job.num_elf_sections];
    void *mem = amem;

    if (mem == NULL && posix_memalign(&mem, 0x1000, size)) {
        return -1;
    }

    rbuf.type = RISC1_CACHED_BUFFER_FROM_UPTR;
    rbuf.p = (__u64)mem;
    rbuf.size = size;
    if (createMapper(pRJob->id, &rbuf))
        return -1;

    esect->mapper_fd = rbuf.mapper_fd;
    esect->type = RISC1_ELF_SECTION_DATA;
    esect->size = size;
    esect->risc1_virtual_address = risc_get_paddr(addr);

    pRJob->job.num_elf_sections++;

    return 0;
}

static pRisc1Job freeJob(pRisc1Job pRJob, int nsect)
{
    int i;

    for (i = 0; i < nsect; i++) {
        close(pRJob->segments[i].id);
        free(pRJob->segments[i].mem);
    }
    free(pRJob);
    return NULL;
}

int risc1PrepareJob(pRisc1Job pRJob)
{
    struct risc1_buf rbuf;
    void *mem;

    /* Create stack buffer */
    if (posix_memalign(&mem, 0x1000, RISC1_STACK_SIZE)) {
        perror("Can't allocate memory for stack");
        freeJob(pRJob, pRJob->job.num_elf_sections);
        return -1;
    }
    rbuf.type = RISC1_CACHED_BUFFER_FROM_UPTR;
    rbuf.p = (__u64)mem;
    rbuf.size = RISC1_STACK_SIZE;
    if (createMapper(pRJob->id, &rbuf)) {
        freeJob(pRJob, pRJob->job.num_elf_sections);
        free(mem);
        return -1;
    }

    pRJob->job.stack_fd = rbuf.mapper_fd;

    if (ioctl(pRJob->id, RISC1_IOC_CREATE_JOB, &pRJob->job)) {
        perror("Can't create job");
        close(rbuf.mapper_fd);
        freeJob(pRJob, pRJob->job.num_elf_sections);
        free(mem);
        return -1;
    }

    if (risc1_showinfo)
        fprintf(stderr, "Job fd is %d\n", pRJob->job.job_fd);

    return 0;
}

static int readSegment(int fid, void *mem, int offset, int size)
{
    int pos, ret;

    pos = lseek(fid, 0, SEEK_CUR);
    if (risc1_showinfo)
        fprintf(stderr, "pos = %d\n", pos);

    if (pos < 0) {
        fprintf(stderr, "Can't read section data\n");
        return -1;
    }

    ret = lseek(fid, offset, SEEK_SET);
    if (risc1_showinfo)
        fprintf(stderr, "offset = %d seek = %d\n", offset, ret);

    if (ret != offset) {
        fprintf(stderr, "Can't seek section data read\n");
        return -1;
    }

    ret = read(fid, mem, size);
    if (risc1_showinfo)
        fprintf(stderr, "size = %d ret = %d\n", size, ret);

    if (ret != size) {
        fprintf(stderr, "Can't read section data\n");
        return -1;
    }

    ret = lseek(fid, pos, SEEK_SET);
    if (risc1_showinfo)
        fprintf(stderr, "pos = %d ret = %d\n", pos, ret);

    if (ret != pos) {
        fprintf(stderr, "Can't return after section data read\n");
        return -1;
    }

    return 0;
}

pRisc1Job risc1NewJob(int id, const char *fname)
{
    struct risc1_job *job;
    struct risc1_buf rbuf;
    Elf32_Ehdr ehdr;
    Elf32_Shdr shdr;
    Elf32_Phdr phdr;
    void *mem;
    pRisc1Job pRJob;
    int fid = -1;
    int size;
    int ret;
    int nsect = 0;

    pRJob = calloc(1, sizeof(pRJob[0]));
    if (pRJob == NULL)
        return pRJob;

    pRJob->id = id;
    job = &pRJob->job;
    fid = open(fname, O_RDONLY);
    if (fid < 0) {
        perror(fname);
        goto error;
    }

    size = read(fid, &ehdr, sizeof(ehdr));
    if (size != sizeof(ehdr)) {
        perror("Can't read ELF header");
        goto error;
    }

    /* Check file type */
    if (strncmp(ehdr.e_ident, ELFMAG, SELFMAG)) {
        fprintf(stderr, "Illegal file type %s\n", fname);
        goto error;
    }

    if (ehdr.e_machine != EM_MIPS && ehdr.e_machine != EM_MIPS_RS3_LE) {
        fprintf(stderr, "Illegal architecture in the file %s\n", fname);
        goto error;
    }

    if (lseek(fid, ehdr.e_phoff, SEEK_SET) < 0) {
        perror("Can't read headers");
        goto error;
    }

    if (ehdr.e_phnum > RISC1_MAX_SEGMENTS) {
        fprintf(stderr, "Too many segments %d\n", ehdr.e_phnum);
        goto error;
    }

    /* Read all pheaders */
    for (int i = 0; i < ehdr.e_phnum; i++) {
        enum risc1_job_elf_section_type type;

        if (read(fid, &phdr, sizeof(phdr)) != sizeof(phdr)) {
            perror("Can't read program header");
            goto error;
        }

        /* Process header */
        if (risc1_showinfo)
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
            goto error;
        }

        /* Read segment */
        if (readSegment(fid, mem, phdr.p_offset, phdr.p_filesz)) {
            free(mem);
            close(fid);
            return freeJob(pRJob, nsect);
        }

        if (phdr.p_filesz != phdr.p_memsz) {
            memset((char*)mem + phdr.p_filesz, 0, phdr.p_memsz - phdr.p_filesz);
        }

        rbuf.type = RISC1_CACHED_BUFFER_FROM_UPTR;
        rbuf.p = (__u64)mem;
        rbuf.size = phdr.p_memsz;
        if (createMapper(id, &rbuf)) {
            free(mem);
            close(fid);
            return freeJob(pRJob, nsect);
        }
        job->elf_sections[nsect].mapper_fd = rbuf.mapper_fd;
        job->elf_sections[nsect].type = type;
        job->elf_sections[nsect].size = phdr.p_memsz;
        job->elf_sections[nsect].risc1_virtual_address = risc_get_paddr(phdr.p_vaddr);
        nsect++;
    }

    job->num_elf_sections = nsect;
    job->hugepages = 1;
    close(fid);
    return pRJob;
error:
    free(pRJob);
    close(fid);
    return NULL;
}

int risc1Open(void)
{
    int id = open("/dev/risc1", O_RDWR);
    if (id >= 0)
        return id;

    id = open("/dev/rrisc1", O_RDWR);
    if (id >= 0) {
        risc1_rmode = 1;
        fprintf(stderr, "Select risc1-rproc mode\n");
        id = risc1Bind(id);
    }

    return id;
}

int risc1Close(int id)
{
    return close(id);
}
