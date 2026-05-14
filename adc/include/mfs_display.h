#ifndef MFS_DISPLAY_H
#define MFS_DISPLAY_H

#include <stdint.h>

#define MFS_DISPLAY_DIGITS 4U

void mfs_display_init(void);
void mfs_display_show_digit(uint8_t digit, char c);
void mfs_display_blank(void);

#endif
