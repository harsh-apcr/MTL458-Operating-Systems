#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <inttypes.h>
#include <time.h>

#define SEC_TO_NS(sec) ((sec)*1000000000)
uint64_t nanos()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    uint64_t ns = SEC_TO_NS((uint64_t)ts.tv_sec) + (uint64_t)ts.tv_nsec;
    return ns;
}


#define NPTENTRIES 1024 * 1024  // 2^20 pte per pagetable in a linear page table system
#define VPAGE_OFFSET 12
#define VADDR_OFFSET_MASK (1 << 12) - 1

uint64_t* timestamps;     // lru
int fifo_front;         // fifo
int fifo_rear;          // fifo

int clock_hand;

bool *alloc_pglist;
uint *vpn_list;
int NUM_PHY_PAGES;
int num_allocpages;

typedef uint16_t pte_t;
pte_t entryptable[NPTENTRIES];
typedef enum pte_flag_t {
    VALID = 1,
    PRESENT = 2,
    DIRTY = 4,
    USE = 8,
    READ = 16,
    WRITE = 32
} pte_flag_t;
#define NUM_PTFLAGS 6

/* 
pte_t layout : phys_page_num    protection       use     dirty      present        valid
                  10               2              1        1           1             1
                16-bits or 2 bytes are required for storing a pte
*/

typedef enum replacement_strategy_t {
    OPT, FIFO, CLOCK, LRU, RANDOM
} strategy_t;
strategy_t strategy;

typedef enum memaccess_type {
    R, W
} memaccess_type;

typedef struct memaccess_t {
    uint va;
    memaccess_type t;
} memaccess_t;

uint NUM_ACCESSES;
memaccess_t access_list[10000000];          // stores the va of each access along with its memaccess_type
memaccess_t *curr_memaccess;       // pointer to curr_memaccess in the access_list (access_list <= curr_memaccess < access_list + NUM_ACCESSES)

bool is_dropped;            // whether a page is dropped or written to disk (if dirty)
uint vpn_drop_write;        // if it is written then vpn of the written page (to the disk)
uint vpn_read;              // vpn of the read page (from disk)
bool is_verbose;
bool is_pgreplace;

int num_misses = 0;
int num_disk_writes = 0;
int num_drops = 0;


void replace_page_opt() {
    uint nextaccess[NUM_PHY_PAGES];  // stores when the nearest next access after the curr_memaccess occur
    memset(nextaccess, 0xFF, NUM_PHY_PAGES * sizeof(uint));
    memaccess_t *final_memaccess = access_list + NUM_ACCESSES;
    memaccess_t *memaccess_itr;
    int ppn;
    int num_nextaccess = 0;
    for(memaccess_itr = curr_memaccess + 1;memaccess_itr < final_memaccess;++memaccess_itr) {
        if (num_nextaccess == NUM_PHY_PAGES) break;
        if (entryptable[(memaccess_itr->va) >> VPAGE_OFFSET] & PRESENT)
            ppn = entryptable[(memaccess_itr->va) >> VPAGE_OFFSET] >> NUM_PTFLAGS;
        else continue;
        if (nextaccess[ppn] == UINT32_MAX) {
            nextaccess[ppn] = memaccess_itr - curr_memaccess;
            num_nextaccess++;
        }
    }
    // populate the nearest nextaccess array
    int ppn_farthest = 0;
    uint farthest = 0;
    for(ppn = 0;ppn < NUM_PHY_PAGES;++ppn) {
        if (farthest < nextaccess[ppn]) {
            farthest = nextaccess[ppn];
            ppn_farthest = ppn;
        }
    }
    // deallocate/free the new page (evicting the page)
    uint vpn_farthest = vpn_list[ppn_farthest];
    alloc_pglist[ppn_farthest] = 0;

    is_dropped = (entryptable[vpn_farthest] & DIRTY) ? 0 : 1;
    num_disk_writes += is_dropped ? 0 : 1;
    num_drops += is_dropped ? 1 : 0;
    vpn_drop_write = vpn_farthest;

    // set the present bit in the pagetable to 0
    entryptable[vpn_farthest] -= PRESENT;   // if we'd written to a disk then i would also set ppn (in disk)
    num_allocpages--;
}

