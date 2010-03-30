/* Hypercall stubs. Note: this is all a hack and should die. */

#define	hc	.long   0x44000022

.global lv1_get_logical_pu_id
lv1_get_logical_pu_id:
	mflr	%r0
	stw	%r0,4(%r1)

	stw	%r3,-4(%r1)
	li	%r11,69
	hc
	extsw	%r3,%r3
	lwz	%r5,-4(%r1)
	std	%r4,0(%r5)
	
	lwz	%r0,4(%r1)
	mtlr	%r0
	blr
	
.global lv1_get_logical_partition_id
lv1_get_logical_partition_id:
	mflr	%r0
	stw	%r0,4(%r1)

	stw	%r3,-4(%r1)
	li	%r11,74
	hc
	extsw	%r3,%r3
	lwz	%r5,-4(%r1)
	std	%r4,0(%r5)
	
	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_insert_htab_entry
lv1_insert_htab_entry:
	mflr	%r0
	stw	%r0,4(%r1)

	/* Zero high-order bits of arguments */
	clrldi	%r3,%r3,32
	clrldi	%r4,%r4,32
	clrldi	%r5,%r5,32
	clrldi	%r6,%r6,32
	clrldi	%r7,%r7,32
	clrldi	%r8,%r8,32
	clrldi	%r9,%r9,32
	clrldi	%r10,%r10,32
	/* Move high 32 bits of args 3 and 4 into position */
	sldi	%r5,%r5,32
	or	%r5,%r5,%r6
	sldi	%r6,%r7,32
	or	%r6,%r6,%r8
	/* Shift remaining args in */
	mr	%r7,%r9
	mr	%r8,%r10

	li	%r11,158
	hc
	extsw	%r3,%r3

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_construct_virtual_address_space
lv1_construct_virtual_address_space:
	mflr	%r0
	stw	%r0,4(%r1)
	stwu	%r1,-16(%r1)
	stw	%r7,8(%r1)
	stw	%r8,12(%r1)
	
	clrldi	%r3,%r3,32
	clrldi	%r4,%r4,32
	clrldi	%r5,%r5,32
	clrldi	%r6,%r6,32
	sldi	%r5,%r5,32
	or	%r5,%r5,%r6

	li	%r11,2
	hc
	extsw	%r3,%r3

	lwz	%r7,8(%r1)
	lwz	%r8,12(%r1)
	std	%r4,0(%r7)
	std	%r5,0(%r8)
	lwz	%r1,0(%r1)

	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

.global lv1_select_virtual_address_space
lv1_select_virtual_address_space:
	mflr	%r0
	stw	%r0,4(%r1)

	clrldi	%r3,%r3,32
	clrldi	%r4,%r4,32
	sldi	%r3,%r3,32
	or	%r3,%r3,%r4

	li	%r11,7
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
	
