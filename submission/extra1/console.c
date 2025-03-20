#define COM1    0x3f8
#include "console.h"

static int uart;    // is there a uart?
static uint16_t *vga_buffer = (uint16_t *) VGA_ADDRESS;
static uint8_t vga_row = 0, vga_col = 0;

void microdelay(unsigned long us) {

}

static inline unsigned char inb(unsigned short port)
{
    unsigned char data;

    asm volatile("in %1,%0" : "=a" (data) : "d" (port));
    return data;
}

static inline void outb(unsigned short port, unsigned char data)
{
    asm volatile("out %0,%1" : : "a" (data), "d" (port));
}

void uartinit(void)
{

  // Turn off the FIFO
  outb(COM1+2, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outb(COM1+3, 0x80);    // Unlock divisor
  outb(COM1+0, 115200/115200);
  outb(COM1+1, 0);
  outb(COM1+3, 0x03);    // Lock divisor, 8 data bits.
  outb(COM1+4, 0);
  outb(COM1+1, 0x01);    // Enable receive interrupts.

  // If status is 0xFF, no serial port.
  if(inb(COM1+5) == 0xFF)
      return;

  uart = 1;

  // Acknowledge pre-existing interrupt conditions;
  // enable interrupts.
  inb(COM1+2);
  inb(COM1+0);
}

void uartputc(int c)
{
  int i;

  if(!uart)
      return;
  
  for(i = 0; i < 128 && !(inb(COM1+5) & 0x20); i++)
      microdelay(10);

  outb(COM1+0, c);
}
// Function to update cursor position
static void vga_move_cursor() {
    uint16_t pos = vga_row * VGA_WIDTH + vga_col;
    outb(0x3D4, 14);
    outb(0x3D5, (pos >> 8) & 0xFF);
    outb(0x3D4, 15);
    outb(0x3D5, pos & 0xFF);
}

// Clear the VGA screen
void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (WHITE_ON_BLACK << 8) | ' ';
    }
    vga_row = 0;
    vga_col = 0;
    vga_move_cursor();
}

// Print a single character on VGA screen
void vga_putc(char c) {
    if (c == '\n') {
        vga_row++;
        vga_col = 0;
    } else {
        vga_buffer[vga_row * VGA_WIDTH + vga_col] = (WHITE_ON_BLACK << 8) | c;
        vga_col++;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    }
    
    if (vga_row >= VGA_HEIGHT) {
        vga_clear();  // Scroll or clear screen when full
    }

    vga_move_cursor();
}

// VGA initialization
void vga_init(void) {
    vga_clear();
}

// Modified printk to print on both serial and VGA
void printk(const char *str) {
    for (int i = 0; str[i] != 0; i++) {
        uartputc(str[i]);  // Print to serial port
        vga_putc(str[i]);  // Print to VGA screen
    }
}