void replace_page_fifo() {
    int ppn = fifo_front;
    alloc_pglist[ppn] = 0;  // de-allocate from FREE-PAGE-LIST
    // set the present bit in pte to 0
    uint vpn = vpn_list[ppn];
    entryptable[vpn] -= PRESENT;
    is_dropped = (entryptable[vpn] & DIRTY) ? 0 : 1;
    num_disk_writes += is_dropped ? 0 : 1;
    num_drops += is_dropped ? 1 : 0;
    vpn_drop_write = vpn;
    fifo_front = (fifo_front + 1) % NUM_PHY_PAGES;
    num_allocpages--;
}

void replace_page_lru() {
    uint64_t lru_page = nanos();
    int lru_ppn;
    // printf("timestamps\n");
    // printf("lru_page : %ld\n", lru_page);
    for(int i=0;i<NUM_PHY_PAGES;i++) assert(lru_page >= timestamps[i]);
    for(int ppn = 0;ppn < NUM_PHY_PAGES;ppn++) {
        // printf("timestamp[%d] : %ld\n", ppn, timestamps[ppn]);
        if (lru_page >= timestamps[ppn]) {
            lru_ppn = ppn;
            lru_page = timestamps[ppn];
        }
    }
    alloc_pglist[lru_ppn] = 0;
    // also set the present bit of the mapping vpn -> ppn to 0
    int vpn = vpn_list[lru_ppn];
    entryptable[vpn] -= PRESENT;
    is_dropped = (entryptable[vpn] & DIRTY) ? 0 : 1;
    num_disk_writes += is_dropped ? 0 : 1;
    num_drops += is_dropped ? 1 : 0;
    vpn_drop_write = vpn;
    num_allocpages--;
}        // printf("%ld\n", timestamps[ppn]);


void replace_page_random() {
    int ppn_to_replace = rand() % NUM_PHY_PAGES;
    alloc_pglist[ppn_to_replace] = 0;
    // also set the present bit of the mapping vpn -> ppn to 0
    int vpn = vpn_list[ppn_to_replace];
    entryptable[vpn] -= PRESENT;
    is_dropped = (entryptable[vpn] & DIRTY) ? 0 : 1;
    num_disk_writes += is_dropped ? 0 : 1;
    num_drops += is_dropped ? 1 : 0;
    vpn_drop_write = vpn;
    num_allocpages--;
}

void replace_page_clock() {
    assert(clock_hand < NUM_PHY_PAGES);
    int vpn = vpn_list[clock_hand];
    if (entryptable[vpn] & USE) {
        entryptable[vpn] -= USE;
        clock_hand = (clock_hand + 1) % NUM_PHY_PAGES;
        replace_page_clock();
        return;
    }
    // entryptable[vpn] & USE == 0
    int ppn_to_replace = clock_hand;
    clock_hand = (clock_hand + 1) % NUM_PHY_PAGES;
    alloc_pglist[ppn_to_replace] = 0;
    // also set the present bit of the mapping vpn -> ppn to 0
    entryptable[vpn] -= PRESENT;
    is_dropped = (entryptable[vpn] & DIRTY) ? 0 : 1;
    num_disk_writes += is_dropped ? 0 : 1;
    num_drops += is_dropped ? 1 : 0;
    vpn_drop_write = vpn;
    num_allocpages--;
}

void replace_page(strategy_t strategy) {
    switch(strategy) {
        case OPT:
            replace_page_opt();
            break;
        case FIFO:
            replace_page_fifo();
            break;
        case CLOCK:
            replace_page_clock();
            break;
        case LRU:
            replace_page_lru();
            break;
        case RANDOM:
            replace_page_random();
            break;
        default:
            break;
    }
}


/*
page fault handler
*/
void page_fault(uint vaddr) {
    int VPN = vaddr >> VPAGE_OFFSET;
    if (num_allocpages == NUM_PHY_PAGES) {
        // use page replacement strategy to identify a page to evict
        is_pgreplace = true;
        vpn_read = VPN;
        replace_page(strategy);
        // num_allocpages == NUM_PHY_PAGES - 1
        goto allocate_page;
    } else {
        // num_allocpages < NUM_PHY_PAGES
        // first-fit strategy for finding a free page
        is_pgreplace = false;
        allocate_page: {
            // page could be allocated either for an invalid entry
            // or for a valid entry with present-bit = 0
            vpn_read = VPN;
            int ppn = 0;
            while (ppn < NUM_PHY_PAGES && alloc_pglist[ppn]) // only atmost 1000 cheap iterations so not that bad
                ppn += 1;
            // alloc_pglist[ppn] == 0 || ppn == NUM_PHY_PAGES
            assert(ppn < NUM_PHY_PAGES);
            alloc_pglist[ppn] = true;   // allocate the ppn
            num_allocpages++;
            entryptable[VPN] = (ppn << NUM_PTFLAGS) + WRITE + READ + PRESENT + VALID;   // map VPN -> ppn
            vpn_list[ppn] = VPN;                                                        // map ppn -> VPN
            // enqueue
            fifo_front = (fifo_front == -1) ? 0 : fifo_front;
            fifo_rear = (fifo_rear + 1) % NUM_PHY_PAGES;
        }
    }
}


