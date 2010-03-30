/* Hypercall stubs. Note: this is all a hack and should die. */

#define	hc	.long   0x44000022

#define LD64_IM(r, highest, higher, high, low) \
	lis	r,highest; \
	addi	r,r,higher; \
	sldi	r,r,32; \
	addis	r,r,high; \
	addi	r,r,low;

.global lv1_get_physmem
lv1_get_physmem:
	mflr	%r0
	stw	%r0,4(%r1)
	stw	%r3,-8(%r1)	/* Address for maxmem */

	li	%r11,69		/* Get PU ID */
	hc
	std	%r4,-16(%r1)

	li	%r11,74		/* Get LPAR ID */
	hc
	std	%r4,-24(%r1)

	ld	%r3,-24(%r1)
	LD64_IM(%r4,0x0000,0x0000,0x6269,0x0000 /* "bi" */)
	LD64_IM(%r5,0x7075,0x0000,0x0000,0x0000 /* "pu" */)
	ld	%r6,-16(%r1)
	LD64_IM(%r7,0x726d,0x5f73,0x697a,0x6500 /* "rm_size" */)
	li	%r11,91
	hc
	extsw	%r3,%r3

	lwz	%r5,-8(%r1)
	std	%r4,0(%r5)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_setup_address_space
lv1_setup_address_space:
	mflr	%r0
	stw	%r0,4(%r1)

	stw	%r3,-4(%r1)
	stw	%r4,-8(%r1)

	li	%r3,18		/* PT size: log2(256 KB) */
	li	%r4,2		/* Two page sizes */
	li	%r5,24		/* Page sizes: (24 << 56) | (16 << 48) */
	sldi	%r5,%r5,24
	li	%r6,16
	sldi	%r6,%r6,16
	or	%r5,%r5,%r6
	sldi	%r5,%r5,32

	li	%r11,2		/* lv1_construct_virtual_address_space */
	hc

	lwz	%r6,-4(%r1)
	lwz	%r7,-8(%r1)
	std	%r4,0(%r6)
	std	%r5,0(%r7)

	/* AS_ID in r4 */
	mr	%r3,%r4
	li	%r11,7		/* lv1_select_virtual_address_space */
	hc
	extsw	%r3,%r3

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_insert_pte
lv1_insert_pte:
	mflr	%r0
	stw	%r0,4(%r1)

	mr	%r11,%r4	/* Save R4 */

	clrldi	%r3,%r3,32
	clrldi	%r7,%r5,32

	sldi	%r4,%r3,3	/* Convert ptegidx into base PTE slot */
	li	%r3,0		/* Current address space */
	ld	%r5,0(%r11)
	ld	%r6,8(%r11)
	li	%r8,0		/* No other flags */
	
	li	%r11,158
	hc
	extsw	%r3,%r3

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_panic
lv1_panic:
	mflr	%r0
	stw	%r0,4(%r1)

	li	%r11,255
	hc
	extsw	%r3,%r3

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_gpu_open
lv1_gpu_open:
	mflr	%r0
	stw	%r0,4(%r1)

	li	%r11,210
	hc
	extsw	%r3,%r3

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_gpu_context_attribute
lv1_gpu_context_attribute:
	mflr	%r0
	stw	%r0,4(%r1)

	li	%r11,225
	hc
	extsw	%r3,%r3

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr
	
.global lv1_gpu_context_allocate
lv1_gpu_context_allocate:
	mflr	%r0
	stw	%r0,4(%r1)
	stw	%r7,-4(%r1)

	sldi	%r3,%r3,32
	clrldi	%r4,%r4,32
	ori	%r3,%r3,%r4
	clrldi	%r4,%r5,32
	clrldi	%r5,%r6,32

	li	%r11,217
	hc
	extsw	%r3,%r3

	lwz	%r7,-4(%r1)
	std	%r4,0(%r7)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr
	
.global lv1_gpu_memory_allocate
lv1_gpu_memory_allocate:
	mflr	%r0
	stw	%r0,4(%r1)
	stw	%r8,-4(%r1)
	stw	%r9,-8(%r1)

	li	%r11,214
	hc
	extsw	%r3,%r3

	lwz	%r8,-4(%r1)
	lwz	%r9,-8(%r1)
	std	%r4,0(%r8)
	std	%r5,0(%r9)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

