/***********************license start***************
 *  Copyright (c) 2003-2008 Cavium Networks (support@cavium.com). All rights
 *  reserved.
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Cavium Networks nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 *  TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *  AND WITH ALL FAULTS AND CAVIUM NETWORKS MAKES NO PROMISES, REPRESENTATIONS
 *  OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *  RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
 *  REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
 *  DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
 *  PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
 *  POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
 *  OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 *
 *
 *  For any questions regarding licensing please contact marketing@caviumnetworks.com
 *
 ***********************license end**************************************/

/*
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors."
 */

/* $FreeBSD$ */

#ifndef __OCTEON_PCMAP_REGS_H__
#define __OCTEON_PCMAP_REGS_H__

#include "opt_cputype.h" 

#ifndef LOCORE

/*
 * Utility inlines & macros
 */

#if defined(__mips_n64)
#define	oct_write64(a, v)	(*(volatile uint64_t *)(a) = (uint64_t)(v))
#define	oct_write8_x8(a, v)	(*(volatile uint8_t *)(a) = (uint8_t)(v))

#define	OCT_READ(n, t)							\
static inline t oct_read ## n(uintptr_t a)				\
{									\
	volatile t *p = (volatile t *)a;				\
	return (*p);							\
}

OCT_READ(8, uint8_t);
OCT_READ(16, uint16_t);
OCT_READ(32, uint32_t);
OCT_READ(64, uint64_t);

#elif defined(__mips_n32) || defined(__mips_o32)
#if defined(__mips_n32)
static inline void oct_write64 (uint64_t csr_addr, uint64_t val64)
{
    __asm __volatile (
	    ".set push\n"
            ".set mips64\n"
            "sd     %0, 0(%1)\n"
            ".set pop\n"
            :
	    : "r"(val64), "r"(csr_addr));
}

static inline void oct_write8_x8 (uint64_t csr_addr, uint8_t val8)
{
    __asm __volatile (
	    ".set push\n"
            ".set mips64\n"
            "sb    %0, 0(%1)\n"
            ".set pop\n"
            :
	    : "r"(val8), "r"(csr_addr));
}

#define	OCT_READ(n, t, insn)						\
static inline t oct_read ## n(uint64_t a)				\
{									\
    uint64_t tmp;							\
									\
    __asm __volatile (							\
	".set push\n"							\
        ".set mips64\n"							\
        insn "\t%0, 0(%1)\n"						\
        ".set pop\n"							\
        : "=r"(tmp)							\
        : "r"(a));							\
    return ((t)tmp);							\
}

OCT_READ(8, uint8_t, "lb");
OCT_READ(16, uint16_t, "lh");
OCT_READ(32, uint32_t, "lw");
OCT_READ(64, uint64_t, "ld");
#else

/*
 * XXX
 * Add o32 variants that load the address into a register and the result out
 * of a register properly, and simply disable interrupts before and after and
 * hope that we don't need to refill or modify the TLB to access the address.
 * I'd be a lot happier if csr_addr were a physical address and we mapped it
 * into XKPHYS here so that we could guarantee that interrupts were the only
 * kind of exception we needed to worry about.
 *
 * Also, some of this inline assembly is needlessly verbose.  Oh, well.
 */
static inline void oct_write64 (uint64_t csr_addr, uint64_t val64)
{
	uint32_t csr_addrh = csr_addr >> 32;
	uint32_t csr_addrl = csr_addr;
	uint32_t valh = val64 >> 32;
	uint32_t vall = val64;
	uint32_t tmp1;
	uint32_t tmp2;
	uint32_t tmp3;
	register_t sr;

	sr = intr_disable();

	__asm __volatile (
	    ".set push\n"
            ".set mips64\n"
	    ".set noreorder\n"
	    ".set noat\n"
	    "dsll   %0, %3, 32\n"
	    "dsll   %1, %5, 32\n"
	    "dsll   %2, %4, 32\n"
	    "dsrl   %2, %2, 32\n"
	    "or     %0, %0, %2\n"
	    "dsll   %2, %6, 32\n"
	    "dsrl   %2, %2, 32\n"
	    "or     %1, %1, %2\n"
	    "sd     %0, 0(%1)\n"
            ".set pop\n"
	    : "=&r" (tmp1), "=&r" (tmp2), "=&r" (tmp3)
	    : "r" (valh), "r" (vall), "r" (csr_addrh), "r" (csr_addrl));

	intr_restore(sr);
}

