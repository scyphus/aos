/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include <aos/const.h>

#define MAX_PROCESSORS  256

#define IV_IRQ(n)       (0x20 + (n))
#define IV_IRQ0         0x20
#define IV_IRQ1         0x21
#define IV_IRQ2         0x22
#define IV_IRQ3         0x23
#define IV_IRQ4         0x24
#define IV_IRQ5         0x25
#define IV_IRQ6         0x26
#define IV_IRQ7         0x27
#define IV_IRQ8         0x28
#define IV_IRQ9         0x29
#define IV_IRQ10        0x2a
#define IV_IRQ11        0x2b
#define IV_IRQ12        0x2c
#define IV_IRQ13        0x2d
#define IV_IRQ14        0x2e
#define IV_IRQ15        0x2f
#define IV_IRQ32        0x40
#define IV_LOC_TMR      0x50
#define IV_IPI          0x51
#define IV_CRASH        0xfe

#define PAGESIZE        4096





/* tCAM module */
#define LEAF_COMPRESSION        1
#define SKIP_TOP_TIER           2
#define LEAF_INDEX              0

#define BLOCK_SIZE              64
#define BLOCK_SIZE_BIT          6
struct ptcam_block {
    struct {
        int type;
        union {
            struct {
                u64 prefix;
                u32 preflen;
                u64 data;
            } data;
            struct ptcam_block *children;
        } c;
    } blk[BLOCK_SIZE];
};
struct ptcam_node {
#if LEAF_COMPRESSION
    u64 mask;
#endif
    u64 node;
    u32 map0;
    u32 map1;
} __attribute__ ((packed));
struct ptcam {
    /* Original memory */
    struct {
        struct ptcam_block *root;
    } o;
    /* Compressed one */
    struct {
        struct ptcam_node *nodes;
#if LEAF_INDEX
        u16 *indexes;
#endif
        u64 *data;
    } c;
};
int ptcam_add_entry(struct ptcam*, u64, u32, u64);
int ptcam_commit(struct ptcam *);
struct ptcam * ptcam_init(void);
u64 ptcam_lookup(struct ptcam *, u64);
extern struct ptcam *tcam;



struct dxr_next_hop {
    u64 addr;
    struct dxr_next_hop *next;
    /* Temp */
    int idx;
};
struct dxr_range {
    u32 begin;
    u32 end;
    struct dxr_next_hop *nh;
    struct dxr_range *next;
    /* Temp */
    int idx;
};
struct dxr_lookup_table_entry {
    int nr;
    int stype;
};
struct dxr {
    struct {
        struct dxr_range *head;
        struct dxr_range *tail;
    } range;
    struct dxr_next_hop *nhs;

    /* Compiled */
    u32 *lut;
    u8 *rt;
    u64 *nh;
};
u64 dxr_lookup(struct dxr *, u32);
int dxr_commit(struct dxr *);
int dxr_add_range(struct dxr *, u32 , u32 , u64);
extern struct dxr *dxr;



/* TCP */
enum tcp_state {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_RECEIVED,
    TCP_SYN_SENT,
    /* Established */
    TCP_ESTABLISHED,
    /* Active close */
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_CLOSING,
    TCP_TIME_WAIT,
    /* Passive close */
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
};
struct tcp_session {
    enum tcp_state state;
    /* Receive window */
    struct {
        u32 sz;
        u8 *buf;
        /* Sliding window */
        u32 pos0;
        u32 pos1;
    } rwin;
    /* Transmit window */
    struct {
        u32 sz;
        u8 *buf;
        u32 pos0;
        u32 pos1;
    } twin;
    /* local/remote ipaddr/port */
    u32 lipaddr;
    u16 lport;
    u32 ripaddr;
    u16 rport;
    /* seq/ack number */
    u32 seq;
    u32 ack;
    /* MSS */
    u32 mss;
    /* Window */
    u8 wscale;
};

struct ether_hdr {
    u8 dst[6];
    u8 src[6];
    u16 type;
} __attribute__ ((packed));
struct ip_hdr {
    u8 ihl:4;
    u8 version:4;
    u8 diffserv;
    u16 length;
    u16 id;
    u16 offset:13;
    u16 flags:3;
    u8 ttl;
    u8 protocol;
    u16 checksum;
    u32 srcip;
    u32 dstip;
} __attribute__ ((packed));
struct tcp_hdr {
    u16 sport;
    u16 dport;
    u32 seqno;
    u32 ackno;
    u8 flag_ns:1;
    u8 flag_reserved:3;
    u8 offset:4;
    u8 flag_fin:1;
    u8 flag_syn:1;
    u8 flag_rst:1;
    u8 flag_psh:1;
    u8 flag_ack:1;
    u8 flag_urg:1;
    u8 flag_ece:1;
    u8 flag_cwr:1;
    u16 wsize;
    u16 checksum;
    u16 urgptr;
} __attribute__ ((packed));


