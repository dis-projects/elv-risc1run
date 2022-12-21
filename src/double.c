#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/ioctl.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

//#include "Node.h"
#include "risc1lib.h"
#include "elcorelib.h"

void risc1LoadFile(int id, const char **args)
{
    pRisc1Job pRJob = risc1NewJob(id, args[1]);
    int ret;

    if (pRJob == NULL) {
        perror("Can't create job");
        exit(1);
    }

    //if (risc1_conf != NULL && addConfSections(pRJob)) {
    //    perror("Can't add sections");
    //    exit(1);
    //}

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

void elcoreLoadFile(int id, const char **args)
{
    pElcoreJob pRJob = elcoreNewJob(id, args[1]);
    int ret;

    if (pRJob == NULL) {
        perror("Can't create job");
        exit(1);
    }

    //if (risc1_conf != NULL && addConfSections(pRJob)) {
    //    perror("Can't add sections");
    //    exit(1);
    //}

    if (elcorePrepareJob(pRJob)) {
        perror("Can't prepare job");
        exit(1);
    }

    ret = elcoreProcessArgs(pRJob, args);
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
    int rid, eid;

    /* Open devices */
    rid = risc1Open();
    if (rid < 0) {
        perror("Can't open RISC1");
        return 1;
    }

    eid = elcoreOpen(0);
    if (eid < 0) {
        perror("Can't open ELCORE");
        return 1;
    }

    //risc1LoadFile(rid, argv);
    elcoreLoadFile(eid, argv);

    return 0;
}
