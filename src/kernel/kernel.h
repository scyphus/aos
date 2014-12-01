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
#define IV_TASK_EVENT   0x40
#define IV_LOC_TMR      0x50
#define IV_IPI          0x51
#define IV_CRASH        0xfe

#define PAGESIZE        4096


#define SYSCALL_MAX_NR  0x10
#define SYSCALL_HLT     0x01
#define SYSCALL_READ    0x02
#define SYSCALL_WRITE   0x03


#if 0
struct arch_call_set {
    int (*get_this_cpu)(void);
    void (*crash)(void);
    struct ktask * (*get_current_task)(void);
    int (*set_next_task)(struct ktask *);
    int (*set_next_task_other_cpu)(struct ktask *, int);
    struct ktask * (*get_next_task)(void);
};
struct kernel_call_set {
    void (*tick)(void);
};
#endif

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
/* TCP options */
#define TCP_OPT_EOL             0
#define TCP_OPT_NOP             1
#define TCP_OPT_MSS             2
#define TCP_OPT_WSCALE          3
#define TCP_OPT_SACK_PERMITTED  4
#define TCP_OPT_SACK            5
#define TCP_OPT_TIMESTAMP       8
#define TCP_OPT_ALT_CS_REQ      14
#define TCP_OPT_ALT_CS_DATA     15
/* TCP session */
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
    u32 rseqno;
    u32 rackno;
    /* Received window size */
    u16 rcvwin;
    /* MSS */
    u32 mss;
    /* Window */
    u8 wscale;

    /* Payload handler */
    int (*recv)(struct tcp_session *sess, const u8 *pkt, u32 len);
    int (*send)(struct tcp_session *sess, const u8 *pkt, u32 len);
    struct net *net;
    struct net_stack_chain_next *tx;
    /* PAPP */
    void * (*palloc)(struct tcp_session *sess, int sz, int *asz);
    void (*pfree)(struct tcp_session *sess, void *p);

    /* For optimization */
    //u8 *ackpkt;
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
struct tcp_phdr4 {
    u32 saddr;
    u32 daddr;
    u8 zeros;
    u8 proto;
    u16 tcplen;
    u16 sport;
    u16 dport;
    u32 seqno;
    u32 ackno;
    u8 rsvd:4;
    u8 offset:4;
    u8 flags;
    u16 wsize;
    u16 checksum;
    u16 urgptr;
};


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


#define NET_PAPP_ENOERROR       0
#define NET_PAPP_ENOARPENT      10
#define NET_PAPP_ENORIBENT      11
#define NET_PAPP_ERAND          12



struct net_papp_meta_host_port_ip {
    /* Main parameter */
    struct net_port_host *hport;
    /* Additional parameters */
    u32 saddr;
    u32 daddr;
    int flags;
    int proto;
    /* Write-back parameters */
    u64 param0;
    u64 param1;
};
struct net_papp_meta_ip {
    u32 saddr;
    u32 daddr;
    int flags;
    int proto;
};
/* Write-back */
struct net_papp_status {
    int errno;
    u64 param0;
    u64 param1;
};
/* PAPP context */
struct net_papp_ctx
{
    struct net *net;
    void *data;
    int (*alloc)(struct net_papp_ctx *, u8 *, struct net_papp_status *);
    int (*xmit)(struct net_papp_ctx *, u8 *, int, u8 *, int);
};
struct net_papp_ctx_data_tcp {
    struct tcp_session *sess;
    struct net_papp_ctx *ulayctx;
};

struct netsc_papp {
    /* Queue length */
    int len; /* in bytes (must be 2^n) */
    int wrap; /* len - 1 */

    /* Packet buffer */
    struct {
        u64 base; /* Buffer exposed to the user context */
        int sz;
    } pkt;
    /* Header buffer */
    struct {
        u64 base; /* Buffer not exposed to the user context */
        int sz;
        /* Offsets */
        int *off;
    } hdr;
    /* Ring buffer */
    struct {
        int *desc;
        int head;
        int tail;
    } ring;
};

/* DRIVER */
#define NETDEV_MAX_NAME 32
struct netdev {
    char name[NETDEV_MAX_NAME];
    u8 macaddr[6];