/* ARP */
struct net_arp_entry {
    u32 protoaddr;
    u64 hwaddr;
    u64 expire;
    int state;
};
struct net_arp_table {
    int sz;
    struct net_arp_entry *entries;
};

/* ND */
struct net_nd_entry {
    u8 neighbor[16];
    u8 linklayeraddr[6];
    u64 expire;
    int state;
};
struct net_nd_table {
    int sz;
    struct net_nd_entry *entries;
};




/* DRIVER */
#define NETDEV_MAX_NAME 32
struct netdev {
    char name[NETDEV_MAX_NAME];
    u8 macaddr[6];

    void *vendor;

    int (*sendpkt)(const u8 *pkt, u32 len, struct netdev *netdev);
    int (*recvpkt)(u8 *pkt, u32 len, struct netdev *netdev);

    /* Bidirectional link */
    struct net_port *port;
};
struct netdev_list {
    struct netdev *netdev;
    struct netdev_list *next;
};
struct net_port {
    /* Bidirectional link */
    struct netdev *netdev;
    /* VLAN bridges */
    struct net_bridge *bridges[4096];
};
/* FDB */
#define NET_FDB_INVAL           0
#define NET_FDB_PORT_DYNAMIC    1
#define NET_FDB_PORT_STATIC     2
#define NET_FDB_IPIF            3
struct net_fdb_entry {
    int type;
    u64 macaddr; /* Stored in the network order */
    union {
        struct net_port *port;
        struct net_ipif *ipif;
    } u;
};
struct net_fdb {
    int nr;
    struct net_fdb_entry *entries;
};
struct net_bridge {
    /* Lower layer information */
    int nr;
    struct net_port **ports; /* FIXME: outgoing VLAN tag/untagged */
    /* Upper layer information */
    int nr_ipif;
    struct net_ipif **ipifs;
    /* FDB */
    struct net_fdb fdb;
};
struct net_ipif {
    u64 mac;
    struct net_bridge *bridge;
    struct net_ipv4 *ipv4;
};
struct net_ipv4 {
    u32 addr; /* Stored in the network order */
    struct net_ipif *ipif;
    struct net_arp_table arp;
};
struct net_router {
    int nr;
    struct net_ipv4 **ipifs;
};

struct net {
    int sys_mtu;

    /*void *code;*/

    //struct netdev_list *devs;
    //struct net_bridge bridge;
};





struct peth {
    char name[NETDEV_MAX_NAME];
    u8 macaddr[6];

    void *vendor;
    int (*sendpkt)(u8 *pkt, u32 len, struct peth *peth);
    int (*recvpkt)(u8 *pkt, u32 len, struct peth *peth);
};

struct veth {
    struct peth *parent;
    u8 macaddr[6];
    int vlan;
};


void rng_init(void);
void rng_stir(void);
u32 rng_random(void);



#define TASK_POLICY_KERNEL      0
#define TASK_POLICY_DRIVER      1
#define TASK_POLICY_USER        3
#define TASK_TABLE_SIZE         0x10000
#define TASK_KSTACK_SIZE        4096
#define TASK_USTACK_SIZE        4096 * 0x10
#define TASK_STACK_GUARD        16
#define TASK_QUEUE_LEN          0x10
void task_restart(void);
void halt(void);



/*
 * Inter-task message
 */
#define KMSG_TYPE_SIGNAL        1
#define KMSG_TYPE_DATA          2
struct kmsg {
    u8 type;
    union {
        u8 msg;
        struct {
            void *buf;
            int n;
        } data;
    } u;
};


#define TASK_STATE_READY        1
#define TASK_STATE_RUNNING      2
#define TASK_STATE_BLOCKED      3

/*
 * Task
 */
struct ktask {
    /* Task ID */
    u64 id;
    const char *name;

    /* Main routine */
    int (*main)(int argc, char *argv[]);
    char **argv;

    /* For scheduler */
    int pri;
    int cred;

    /* State */
    int state;

    /* Return code */
    int ret;

    /* Archtecture-specific structure */
    void *arch;

    /* Message */
    struct kmsg msg;
};

/*
 * Task queue for scheduler
 */
struct ktask_queue_entry {
    struct ktask *ktask;
    struct ktask_queue_entry *next;
};
struct ktask_queue {
    struct ktask_queue_entry *head;
    struct ktask_queue_entry *tail;
};

/*
 * Task table
 */
struct ktask_table {
    /* Kernel tasks */
    struct ktask *tasks[TASK_TABLE_SIZE];
};

/*
 * Semaphore
 */
struct semaphore {
    int lock;
    int count;
    struct ktask_queue waiting;
};

/*
 * Interrupt handler
 */
struct interrupt_handler {
    void (*handler)(int, void *);
    void *user;
};
#define IRQ_MAX 63
struct interrupt_handler irq_handler_table[IRQ_MAX+1];


