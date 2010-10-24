typedef void (*sched_func)(void);

void rb(int bit,uint8_t n,uint8_t o,char *name,char *on, char *off);
void rb0(int bit,uint8_t n,char *name,char *on, char *off);
void rl(int bit,uint8_t n,char *name);

void schedule(uint64_t after, sched_func f);

void intr(uint8_t a);

typedef uint8_t (*io_read_func)(uint8_t a);
typedef void (*io_write_func)(uint8_t a,uint8_t x);

void add_ior(uint8_t a,io_read_func io_read_funcs);
void add_iow(uint8_t a,io_write_func io_write_funcs);

#define ADD_IOR(x) add_ior(0x ## x,avr_IOR ## x);
#define ADD_IOW(x) add_iow(0x ## x,avr_IOW ## x);
#define ADD_IORW(x) ADD_IOR(x); ADD_IOW(x);

int getI();
