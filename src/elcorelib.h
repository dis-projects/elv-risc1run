#define ELCORE50_MAX_SEGMENTS        16

typedef struct ElcoreJob {
    int id;
#ifdef ELCORE_LIB
	struct {
		void *mem;
		int id;
	} segments[ELCORE50_MAX_SEGMENTS];
    struct elcore50_job job;
#endif
} ElcoreJob, *pElcoreJob;

pElcoreJob ElcoreNewJob(int id, const char *fname);
int elcoreAddSecton(pElcoreJob pRJob, void *mem, uint32_t addr, uint32_t size);
int elcorePrepareJob(pElcoreJob pRJob);
int elcoreProcessArgs(pElcoreJob pRJob, const char **args);
int elcoreOpen(void);
int elcoreClose(int id);