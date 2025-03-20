#include "console.h"
#include "mmu.h"
#include <stdint.h>
#include <stddef.h>

struct segdesc gdt[NSEGS] = {
    [0] = {0}, // Null segment
    [SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0),
    [SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0),
};

// GDT Descriptor
struct gdtdesc gdtdesc = {
    .limit = sizeof(gdt) - 1,
    .base = (uint)&gdt[0]
};

// Tell boot.asm about c_stack
__attribute__((aligned(PGSIZE)))
char c_stack[PGSIZE];

//-----Paging Setup------

// Page Tables (Maps first 8MB of memory)
__attribute__((__aligned__(PGSIZE)))
pte_t entry_pgtable1[NPTENTRIES];  // Maps 0MB - 4MB

__attribute__((__aligned__(PGSIZE)))
pte_t entry_pgtable2[NPTENTRIES];  // Maps 4MB - 8MB

__attribute__((__aligned__(PGSIZE)))
pde_t entry_pgdir[NPDENTRIES];  // Page Directory


static inline void halt(void)
{
    asm volatile("hlt" : : );
}

static inline void
write_gdt(struct segdesc *p, int size)
{
    volatile ushort pd[3];

    pd[0] = size-1;
    pd[1] = (uint)p;
    pd[2] = (uint)p >> 16;

    asm volatile("lgdt (%0)" : : "r" (pd));
}

static inline uint
read_cr0(void)
{
    uint val;
    asm volatile("movl %%cr0,%0" : "=r" (val));
    return val;
}

static inline void
write_cr0(uint val)
{
    asm volatile("movl %0,%%cr0" : : "r" (val));
}

static inline void
write_cr3(uint val)
{
    asm volatile("movl %0,%%cr3" : : "r" (val));
}


// Setup Paging
void setup_paging() {
    // Map first 4MB (0MB - 4MB)
    for (int i = 0; i < NPTENTRIES; i++) {
        entry_pgtable1[i] = (i * PGSIZE) | (PTE_P) | (PTE_W);
    }

    // Map next 4MB (4MB - 8MB)
    for (int i = 0; i < NPTENTRIES; i++) {
        entry_pgtable2[i] = ((4 * 1024 * 1024) + (i * PGSIZE)) | (PTE_P) | (PTE_W);
    }

    // Set up page directory
    entry_pgdir[0] = ((uint32_t)entry_pgtable1) | (PTE_P) | (PTE_W);  // First 4MB
    entry_pgdir[1] = ((uint32_t)entry_pgtable2) | (PTE_P) | (PTE_W);  // Next 4MB
    
    // Load page directory into CR3
    write_cr3((uint32_t)&entry_pgdir);

    // Enable paging in CR0
    uint cr0 = read_cr0();
    cr0 |= 0x80000000; // Set PG (paging enable) bit in CR0
    write_cr0(cr0);
}

int main(void)
{
    int i; 
    int sum = 0;

    

    // Initialize the page table here
    setup_paging(); 
    // Initialize the console
    uartinit();
    // Initialize VGA text mode
    vga_init();  
    printk("DEBUG: Entering main()\n");
    printk("Hello from C, VGA and Serial!\n");

    // This test code touches 32 pages in the range 0 to 8MB
    for (i = 0; i < 64; i++) {
        int *p = (int *)(i * 4096 * 32);
        sum += *p; 
                
        printk("page\n"); 
    }
    halt(); 
    return sum; 
}


