#if !defined(__CONSOLE_H__)
#define __CONSOLE_H__

#include <stdint.h>

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define WHITE_ON_BLACK 0x0F  // White text, Black background

void uartinit(void); 
void printk(const char *str);
void vga_putc(char c);   // Print a character to VGA screen
void vga_clear(void);    // Clear screen
void vga_init(void);     // Initialize VGA screen

#endif
