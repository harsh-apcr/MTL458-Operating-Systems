#include "mmu.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PAGE_TABLE_MEM_SIZE 400 * 1024
#define FREE_PAGE_LIST_SIZE 50 * 1024
#define OS_RESERVED_PAGES 18 * 1024
// byte addressable memory
unsigned char RAM[RAM_SIZE];  


// OS's memory starts at the beginning of RAM.
// Store the process related info, page tables or other data structures here.
// do not use more than (OS_MEM_SIZE: 72 MB).
unsigned char* OS_MEM = RAM;  

// memory that can be used by processes.   
// 128 MB size (RAM_SIZE - OS_MEM_SIZE)
unsigned char* PS_MEM = RAM + OS_MEM_SIZE; 

// This first frame has frame number 0 and is located at start of RAM(NOT PS_MEM).
// We also include the OS_MEM even though it is not paged. This is 
// because the RAM can only be accessed through physical RAM addresses.  
// The OS should ensure that it does not map any of the frames, that correspond
// to its memory, to any process's page. 
int NUM_FRAMES = ((RAM_SIZE) / PAGE_SIZE);

// Actual number of usable frames by the processes.
int NUM_USABLE_FRAMES = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);

// To be set in case of errors. 
int error_no; 

void allocate_page_entry(unsigned int pte_idx, int flags) {
    // Check if the memory is already allocated to the process at this pte
    unsigned char is_free = *(page_table_entry *)(RAM + pte_idx) == 0 ? 1 : (RAM + PAGE_TABLE_MEM_SIZE)[*(page_table_entry *)(RAM + pte_idx) >> 3];
    if (is_free == 0) {
        // not free -> page is already allocated
        error_no = ERR_SEG_FAULT;
        return;
    }
    // is_free == 1 (iow page is free, now allocate the page)
    unsigned char *free_page_list;

    // FIRST FIT LIKE STRATEGY FOR ALLOCATING A FREE PAGE
    for(free_page_list = (RAM + PAGE_TABLE_MEM_SIZE) + OS_RESERVED_PAGES;
    free_page_list != (RAM + PAGE_TABLE_MEM_SIZE) + FREE_PAGE_LIST_SIZE;
    free_page_list++) {
        if (*free_page_list == 1) 
            break;
    }
    // if *free_page_list == 0 -> all the pages are reserved in the free page list
    if (*free_page_list == 0) {
        // all of the PS_MEM is fully allocated (no free page available), so kill the process
        error_no = ERR_SEG_FAULT;
        return;
    }
    // *free_page_list == 1
    *free_page_list = 0;    // allocate this page
    unsigned int phys_page_no = free_page_list - (RAM + PAGE_TABLE_MEM_SIZE);
    *(page_table_entry *)(RAM + pte_idx) = (phys_page_no << 3) + flags;
}

void deallocate_page_entry(unsigned int pte_idx) {
    unsigned int phys_page_no = *(page_table_entry *)(RAM + pte_idx) >> 3;
    unsigned char *free_page_list = (RAM + PAGE_TABLE_MEM_SIZE) + phys_page_no;
    if (phys_page_no == 0 || *free_page_list) {
        // pte was never allocated
        error_no = ERR_SEG_FAULT;
        return;
    }
    else {
        // phys_page_no != 0 && *free_page_list == 0 (allocated in PS_MEM)
        *free_page_list = 1;
        // free'd the page
        *(page_table_entry *)(RAM + pte_idx) = 0;
    }
}

int is_pid_present(int pid) {
    return (RAM + PAGE_TABLE_MEM_SIZE + FREE_PAGE_LIST_SIZE)[pid] == 1;
}

void os_init() {
    // TODO student 
    // initialize your data structures.

    // OS takes up 72 / 4 * 1024 pages = 18 * 1024 pages
    // each page takes up 4KB of memory
    memset(RAM, (unsigned char)0, PAGE_TABLE_MEM_SIZE);   // PS_PAGE_TABLE are all initialized to 0 (free)
    memset((RAM + PAGE_TABLE_MEM_SIZE), (unsigned char)0, OS_RESERVED_PAGES);    // RESERVED FOR KERNEL
    memset((RAM + PAGE_TABLE_MEM_SIZE) + OS_RESERVED_PAGES, (unsigned char)1, FREE_PAGE_LIST_SIZE - OS_RESERVED_PAGES);    // ALL THE PAGES ARE INITIALLY FREE
    memset((RAM + PAGE_TABLE_MEM_SIZE + FREE_PAGE_LIST_SIZE), (unsigned char)0, MAX_PROCS * sizeof(unsigned char));    // INITIALIZE PROCESS LIST
    memset((unsigned int *) PS_MEM - 1,(unsigned char)0, sizeof(unsigned int));   // LAST_PID
}