static inline void oct_write8_x8 (uint64_t csr_addr, uint8_t val8)
{
	uint32_t csr_addrh = csr_addr >> 32;
	uint32_t csr_addrl = csr_addr;
	uint32_t tmp1;
	uint32_t tmp2;
	register_t sr;

	sr = intr_disable();

	__asm __volatile (
	    ".set push\n"
            ".set mips64\n"
	    ".set noreorder\n"
	    ".set noat\n"
	    "dsll   %0, %3, 32\n"
	    "dsll   %1, %4, 32\n"
	    "dsrl   %1, %1, 32\n"
	    "or     %0, %0, %1\n"
	    "sb     %2, 0(%0)\n"
            ".set pop\n"
	    : "=&r" (tmp1), "=&r" (tmp2)
	    : "r" (val8), "r" (csr_addrh), "r" (csr_addrl));

	intr_restore(sr);
}

#define	OCT_READ(n, t, insn)						\
static inline t oct_read ## n(uint64_t csr_addr)			\
{									\
	uint32_t csr_addrh = csr_addr >> 32;				\
	uint32_t csr_addrl = csr_addr;					\
	uint32_t tmp1, tmp2;						\
	register_t sr;							\
									\
	sr = intr_disable();						\
									\
	__asm __volatile (						\
	    ".set push\n"						\
            ".set mips64\n"						\
	    ".set noreorder\n"						\
	    ".set noat\n"						\
	    "dsll   %1, %2, 32\n"					\
	    "dsll   %0, %3, 32\n"					\
	    "dsrl   %0, %0, 32\n"					\
	    "or     %1, %1, %0\n"					\
	    "lb     %1, 0(%1)\n"					\
	    ".set pop\n"						\
	    : "=&r" (tmp1), "=&r" (tmp2)				\
	    : "r" (csr_addrh), "r" (csr_addrl));			\
									\
	intr_restore(sr);						\
									\
	return ((t)tmp2);						\
}

OCT_READ(8, uint8_t, "lb");
OCT_READ(16, uint16_t, "lh");
OCT_READ(32, uint32_t, "lw");

static inline uint64_t oct_read64 (uint64_t csr_addr)
{
	uint32_t csr_addrh = csr_addr >> 32;
	uint32_t csr_addrl = csr_addr;
	uint32_t valh;
	uint32_t vall;
	register_t sr;

	sr = intr_disable();

	__asm __volatile (
	    ".set push\n"
            ".set mips64\n"
	    ".set noreorder\n"
	    ".set noat\n"
	    "dsll   %0, %2, 32\n"
	    "dsll   %1, %3, 32\n"
	    "dsrl   %1, %1, 32\n"
	    "or     %0, %0, %1\n"
	    "ld     %1, 0(%0)\n"
	    "dsrl   %0, %1, 32\n"
	    "dsll   %1, %1, 32\n"
	    "dsrl   %1, %1, 32\n"
	    ".set pop\n"
	    : "=&r" (valh), "=&r" (vall)
	    : "r" (csr_addrh), "r" (csr_addrl));

	intr_restore(sr);

	return ((uint64_t)valh << 32) | vall;
}
#endif

#endif

#define	oct_write64_int64(a, v)	(oct_write64(a, (int64_t)(v)))

/*
 * Most write bus transactions are actually 64-bit on Octeon.
 */
static inline void oct_write8 (uint64_t csr_addr, uint8_t val8)
{
    oct_write64(csr_addr, (uint64_t) val8);
}

static inline void oct_write16 (uint64_t csr_addr, uint16_t val16)
{
    oct_write64(csr_addr, (uint64_t) val16);
}

static inline void oct_write32 (uint64_t csr_addr, uint32_t val32)
{
    oct_write64(csr_addr, (uint64_t) val32);
}

#define	oct_readint32(a)	((int32_t)oct_read32((a)))

