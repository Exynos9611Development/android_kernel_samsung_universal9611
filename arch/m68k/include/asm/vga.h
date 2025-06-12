/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_M68K_VGA_H
#define _ASM_M68K_VGA_H

#include <asm/io.h>

/*
 * FIXME
 * Ugh, we don't have PCI space, so map readb() and friends to use raw I/O
 * accessors, which are identical to the z_*() Zorro bus accessors.
 * This should make cirrusfb work again on Amiga
 */
#undef inb_p
#undef inw_p
#undef outb_p
#undef outw
#undef readb
#undef writeb
#undef writew
#define inb_p(port)		0
#define inw_p(port)		0
#define outb_p(port, val)	do { } while (0)
#define outw(port, val)		do { } while (0)
#define readb			__raw_readb
#define writeb			__raw_writeb
#define writew			__raw_writew

#endif /* _ASM_M68K_VGA_H */