// ----------------------------------- Functions for managing memory --------------------------------- //

/**
 *  Process Virtual Memory layout: 
 *  ---------------------- (virt. memory start 0x00)
 *        code
 *  ----------------------  
 *     read only data 
 *  ----------------------
 *     read / write data
 *  ----------------------
 *        heap
 *  ----------------------
 *        stack  
 *  ----------------------  (virt. memory end 0x3fffff)
 * 
 * 
 *  code            : read + execute only
 *  ro_data         : read only
 *  rw_data         : read + write only
 *  stack           : read + write only
 *  heap            : (protection bits can be different for each heap page)
 * 
 *  assume:
 *  code_size, ro_data_size, rw_data_size, max_stack_size, are all in bytes
 *  code_size, ro_data_size, rw_data_size, max_stack_size, are all multiples of PAGE_SIZE
 *  code_size + ro_data_size + rw_data_size + max_stack_size < PS_VIRTUAL_MEM_SIZE
 *  
 * 
 *  The rest of memory will be used dynamically for the heap.
 * 
 *  This function should create a new process, 
 *  allocate code_size + ro_data_size + rw_data_size + max_stack_size amount of physical memory in PS_MEM,
 *  and create the page table for this process. Then it should copy the code and read only data from the
 *  given `unsigned char* code_and_ro_data` into processes' memory.
 *   
 *  It should return the pid of the new process (if the PS_MEM is full then returns -1)
 *  
 */
int create_ps(int code_size, int ro_data_size, int rw_data_size,
                 int max_stack_size, unsigned char* code_and_ro_data) 
{   
    // TODO student
    unsigned int *last_pid =(unsigned int*)PS_MEM - 1;
    int i = 0;
    while (is_pid_present(*last_pid)) {
        if (*last_pid == 99) {
            *last_pid = 0;
        } else {
            *last_pid += 1;
        }
        if (++i == 100) {
            return -1;
        } 
    }
        
    // *last_pid is not present
    unsigned int new_pid = *last_pid;
    // SETUP PAGE TABLE (4KB size) for this process
    // and allocate code + ro data + rw data + stack in PS_MEM
    
    for(unsigned int VPN = 0; VPN < (code_size) / (4 * 1024); VPN++) {
        unsigned int pte_idx = (new_pid << 12) + (VPN << 2);
        allocate_page_entry(pte_idx, O_READ | O_EX);
    }

    for(unsigned int VPN = (code_size) / (4 * 1024); VPN < (code_size + ro_data_size) / (4 * 1024); VPN++) {
        unsigned int pte_idx = (new_pid << 12) + (VPN << 2);
        allocate_page_entry(pte_idx, O_READ);
    }

    for(unsigned int VPN = (code_size + ro_data_size) / (4 * 1024); VPN < (code_size + ro_data_size + rw_data_size) / (4 * 1024); VPN++) {
        unsigned int pte_idx = (new_pid << 12) + (VPN << 2);
        allocate_page_entry(pte_idx, O_READ | O_WRITE);
    }

    unsigned int stack_top = PS_VIRTUAL_MEM_SIZE - max_stack_size;
    for(unsigned int VPN = stack_top / (4 * 1024); VPN < PS_VIRTUAL_MEM_SIZE / (4 * 1024); VPN++) {
        unsigned int pte_idx = (new_pid << 12) + (VPN << 2);
        allocate_page_entry(pte_idx, O_READ | O_WRITE);
    }
    // copy the code and ro data into process memory (typically from disk in a real OS)
    // unsigned int pte_idx = new_pid << 12;
    // int offset = 0;
    // for(unsigned char *code_ro_data = code_and_ro_data;*code_ro_data != 0; offset++, code_ro_data++) {
    //     if (offset == PAGE_SIZE) {
    //         offset = 0;
    //         pte_idx += 4;
    //     }
    //     RAM[((*(page_table_entry *)(PS_PAGE_TABLE + pte_idx) >> 3) << 12) + offset]  = *code_ro_data;
    // }
    int offset = 0;
    for(unsigned int VPN = 0;VPN < (code_size + ro_data_size) / (4 * 1024);VPN++) {
        unsigned int pte_idx = (new_pid << 12) + (VPN << 2);
        memcpy(RAM + ((*(page_table_entry *)(RAM + pte_idx) >> 3) << 12), code_and_ro_data + offset, PAGE_SIZE);
        offset += PAGE_SIZE;
    }


    (RAM + PAGE_TABLE_MEM_SIZE + FREE_PAGE_LIST_SIZE)[new_pid] = 1;
    return new_pid;
}