/*
 * Octeon Address Space Definitions
 */
typedef enum {
   OCTEON_MIPS_SPACE_XKSEG = 3LL,
   OCTEON_MIPS_SPACE_XKPHYS = 2LL,
   OCTEON_MIPS_SPACE_XSSEG = 1LL,
   OCTEON_MIPS_SPACE_XUSEG = 0LL
} octeon_mips_space_t;

typedef enum {
   OCTEON_MIPS_XKSEG_SPACE_KSEG0 = 0LL,
   OCTEON_MIPS_XKSEG_SPACE_KSEG1 = 1LL,
   OCTEON_MIPS_XKSEG_SPACE_SSEG = 2LL,
   OCTEON_MIPS_XKSEG_SPACE_KSEG3 = 3LL
} octeon_mips_xkseg_space_t;


/*
***********************************************************************
 * 32 bit mode alert
 * The kseg0 calc below might fail in xkphys.
 */

/*
 * We limit the allocated device physical blocks to low mem. So use Kseg0
 */

/*
 * Need to go back to kernel to find v->p mappings & vice-versa
 * We are getting non 1-1 mappings.
 * #define OCTEON_PTR2PHYS(addr)  ((unsigned long) addr & 0x7fffffff)
 */
#define OCTEON_PTR2PHYS(addr) octeon_ptr_to_phys(addr)



/*  PTR_SIZE == sizeof(uint32_t)  */

#if defined(__mips_n32) || defined(__mips_o32)
#define mipsx_addr_size				uint32_t	// u_int64
#define MIPSX_ADDR_SIZE_KSEGX_BIT_SHIFT		30		// 62
#define MIPSX_ADDR_SIZE_KSEGX_MASK_REMOVED	0x1fffffff	// 0x1fffffff
#else
#define mipsx_addr_size				uint64_t
#define MIPSX_ADDR_SIZE_KSEGX_BIT_SHIFT		62
#define MIPSX_ADDR_SIZE_KSEGX_MASK_REMOVED	0x1fffffffffffffff
#endif


#define octeon_ptr_to_phys(ptr)                                           \
   (((((mipsx_addr_size) ptr) >> MIPSX_ADDR_SIZE_KSEGX_BIT_SHIFT) == 2) ? \
    	((mipsx_addr_size) ptr & MIPSX_ADDR_SIZE_KSEGX_MASK_REMOVED)  :   \
        (vtophys(ptr)))

#ifdef CODE_FOR_64_BIT_NEEDED
static inline mipsx_addr_size octeon_ptr_to_phys (void *ptr)
{
    if ((((mipsx_addr_size) ptr) >> MIPSX_ADDR_SIZE_KSEGX_BIT_SHIFT) == 2) {
        /*
         * KSEG0 based address ?
         */
        return ((mipsx_addr_size) ptr & MIPSX_ADDR_SIZE_KSEGX_MASK_REMOVED);
    } else {
        /*
         * Ask kernel/vm to give us the phys translation.
         */
        return (vtophys(ptr));
    }
}
#endif

#define OCTEON_IO_SEG OCTEON_MIPS_SPACE_XKPHYS

/*
 * octeon_addr_t
 */