/*
returns the access to paddr with given virtual address (incase vaddr is not mapped performs page_fault)
*/
uint memaccess(uint vaddr, memaccess_type t) {
    int VPN = vaddr >> VPAGE_OFFSET;
    retry_on_pagefault: {
        if (entryptable[VPN] & PRESENT) {
            // page is present in memory
            uint ppn = entryptable[VPN] >> NUM_PTFLAGS;
            entryptable[VPN] |= (t == W) ? DIRTY : 0;       // set dirty bit incase of a write access
            
            timestamps[ppn] = nanos();                      // timestamps for LRU
            entryptable[VPN] |= USE;                        // set use bit (for clock)

            return (ppn << VPAGE_OFFSET) + (vaddr & VADDR_OFFSET_MASK);
        } else {
            // page is not present in memory, but page is valid
            // page_fault
            num_misses++;
            page_fault(vaddr);
            if (is_verbose && is_pgreplace) {
                if (is_dropped) {
                    printf("Page 0x%x was read from disk, page 0x%x was dropped (it was not dirty).\n", vpn_read, vpn_drop_write);
                } else {
                    printf("Page 0x%x was read from disk, page 0x%x was written to disk.\n", vpn_read, vpn_drop_write);
                }
            }
            goto retry_on_pagefault;
        }
    }
}

void set_strategy(char *strat) {
    if (!strcmp(strat, "OPT")) strategy = OPT;
    else if (!strcmp(strat, "FIFO")) strategy = FIFO;
    else if (!strcmp(strat, "CLOCK")) strategy = CLOCK;
    else if (!strcmp(strat, "LRU")) strategy = LRU;
    else if (!strcmp(strat, "RANDOM")) strategy = RANDOM;
    else {
        fprintf(stderr, "INVALID STRATEGY %s\n", strat);
        exit(1);
    }
} 

int main(int argc, char **argv) {
    srand(5635);
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage : ./a.out trace.in <num_phy_page> <strategy> [-verbose]\n");
        exit(1);
    }
    // argc == 4 || argc == 5
    NUM_PHY_PAGES = atoi(argv[2]);
    if (NUM_PHY_PAGES > 1000) {
        fprintf(stderr, "<num_phy_page> must be less than 1000");
        exit(1);
    }
    alloc_pglist = calloc(NUM_PHY_PAGES, sizeof(bool));    // allocated page list (initially all 0's)
    memset(entryptable, 0, NPTENTRIES * sizeof(pte_t));         // allocate 0 to pt initially
    num_allocpages = 0;
    timestamps = malloc(NUM_PHY_PAGES * sizeof(uint64_t));    // only well defined for pages that are accessed
    vpn_list = malloc(NUM_PHY_PAGES * sizeof(uint));

    fifo_front = -1;
    fifo_rear = -1;

    clock_hand = 0;
    is_verbose = argc == 5;
    set_strategy(argv[3]);

    char *trace_in = argv[1];
    FILE *file = fopen(trace_in, "r");
    if (file == NULL) {
        fprintf(stderr, "file %s doesn't exist\n", trace_in);
        exit(1);
    }

    int ctr = 0;
    int addr;char rw[2];
    while (fscanf(file, "%x %s", &addr, rw) > 0) {
        if (rw[0] == 'R') access_list[ctr].t = R;
        else access_list[ctr].t = W;
        access_list[ctr].va = addr;
        ctr++;
    }
    NUM_ACCESSES = ctr;

    for(curr_memaccess = access_list;curr_memaccess < access_list + NUM_ACCESSES;++curr_memaccess) {
        memaccess(curr_memaccess->va, curr_memaccess->t);
    }
    printf("Number of memory accesses: %d\n", NUM_ACCESSES);
    printf("Number of misses: %d\n", num_misses);
    printf("Number of writes: %d\n", num_disk_writes);
    printf("Number of drops: %d\n", num_drops);

    free(alloc_pglist);
}











