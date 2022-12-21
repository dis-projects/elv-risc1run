#define RISC1_ONCD_GRFCPU	0
#define RISC1_ONCD_GCP0		2

typedef struct Risc1RegTrace {
    char *name;
    int code;
} Risc1RegTrace, *pRisc1RegTrace;

#define RISC1_MAX_SEGMENTS        16

typedef struct Risc1Job {
    int id;
#ifdef RISC1_LIB
	struct {
		void *mem;
		int id;
	} segments[RISC1_MAX_SEGMENTS];
    struct risc1_job job;
#endif
} Risc1Job, *pRisc1Job;

static inline uint32_t risc_get_paddr(uint32_t addr)
{
	if (addr >= 0xc0000000)
		return addr;
	if (addr >= 0xa0000000)
		return addr - 0xa0000000;
	return addr & 0x7fffffff;
}

pRisc1Job risc1NewJob(int id, const char *fname);
int risc1AddSecton(pRisc1Job pRJob, void *mem, uint32_t addr, uint32_t size);
int risc1PrepareJob(pRisc1Job pRJob);
int risc1ProcessArgs(pRisc1Job pRJob, const char **args);
int risc1Open(void);
int risc1Close(int id);

int risc1StartFirmwareList(int id);
int risc1GetNextFirmware(int id, char **next);

