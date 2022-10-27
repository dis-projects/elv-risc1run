#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/ioctl.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "Node.h"
#include "risc1lib.h"

#include "linux/risc1.h"

static Risc1RegTrace sregs[] = {
    {"pc", 0x1ff},
    {"sp", (29 << 3) | RISC1_ONCD_GRFCPU},
};

static int trace = 0;

extern int risc1_showinfo;
extern int risc1_tstep;
extern int risc1_status_debug;
extern int risc1_map_out;
extern int risc1_show_regs;
extern int risc1_rtrace;
extern int risc1_debug_enable;
extern int risc1_status_always;
extern int risc1_syscall_out;
extern uint32_t risc1_pcstop;
extern uint32_t risc1_timeout;
extern uint32_t risc1_catch_mode;
extern pRisc1RegTrace pRisc1Trace;
extern Node *risc1_conf;

static char *buf = NULL;

static void checkConfiguration(int id)
{
    struct risc1_caps caps;
    int ret;

    ret = ioctl(id, RISC1_GET_CAPS, &caps);

    if (ret < 0) {
        perror("RISC1_GET_CAPS");
        exit(1);
    }

    if (!risc1_showinfo)
        return;

    printf("Name : %s\n", caps.drvname);
    printf("HWID : %08x\n", caps.hw_id);
}

static void SetDebugTraceOptions(Node *root)
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

    risc1_status_debug = NodeGetBoolean(debug, "status_debug", 0);
    risc1_status_always = NodeGetBoolean(debug, "status_always", 0);
    risc1_show_regs = NodeGetBoolean(debug, "show_regs", 1);
    risc1_pcstop = NodeGetInteger(debug, "pcstop", 0);
    risc1_timeout = NodeGetInteger(debug, "timeout", -1);
    risc1_tstep = NodeGetInteger(debug, "tstep", 1000);
    risc1_catch_mode = NodeGetInteger(debug, "catch_mode", 0x00bb0003);
    risc1_map_out =  NodeGetInteger(debug, "map_out", 0);
    risc1_syscall_out = NodeGetInteger(debug, "syscall_out", 0);

    NodeFindStart(kernel_debug, NULL, &npos);
    while (reg = NodeFindNext(&npos)) {
        int value = NodeGetInteger(reg, NULL, 0);
        //char *fname;
        int id, len;

        if (risc1_showinfo)
            printf("Set value %d for %s\n", value, reg->element.key);

        asprintf(&buf, "/sys/kernel/debug/risc1/%s", reg->element.key);
        id = open(buf, O_WRONLY);
        len = asprintf(&buf, "%d\n", value);
        write(id, buf, len);
        close(id);
    }

    if (!risc1_debug_enable && !NodeGetBoolean(debug, "debug_enable", 0))
        return;

    risc1_debug_enable = 1;

    if (regs == NULL)
        return;

    max_trace = NodeGetInteger(debug, "max_trace", 256);
    pRisc1Trace = calloc(max_trace, sizeof(pRisc1Trace[0]));
    if (pRisc1Trace == NULL) {
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

            pRisc1Trace[risc1_rtrace].name = name;
            pRisc1Trace[risc1_rtrace].code = sregs[i].code;
            risc1_rtrace++;
            ok = 1;
            break;
        }
        if (ok) continue;

        if(name[0] == 'r' || name[0] == 'R') {
            pRisc1Trace[risc1_rtrace].name = name;
            sscanf(name + 1, "%d", &code);
            printf("regcode %d\n", code);
            code &= 31;
            pRisc1Trace[risc1_rtrace].code = RISC1_ONCD_GRFCPU | (code << 3);
            risc1_rtrace++;
            continue;
        }

        fprintf(stderr, "Unknown register %s\n", name);
        exit(1);
    }

}

static int addConfSections(pRisc1Job pRJob)
{
    Node *memory, *region;
    NodePosition np;

    memory = NodeFind(risc1_conf, "memory");
    if (memory == NULL)
        return 0;

    NodeFindStart(memory, "region", &np);
    while (region = NodeFindNext(&np)) {
        uint32_t addr = NodeGetInteger(region, "addr", 0);
        uint32_t size = NodeGetInteger(region, "size", 0);
        if (size == 0)
            continue;

        if (risc1AddSecton(pRJob, NULL, addr, size))
            return -1;
    }

    return 0;
}

void risc1LoadFile(int id, const char **args)
{
    pRisc1Job pRJob = risc1NewJob(id, args[1]);
    int ret;

    if (pRJob == NULL) {
        perror("Can't create job");
        exit(1);
    }

    if (risc1_conf != NULL && addConfSections(pRJob)) {
        perror("Can't add sections");
        exit(1);
    }

    if (risc1PrepareJob(pRJob)) {
        perror("Can't prepare job");
        exit(1);
    }

    ret = risc1ProcessArgs(pRJob, args);
    switch(ret) {
    case 1:
        fprintf(stderr, "Exit\n");
        break;
    default:
        fprintf(stderr, "Exception\n");
        break;
    }
}

int main(int argc, const char **argv)
{
    int id;

    /* Open device */
    id = risc1Open();
    if (id < 0) {
        perror("Can't open RISC1");
        return 1;
    }

    /* Check flags */
    while (argv[1] != NULL && argv[1][0] == '-') {
        switch(argv[1][1]){
        case 'i':
        {
            risc1_showinfo = 1;
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
            risc1_debug_enable = 1;
            argv++;
        }
        break;

        case 'c':
        {
            risc1_conf = ReadConfiguration(argv[2]);
            SetDebugTraceOptions(risc1_conf);
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

        risc1LoadFile(id, argv);
    }

finish:
    risc1Close(id);
    return 0;
}