/*
 * Processor
 */
#define PROCESSOR_BSP 1
#define PROCESSOR_AP_TICKFULL 2
#define PROCESSOR_AP_TICKLESS 3
struct processor {
    /* Processor ID */
    u8 id;
    /* Processor type */
    u8 type;
    /* Idle task */
    struct ktask *idle;
};
struct processor_table {
    int n;
    struct processor *prs;
    u8 map[MAX_PROCESSORS];
};

/*
 * Kernel memory management
 */
struct kmem_slab_page_hdr {
    struct kmem_slab_page_hdr *next;
    u64 reserved;
} __attribute__ ((packed));

struct kmem_slab_obj {
    void *addr;
} __attribute__ ((packed));


/*
 * Slab objects
 *  slab_hdr
 *    object 0
 *    object 1
 *    ...
 */
struct kmem_slab {
    struct kmem_slab *next;
    int nr;
    int nused;
    int free;
    void *obj_head;
    /* Free pointer follows (nr byte) */
    u8 marks[1];
    /* Objects follows */
} __attribute__ ((packed));



struct kmem_slab_free_list {
    struct kmem_slab *partial;
    struct kmem_slab *full;
    struct kmem_slab *free;
} __attribute__ ((packed));

struct kmem_slab_root {
    /* Generic slabs */
    struct kmem_slab_free_list gslabs[8];
    /* Large objects */
    //struct kmem_slab_obj *lobj;
} __attribute__ ((packed));


/*
 * Call out queue
 */
struct callout_entry {
    u64 fire_clock;
    void (*func)(u64 x);
    struct callout_entry *next;
};
struct callout_queue {
    struct callout_entry *ent;
    struct callout_queue *next;
} __attribute__ ((packed));
struct callout_queue callout[100];


typedef __builtin_va_list va_list;
#define va_start(ap, last)      __builtin_va_start((ap), (last))
#define va_arg                  __builtin_va_arg
#define va_end(ap)              __builtin_va_end(ap)
#define va_copy(dest, src)      __builtin_va_copy((dest), (src))
#define alloca(size)            __builtin_alloca((size))


/* in kernel.c */
int kprintf(const char *, ...);
int kvprintf(const char *, va_list);
void panic(const char *);
struct ktask * ktask_alloc(int);
int ktask_kernel_main(int argc, char *argv[]);
int ktask_idle_main(int argc, char *argv[]);

void syscall_init(void);


/* in util.c */
int kstrcmp(const char *, const char *);
int kstrncmp(const char *, const char *, int);
int kmemcmp(const u8 *, const u8 *, int);
void * kmemcpy(void *, const void *, u64);
void * kmemset(void *, int, u64);
void kmem_init(void);
void * kmalloc(u64);
void kfree(void *);
int kstrlen(const char *);
char * kstrdup(const char *);

int register_irq_handler(int, void (*)(int, void *), void *);

/* in task.c */
int ktask_init(void);
int sched_init(void);
void sched(void);
int sched_ktask_enqueue(struct ktask_queue_entry *);
struct ktask_queue_entry * sched_ktask_dequeue(void);
struct ktask_queue_entry * ktask_queue_entry_new(struct ktask *);
int ktask_fork_execv(int, int (*)(int, char *[]), char **);

/* in processor.c */
int processor_init(void);
struct processor * processor_this(void);


/* in shell.c */
int proc_shell(int, char *[]);
/* in router.c */
void proc_router(void);


/* Architecture-dependent functions in arch.c */
void arch_init(void);
void arch_bsp_init(void);
void arch_ap_init(void);

void arch_putc(int);
void arch_busy_usleep(u64);
void arch_crash(void);

void arch_poweroff(void);

void arch_spin_lock(volatile int *);
void arch_spin_unlock(volatile int *);
void arch_spin_lock_intr(volatile int *);
void arch_spin_unlock_intr(volatile int *);

u8 arch_inb(u16);

void arch_idle(void);

void * arch_memcpy(void *, const void *, u64);

void * arch_alloc_task(struct ktask *, void (*entry)(struct ktask *), int);
void arch_free_task(void *);
struct ktask * arch_get_current_task(void);
int arch_set_next_task(struct ktask *);
int arch_set_next_task_other_cpu(struct ktask *, int);
struct ktask * arch_get_next_task(void);

int arch_cpu_active(u16);

void arch_scall(u64 nr);

void arch_disable_interrupts(void);
void arch_enable_interrupts(void);

void syscall_setup(void);

/* Clock and timer */
void arch_clock_update(void);
u64 arch_clock_get(void);

/* Physical memory management */
int phys_mem_wire(void *, u64);
void * phys_mem_alloc_pages(u64);
void phys_mem_free_pages(void *);


/* in system.c */
#include <aos/types.h>
int open(const char *, int);
ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);



#endif /* _KERNEL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