typedef union {
   uint64_t         word64;

   struct {
       octeon_mips_space_t          R   : 2;
       uint64_t               offset :62;
   } sva; // mapped or unmapped virtual address

   struct {
       uint64_t               zeroes :33;
       uint64_t               offset :31;
   } suseg; // mapped USEG virtual addresses (typically)

   struct {
       uint64_t                ones  :33;
       octeon_mips_xkseg_space_t   sp   : 2;
       uint64_t               offset :29;
   } sxkseg; // mapped or unmapped virtual address

   struct {
       octeon_mips_space_t         R    :2; // CVMX_MIPS_SPACE_XKPHYS in this case
       uint64_t                 cca  : 3; // ignored by octeon
       uint64_t                 mbz  :10;
       uint64_t                  pa  :49; // physical address
   } sxkphys; // physical address accessed through xkphys unmapped virtual address

   struct {
       uint64_t                 mbz  :15;
       uint64_t                is_io : 1; // if set, the address is uncached and resides on MCB bus
       uint64_t                 did  : 8; // the hardware ignores this field when is_io==0, else device ID
       uint64_t                unaddr: 4; // the hardware ignores <39:36> in Octeon I
       uint64_t               offset :36;
   } sphys; // physical address

    struct {
        uint64_t               zeroes :24; // techically, <47:40> are dont-cares
        uint64_t                unaddr: 4; // the hardware ignores <39:36> in Octeon I
        uint64_t               offset :36;
    } smem; // physical mem address

    struct {
        uint64_t                 mem_region  :2;
        uint64_t                 mbz  :13;
        uint64_t                is_io : 1; // 1 in this case
        uint64_t                 did  : 8; // the hardware ignores this field when is_io==0, else device ID
        uint64_t                unaddr: 4; // the hardware ignores <39:36> in Octeon I
        uint64_t               offset :36;
    } sio; // physical IO address

    struct {
        uint64_t                didspace : 24;
        uint64_t                unused   : 40;
    } sfilldidspace;

} octeon_addr_t;


typedef union {
    uint64_t	    word64;
    struct {
        uint32_t    word32hi;
        uint32_t    word32lo;
    } bits;
} octeon_word_t;




/*
 * octeon_build_io_address
 *
 * Builds a memory address for I/O based on the Major 5bits and Sub DID 3bits
 */
static inline uint64_t octeon_build_io_address (uint64_t major_did,
                                                uint64_t sub_did)
{
    return ((0x1ull << 48) | (major_did << 43) | (sub_did << 40));
}

/*
 * octeon_build_mask
 *
 * Builds a bit mask given the required size in bits.
 *
 * @param bits   Number of bits in the mask
 * @return The mask
 */
static inline uint64_t octeon_build_mask (uint64_t bits)
{
    return ~((~0x0ull) << bits);
}

/*
 * octeon_build_bits
 *
 * Perform mask and shift to place the supplied value into
 * the supplied bit rage.
 *
 * Example: octeon_build_bits(39,24,value)
 * <pre>
 * 6       5       4       3       3       2       1
 * 3       5       7       9       1       3       5       7      0
 * +-------+-------+-------+-------+-------+-------+-------+------+
 * 000000000000000000000000___________value000000000000000000000000
 * </pre>
 *
 * @param high_bit Highest bit value can occupy (inclusive) 0-63
 * @param low_bit  Lowest bit value can occupy inclusive 0-high_bit
 * @param value    Value to use
 * @return Value masked and shifted
 */
static inline uint64_t octeon_build_bits (uint64_t high_bit, uint64_t low_bit,
                                          uint64_t value)
{
    return ((value & octeon_build_mask(high_bit - low_bit + 1)) << low_bit);
}


/* ------------------------------------------------------------------- *
 *                      octeon_get_chipid()                               *
 * ------------------------------------------------------------------- */
#define OCTEON_CN31XX_CHIP  0x000d0100
#define OCTEON_CN30XX_CHIP  0x000d0200
#define OCTEON_CN3020_CHIP  0x000d0112
#define OCTEON_CN5020_CHIP  0x000d0601

static inline uint32_t octeon_get_chipid(void)
{
    uint32_t id;

    __asm __volatile ("mfc0 %0, $15,0" : "=r" (id));

    return (id);
}


static inline uint32_t octeon_get_core_num (void)
{

    return (0x3FF & mips_rd_ebase());
}


static inline uint64_t octeon_get_cycle(void)
{

/*  ABI == 32 */

    uint32_t tmp_low, tmp_hi;

    __asm __volatile (
               "   .set push                  \n"
               "   .set mips64r2                \n"
               "   .set noreorder               \n"
               "   rdhwr %[tmpl], $31           \n"
               "   dadd  %[tmph], %[tmpl], $0   \n"
               "   dsrl  %[tmph], 32            \n"
               "   dsll  %[tmpl], 32            \n"
               "   dsrl  %[tmpl], 32            \n"
               "   .set pop                 \n"
                  : [tmpl] "=&r" (tmp_low), [tmph] "=&r" (tmp_hi) : );

    return(((uint64_t)tmp_hi << 32) + tmp_low);
}