/**
 * This function should deallocate all the resources for this process. 
 * 
 */
void exit_ps(int pid) 
{
    // deallocate the page table (RAM may contain garbage value)
    int pte_idx = pid << 12;
    int i;
    for(i = 0;i < 1024;i++) {
        unsigned int phys_page_no = *(page_table_entry*)(RAM + pte_idx) >> 3;
        if (phys_page_no != 0)
            (RAM + PAGE_TABLE_MEM_SIZE)[phys_page_no] = (unsigned char)1;   // free this page

        *(page_table_entry*)(RAM + pte_idx) = 0;   // mark 0 in the pte
        pte_idx += 4;
    }
    // remove from the process list
    (RAM + PAGE_TABLE_MEM_SIZE + FREE_PAGE_LIST_SIZE)[pid] = 0;
}



/**
 * Create a new process that is identical to the process with given pid. 
 * 
 */
int fork_ps(int pid) {

    // TODO student:
    unsigned int *last_pid =(unsigned int*)PS_MEM - 1;
    int i = 0;
    while (is_pid_present(*last_pid)) {
        if (*last_pid == 99) {
            *last_pid = 0;
        } else {
            *last_pid += 1;
        }
        if (++i == 100) {
            return -1;
        } 
    }
    // *last_pid is not present
    int new_pid = *last_pid;
    // copy the page table
    int par_pte_idx = pid << 12;
    int chl_pte_idx = new_pid << 12;
    for(i = 0;i < 1024;i++) {
        int phys_page_no = *(page_table_entry*)(RAM + par_pte_idx) >> 3;   // PPN of parent

        if (phys_page_no != 0 && (RAM + PAGE_TABLE_MEM_SIZE)[phys_page_no] == 0) {
            // found an allocated physical page of the parent
            // allocate phyical page for child
            allocate_page_entry(chl_pte_idx, *(page_table_entry*)(RAM + par_pte_idx) & 7);
            // copy this page's content
            memcpy(RAM + (*(page_table_entry*)(RAM + chl_pte_idx) >> 3) * PAGE_SIZE,
             RAM + (*(page_table_entry*)(RAM + par_pte_idx) >> 3) * PAGE_SIZE,
              PAGE_SIZE);
            // copy parent's PPN to child's PPN at corresponding VPN  
        }
        par_pte_idx += 4;
        chl_pte_idx += 4;
    }
    (RAM + PAGE_TABLE_MEM_SIZE + FREE_PAGE_LIST_SIZE)[new_pid] = 1;
    return new_pid;
}


// dynamic heap allocation
//
// Allocate num_pages amount of pages for process pid, starting at vmem_addr.
// Assume vmem_addr points to a page boundary.  
// Assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
//
//
// Use flags to set the protection bits of the pages.
// Ex: flags = O_READ | O_WRITE => page should be read & writeable.
//
// If any of the pages was already allocated then kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
void allocate_pages(int pid, int vmem_addr, int num_pages, int flags) 
{
    // TODO student (assume vmem_addr points to a page boundary)
    unsigned int VPN = vmem_addr >> 12;
    unsigned int pte_idx = (pid << 12) + (VPN << 2);
    error_no = 1;
    for(int i = 0;i != num_pages;i++) {
        allocate_page_entry(pte_idx, flags);
        if (error_no == ERR_SEG_FAULT) {
            exit_ps(pid);
        }
        pte_idx += 4;
    }
}



// dynamic heap deallocation
//
// Deallocate num_pages amount of pages for process pid, starting at vmem_addr.
// Assume vmem_addr points to a page boundary
// Assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE

// If any of the pages was not already allocated then kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
void deallocate_pages(int pid, int vmem_addr, int num_pages) 
{   
    // TODO student
    unsigned int VPN = vmem_addr >> 12;
    unsigned int pte_idx = (pid << 12) + (VPN << 2);
    error_no = 1;
    for(int i = 0;i != num_pages;i++) {
        deallocate_page_entry(pte_idx);
        if (error_no == ERR_SEG_FAULT) {
            exit_ps(pid);
        }
        pte_idx += 4;
    }

}

// Read the byte at `vmem_addr` virtual address of the process
// In case of illegal memory access kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
// 
// assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
unsigned char read_mem(int pid, int vmem_addr) 
{
    // TODO: student
    unsigned int VPN = vmem_addr >> 12;
    unsigned int pte_idx = (pid << 12) + (VPN << 2);
    
    if (is_present(*(page_table_entry*)(RAM + pte_idx)) 
        && is_readable(*(page_table_entry*)(RAM + pte_idx))) {
        unsigned int pmem_addr = ((*(page_table_entry*)(RAM + pte_idx) >> 3) << 12) + (vmem_addr & 0xFFF);
        return RAM[pmem_addr];
    } else {
        // PS_PAGE_TABLE[pte] & O_READ == 0 (or read permission not set)
        error_no = ERR_SEG_FAULT;
        exit_ps(pid);
        return 0;
    }
}

// Write the given `byte` at `vmem_addr` virtual address of the process
// In case of illegal memory access kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
// 
// assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
void write_mem(int pid, int vmem_addr, unsigned char byte) 
{
    // TODO: student
    unsigned int VPN = vmem_addr >> 12;
    unsigned int pte_idx = (pid << 12) + (VPN << 2);
    if (is_present(*(page_table_entry*)(RAM + pte_idx)) 
        && is_writeable(*(page_table_entry*)(RAM + pte_idx))) {
        unsigned int pmem_addr = ((*(page_table_entry*)(RAM + pte_idx) >> 3) << 12) + (vmem_addr & 0xFFF);
        RAM[pmem_addr] = byte;
    } else {
        // PS_PAGE_TABLE[pte] & O_WRITE == 0 (or read permission not set)
        error_no = ERR_SEG_FAULT;
        exit_ps(pid);
    }
}





// ---------------------- Helper functions for Page table entries ------------------ // 

// return the frame number from the pte
int pte_to_frame_num(page_table_entry pte) 
{
    // TODO: student
    return pte >> 3;
}


// return 1 if read bit is set in the pte
// 0 otherwise
int is_readable(page_table_entry pte) {
    // TODO: student
    return pte & O_READ;
}

// return 1 if write bit is set in the pte
// 0 otherwise
int is_writeable(page_table_entry pte) {
    // TODO: student
    return pte & O_WRITE ? 1 : 0;
}

// return 1 if executable bit is set in the pte
// 0 otherwise
int is_executable(page_table_entry pte) {
    // TODO: student
    return pte & O_EX ? 1 : 0;
}


// return 1 if present bit is set in the pte
// 0 otherwise
int is_present(page_table_entry pte) {
    // TODO: student
    int phys_page_no = pte >> 3;
    if (phys_page_no == 0) {
        // pte is not allocated hence not present
        return 0;
    }
    if ((RAM + PAGE_TABLE_MEM_SIZE)[phys_page_no] == 1) {
        // PPN at pte is no allocated to some process in the memory, hence is not present
        // you never go inside this if-block
        return 0;
    }
    // phys_page_no != 0 && FREE_PAGE_LIST[phys_page_no] == 0
    return 1;
}

// -------------------  functions to print the state  --------------------------------------------- //

void print_page_table(int pid) 
{
    
    unsigned int pte_start = pid << 12; 
    page_table_entry* page_table_start = (page_table_entry*)(RAM + pte_start); // TODO student: start of page table of process pid
    int num_page_table_entries = 1024;           // TODO student: num of page table entries

    // Do not change anything below
    puts("------ Printing page table-------");
    for (int i = 0; i < num_page_table_entries; i++) 
    {
        page_table_entry pte = page_table_start[i];
        printf("Page num: %d, frame num: %d, R:%d, W:%d, X:%d, P%d\n", 
                i, 
                pte_to_frame_num(pte),
                is_readable(pte),
                is_writeable(pte),
                is_executable(pte),
                is_present(pte)
                );
    }

}



