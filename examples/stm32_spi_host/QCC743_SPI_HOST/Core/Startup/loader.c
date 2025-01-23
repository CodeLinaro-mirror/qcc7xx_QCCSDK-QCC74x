/*
 * loader.c
 *
 *  Created on: Dec 5, 2024
 */

extern unsigned long __isr_vector_load_addr_start;
extern unsigned long _sisr_vector;
extern unsigned long _eisr_vector;

extern unsigned long __text_load_addr_start;
extern unsigned long _stext;
extern unsigned long _etext;

extern unsigned long __rodata_load_addr_start;
extern unsigned long _srodata;
extern unsigned long _erodata;

extern unsigned long __init_array_load_addr_start;
extern unsigned long __init_array_start;
extern unsigned long __init_array_end;

extern unsigned long __isr_vector_load_addr_start;
extern unsigned long _sisr_vector;
extern unsigned long _eisr_vector;

extern unsigned long __text_load_addr_start;
extern unsigned long _stext;
extern unsigned long _etext;

extern unsigned long __rodata_load_addr_start;
extern unsigned long _srodata;
extern unsigned long _erodata;

extern unsigned long __init_array_load_addr_start;
extern unsigned long __init_array_start;
extern unsigned long __init_array_end;

static void do_load(unsigned long *load_addr, unsigned long *start, unsigned long *end)
{
	unsigned long *src = load_addr;
	unsigned long *dst = start;

	if (load_addr == start)
		return;

	while (dst < end)
		*dst++ = *src++;
}

void start_load(void)
{
	/* Load ISR vector. */
	do_load(&__isr_vector_load_addr_start, &_sisr_vector, &_eisr_vector);
	/* Load text. */
	do_load(&__text_load_addr_start, &_stext, &_etext);
	/* Load rodata. */
	do_load(&__rodata_load_addr_start, &_srodata, &_erodata);
	/* Load init array. */
	do_load(&__init_array_load_addr_start, &__init_array_start, &__init_array_end);
}