/**
 * Wait for the specified number of cycle
 *
 * @param cycles
 */
static inline void octeon_wait (uint64_t cycles)
{
    uint64_t done = octeon_get_cycle() + cycles;

    while (octeon_get_cycle() < done)
    {
        /* Spin */
    }
}



/*
 * octeon_machdep.c
 *
 * Direct to Board Support level.
 */
extern void octeon_led_write_char(int char_position, char val);
extern void octeon_led_write_hexchar(int char_position, char hexval);
extern void octeon_led_write_hex(uint32_t wl);
extern void octeon_led_write_string(const char *str);
extern void octeon_reset(void);
extern void octeon_led_write_char0(char val);
extern void octeon_led_run_wheel(int *pos, int led_position);
extern void octeon_debug_symbol(void);
extern uint64_t octeon_dram;
extern uint8_t octeon_mac_addr[6];
extern int octeon_core_mask, octeon_mac_addr_count;
extern void octeon_ciu_reset(void);
extern int octeon_board_real(void);
extern unsigned long octeon_get_clock_rate(void);
#endif	/* LOCORE */


#define OCTEON_CLOCK_DEFAULT (500 * 1000 * 1000)


/*
 * Octeon Boot Bus BIST Status
 * Mostly used for dummy read to ensure all prev I/Os are write-complete.
 */
#define  OCTEON_MIO_BOOT_BIST_STAT      0x80011800000000F8ull

/*
 * Octeon UART unit
 */
#define  OCTEON_MIO_UART0               0x8001180000000800ull
#define  OCTEON_MIO_UART1               0x8001180000000C00ull
#define  OCTEON_MIO_UART0_THR           0x8001180000000840ull
#define  OCTEON_MIO_UART1_THR           0x8001180000000C40ull
#define  OCTEON_MIO_UART0_LSR           0x8001180000000828ull
#define  OCTEON_MIO_UART1_LSR           0x8001180000000C28ull
#define  OCTEON_MIO_UART0_RBR           0x8001180000000800ull
#define  OCTEON_MIO_UART1_RBR           0x8001180000000C00ull
#define  OCTEON_MIO_UART0_USR           0x8001180000000938ull
#define  OCTEON_MIO_UART1_USR           0x8001180000000D38ull
#define  OCTEON_MIO_ADDR_HI24           0x800118
#define  OCTEON_MIO_UART_SIZE           0x400ull


/*
 * EBT3000 LED Unit
 */
#define  OCTEON_CHAR_LED_BASE_ADDR	(0x1d020000 | (0x1ffffffffull << 31))

#define  OCTEON_FPA_QUEUES		8

/*
 * Physical Memory Banks
 */
/* 1st BANK */
#define OCTEON_DRAM_FIRST_256_START	0x00000000ull
#define OCTEON_DRAM_FIRST_256_END	(0x10000000ull - 1ull)
#define OCTEON_DRAM_RESERVED_END        0X1FFF000ULL	/* 32 Meg Reserved for Mips Kernel MD Ops */
#define OCTEON_DRAM_FIRST_BANK_SIZE	(OCTEON_DRAM_FIRST_256_END - OCTEON_DRAM_FIRST_256_START + 1)

/* 2nd BANK */
#define OCTEON_DRAM_SECOND_256_START	(0x0000000410000000ull)
#define OCTEON_DRAM_SECOND_256_END	(0x0000000420000000ull - 1ull)  /* Requires 64 bit paddr */
#define OCTEON_DRAM_SECOND_BANK_SIZE	(OCTEON_DRAM_SECOND_256_END - OCTEON_DRAM_SECOND_256_START + 1ull)

/* 3rd BANK */
#define OCTEON_DRAM_ABOVE_512_START	0x20000000ull
#define OCTEON_DRAM_ABOVE_512_END	(0x0000000300000000ull - 1ull)  /* To be calculated as remaining */
#define OCTEON_DRAM_THIRD_BANK_SIZE	(OCTEON_DRAM_ABOVE_512_END - OCTEON_DRAM_ABOVE_512_START + 1ull)

#endif /* !OCTEON_PCMAP_REGS_H__ */
