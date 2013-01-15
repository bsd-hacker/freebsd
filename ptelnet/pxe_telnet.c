#include <stand.h>
#include <stdint.h>

#include "pxe_telnet.h"
#include "telnet_fsm.h"
#include "telnet_opts.h"

/* telnet_write() - writes sequence of bytes to connected socket
 * in:
 *	socket	- socket number
 *	buf	- buffer to write data from
 *	bufsize	- size of buffer
 * out:
 *	0	- all is ok
 *	-1	- all is bad
 *	>0	- count of written bytes
 */
int
telnet_write(int socket, uint8_t *buf, size_t bufsize)
{
	return (0);
}

/* console_write() - writes sequence of bytes to console, extracting commands
 * in:
 *	buf	- buffer to output data from
 *	bufsize	- size of buffer
 * out:
 *	0	- all is ok
 *	-1	- all is bad
 *	>0	- count of written bytes
 */
int
console_write(uint8_t *buf, size_t bufsize)
{
	return (0);
}

/* console_read() - reads sequence of bytes from console
 * in:
 *	socket	- socket number
 *	buf	- buffer to read data to
 *	bufsize	- size of buffer
 * out:
 *	0	- all is ok, read ended
 *	-1	- all is bad
 *	>0	- count of read bytes
 */
int
console_read(uint8_t *buf, size_t bufsize)
{
	return (0);
}

/* pxe_telnet() - connects via telnet protocol to host and interacts with user
 * in:
 *	addr - ip address of remote host
 *	port - remote port to connect to
 * out:
 *	-1	- something failed
 *	0	- all was ok
 */
int
pxe_telnet(PXE_IPADDR *addr, uint16_t port)
{
	uint8_t	buf[PXE_TELNET_BUFSIZE];
	int	byte_count = 0;
	int	on = 1;
	int	s = pxe_socket();
	
	if (s == -1) /* failed to create socket */
		return (-1);
		
	int result = pxe_connect(s, addr, port);
	
	if (result == -1) { /* failed to connect */
		pxe_close(s);
		return (-1);
	}
	
	fsmbuild(); /* set up finite state machines */

	while (1) {
		/* remote server input, TODO: nonblocking socket read */
		byte_count = pxe_recv(s, buf, sizeof(buf), PXE_SOCK_NONBLOCKING);
		
		if (byte_count < 0) {
			printf("\nerror: socket read failed\n");
			break;
		} else if ((byte_count == 0) && (pxe_sock_state(s) != PXE_SOCKET_CONNECTED)) {
			printf("\nconnection closed.\n");
			return (0);
		} else { /* local console output */
			console_write(buf, byte_count);
		}

		/* local console input */
		byte_count = console_read(buf, sizeof(buf));
		
		if (byte_count < 0) {
			printf("\nerror: tty read failed\n");
			break;
		} else if (byte_count == 0) {
			pxe_close(s);
			return (0)
		} else {
			/* output to remote server */
			telnet_write(s, buf, byte_count);
		}
	}
	
	/* any error occured */
	pxe_close(s);
		
	return (-1);
}
