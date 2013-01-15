#ifndef PXE_TELNET_INCLUDED
#define PXE_TELNET_INCLUDED

#include <stdint.h>
#include "../pxe_ip.h"

/* TELNET Command Codes: */
#define	TCSB		(uint8_t)250	/* Start Subnegotiation		*/
#define	TCSE		(uint8_t)240	/* End Of Subnegotiation	*/
#define	TCNOP		(uint8_t)241	/* No Operation			*/
#define	TCDM		(uint8_t)242	/* Data Mark (for Sync)		*/
#define	TCBRK		(uint8_t)243	/* NVT Character BRK		*/
#define	TCIP		(uint8_t)244	/* Interrupt Process		*/
#define	TCAO		(uint8_t)245	/* Abort Output			*/
#define	TCAYT		(uint8_t)246	/* "Are You There?" Function	*/ 
#define	TCEC		(uint8_t)247	/* Erase Character		*/
#define	TCEL		(uint8_t)248	/* Erase Line			*/
#define	TCGA		(uint8_t)249	/* "Go Ahead" Function		*/
#define	TCWILL		(uint8_t)251	/* Desire/Confirm Will Do Option*/
#define	TCWONT		(uint8_t)252	/* Refusal To Do Option		*/
#define	TCDO		(uint8_t)253	/* Request To Do Option		*/
#define	TCDONT		(uint8_t)254	/* Request NOT To Do Option	*/
#define	TCIAC		(uint8_t)255	/* Interpret As Command Escape	*/

/* Telnet Option Codes: */
#define	TOTXBINARY	(uint8_t)  0	/* TRANSMIT-BINARY option	*/
#define	TOECHO		(uint8_t)  1	/* ECHO Option			*/
#define	TONOGA		(uint8_t)  3	/* Suppress Go-Ahead Option	*/
#define	TOTERMTYPE	(uint8_t) 24	/* Terminal-Type Option		*/

/* Network Virtual Printer Special Characters: */
#define	VPLF		'\n'	/* Line Feed				*/
#define	VPCR		'\r'	/* Carriage Return			*/
#define	VPBEL		'\a'	/* Bell (attention signal)		*/
#define	VPBS		'\b'	/* Back Space				*/
#define	VPHT		'\t'	/* Horizontal Tab			*/
#define	VPVT		'\v'	/* Vertical Tab				*/
#define	VPFF		'\f'	/* Form Feed				*/

/* Keyboard Command Characters: */
#define	KCESCAPE	035	/* Local escape character ('^]')	*/
#define	KCDCON		'.'	/* Disconnect escape command		*/
#define	KCNL		'\n'	/* Newline character			*/

#define	KCANY		(NCHRS + 1)

/* Option Subnegotiation Constants: */
#define	TT_IS		0	/* TERMINAL_TYPE option "IS" command	*/
#define	TT_SEND		1	/* TERMINAL_TYPE option "SEND" command	*/

/* establishes telnet connection to server and interacts with user */
int pxe_telnet(PXE_IPADDR *addr, uint16_t port);

#define PXE_TELNET_BUFSIZE         1024    /* buffer size */

#endif /* PXE_TELNET_INCLUDED */