    void *vendor;

    /* for per packet processing */
    int (*sendpkt)(const u8 *pkt, u32 len, struct netdev *netdev);
    int (*recvpkt)(u8 *pkt, u32 len, struct netdev *netdev);

    /* Stack chain */
    int (*papp)(void);

    /* Bidirectional link */
    struct net_port *port;
};
struct netdev_list {
    struct netdev *netdev;
    struct netdev_list *next;
};
/* Router */
struct net_rib4_entry {
    u32 prefix;
    int length;
    u32 nexthop;
    /* scope/srcport? */
};
struct net_rib4 {
    int nr;
    struct net_rib4_entry *entries;
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
        struct net_stack_chain_next *sc;
    } u;
};
struct net_fdb {
    int nr;
    struct net_fdb_entry *entries;
};


/*
 * Global network structure
 */
struct net {
    int sys_mtu;
    /*void *code;*/
    //struct netdev_list *devs;

    struct netsc_papp papp;
};

/*
 * Stack
 */
typedef int (*net_stack_chain_f)(struct net *, u8 *, int, void *);


/*
 * MAC address
 */
struct net_macaddr {
    int nr;
    u64 *addrs;
};

/*
 * IPv4 address
 */
struct net_ip4addr {
    int nr;
    u32 *addrs;
};
/*
 * IPv6 address
 */
struct net_ip6addr {
    int nr;
    u8 *addrs;
};

/*
 * Stack chain
 */
struct net_stack_chain_next {
    net_stack_chain_f func;
    void *data;
};

/*
 * Host L3 port
 */
struct net_port_host {
    /* MAC address */
    u8 macaddr[6];
    /* FDB is not required for host port */
    /* IPv4 address */
    struct net_ip4addr ip4addr;
    /* ARP */
    struct net_arp_table arp;
    /* RIB */
    struct net_rib4 rib4;
    /* IPv6 address */
    struct net_ip6addr ip6addr;
    /* ND */
    struct net_nd_table nd;

    struct net_port *port;

    /* Stack chain for tx */
    struct net_stack_chain_next tx;
};

/*
 * Port
 */
struct net_port {
    /* Bidirectional link */
    struct netdev *netdev;
    /* Stack chain */
    struct net_stack_chain_next next;

    /* VLAN bridges */
    struct net_bridge *bridges[4096];
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

#define TL_TASK_TABLE_SIZE      0x100
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


#define TASK_STATE_ALLOC        0
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
    int scheduled;

    /* State */
    int state;

    /* Return code */
    int ret;

    /* Archtecture-specific structure */
    void *arch;

    /* Message */
    struct kmsg msg;

    /* Queue entry */
    struct ktask_queue_entry *qe;
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
    struct ktask_queue_entry tasks[TASK_TABLE_SIZE];
};

/*
 * Tickless task table
 */
struct ktltask_table {
    /* Kernel tasks */
    struct ktask *tasks[TL_TASK_TABLE_SIZE];
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
void kexit(void);
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
int ktask_change_state(struct ktask *, int);
int sched_init(void);
void sched_tickless_prepare(void);
void sched(void);
void sched_switch(void);
int sched_ktask_enqueue(struct ktask_queue_entry *);
struct ktask_queue_entry * sched_ktask_dequeue(void);
struct ktask_queue_entry * ktask_queue_entry_new(struct ktask *);
int ktask_fork_execv(int, int (*)(int, char *[]), char **);
int ktltask_fork_execv(int, int, int (*)(int, char *[]), char **);

/* in processor.c */
int processor_init(void);
struct processor * processor_this(void);
struct processor * processor_get(u8);


/* in shell.c */
int shell_main(int, char *[]);
/* in router.c */
void proc_router(void);


/* Architecture-dependent functions in arch.c */
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
u64 arch_time(void);

/* Physical memory management */
int phys_mem_wire(void *, u64);
void * phys_mem_alloc_pages(u64);
void phys_mem_free_pages(void *);


/* in system.c */
#include <aos/types.h>
int open(const char *, int);
ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);



/* System service */
struct sysserv {

};

struct sstable {

};




#endif /* _KERNEL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
