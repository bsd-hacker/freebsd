#include <machine/asm.h>	

	.text

ASENTRY(mambocall)
	/*
	 * Use the special Mambo callout opcode and whatever arguments we
	 * were passed. Then return whatever Mambo returned. 
	 */
	.long 0x000EAEB0
	blr

