/*-
 * Copyright (c) 2015-2017 Nuxi, https://nuxi.nl/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syscallsubr.h>
#include <sys/systm.h>
#include <sys/un.h>

#include <net/vnet.h>

#include <netinet/in.h>

#include <contrib/cloudabi/cloudabi_types_common.h>

#include <compat/cloudabi/cloudabi_proto.h>
#include <compat/cloudabi/cloudabi_util.h>

/* Copies a pathname into a UNIX socket address structure. */
static int
copyin_sockaddr_un(const char *path, size_t pathlen, struct sockaddr_un *sun)
{
	int error;

	/* Copy in pathname string if there's enough space. */
	if (pathlen >= sizeof(sun->sun_path))
		return (ENAMETOOLONG);
	error = copyin(path, &sun->sun_path, pathlen);
	if (error != 0)
		return (error);
	if (memchr(sun->sun_path, '\0', pathlen) != NULL)
		return (EINVAL);

	/* Initialize the rest of the socket address. */
	sun->sun_path[pathlen] = '\0';
	sun->sun_family = AF_UNIX;
	sun->sun_len = sizeof(*sun);
	return (0);
}

int
cloudabi_sys_sock_accept(struct thread *td,
    struct cloudabi_sys_sock_accept_args *uap)
{

	return (kern_accept(td, uap->sock, NULL, NULL, NULL));
}

int
cloudabi_sys_sock_bind(struct thread *td,
    struct cloudabi_sys_sock_bind_args *uap)
{
	struct sockaddr_un sun;
	int error;

	error = copyin_sockaddr_un(uap->path, uap->path_len, &sun);
	if (error != 0)
		return (error);
	return (kern_bindat(td, uap->fd, uap->sock, (struct sockaddr *)&sun));
}

int
cloudabi_sys_sock_connect(struct thread *td,
    struct cloudabi_sys_sock_connect_args *uap)
{
	struct sockaddr_un sun;
	int error;

	error = copyin_sockaddr_un(uap->path, uap->path_len, &sun);
	if (error != 0)
		return (error);
	return (kern_connectat(td, uap->fd, uap->sock,
	    (struct sockaddr *)&sun));
}

int
cloudabi_sys_sock_listen(struct thread *td,
    struct cloudabi_sys_sock_listen_args *uap)
{

	return (kern_listen(td, uap->sock, uap->backlog));
}

int
cloudabi_sys_sock_shutdown(struct thread *td,
    struct cloudabi_sys_sock_shutdown_args *uap)
{
	int how;

	switch (uap->how) {
	case CLOUDABI_SHUT_RD:
		how = SHUT_RD;
		break;
	case CLOUDABI_SHUT_WR:
		how = SHUT_WR;
		break;
	case CLOUDABI_SHUT_RD | CLOUDABI_SHUT_WR:
		how = SHUT_RDWR;
		break;
	default:
		return (EINVAL);
	}

	return (kern_shutdown(td, uap->sock, how));
}

int
cloudabi_sys_sock_stat_get(struct thread *td,
    struct cloudabi_sys_sock_stat_get_args *uap)
{
	cloudabi_sockstat_t ss = {};
	cap_rights_t rights;
	struct file *fp;
	struct socket *so;
	int error;

	error = getsock_cap(td, uap->sock, cap_rights_init(&rights,
	    CAP_GETSOCKOPT, CAP_GETPEERNAME, CAP_GETSOCKNAME), &fp, NULL, NULL);
	if (error != 0)
		return (error);
	so = fp->f_data;

	/* Set ss_error. */
	SOCK_LOCK(so);
	ss.ss_error = cloudabi_convert_errno(so->so_error);
	if ((uap->flags & CLOUDABI_SOCKSTAT_CLEAR_ERROR) != 0)
		so->so_error = 0;
	SOCK_UNLOCK(so);

	/* Set ss_state. */
	if ((so->so_options & SO_ACCEPTCONN) != 0)
		ss.ss_state |= CLOUDABI_SOCKSTATE_ACCEPTCONN;

	fdrop(fp, td);
	return (copyout(&ss, uap->buf, sizeof(ss)));
}

int
cloudabi_sock_recv(struct thread *td, cloudabi_fd_t fd, struct iovec *data,
    size_t datalen, cloudabi_fd_t *fds, size_t fdslen,
    cloudabi_riflags_t flags, size_t *rdatalen, size_t *rfdslen,
    cloudabi_roflags_t *rflags)
{
	struct sockaddr_storage ss;
	struct msghdr hdr = {
		.msg_name = &ss,
		.msg_namelen = sizeof(ss),
		.msg_iov = data,
		.msg_iovlen = datalen,
	};
	struct mbuf *control;
	int error;

	/* Convert flags. */
	if (flags & CLOUDABI_SOCK_RECV_PEEK)
		hdr.msg_flags |= MSG_PEEK;
	if (flags & CLOUDABI_SOCK_RECV_WAITALL)
		hdr.msg_flags |= MSG_WAITALL;

	control = NULL;
	error = kern_recvit(td, fd, &hdr, UIO_SYSSPACE,
	    fdslen > 0 ? &control : NULL);
	if (error != 0)
		return (error);

	/* Convert return values. */
	*rdatalen = td->td_retval[0];
	td->td_retval[0] = 0;
	*rfdslen = 0;
	*rflags = 0;
	if (hdr.msg_flags & MSG_TRUNC)
		*rflags |= CLOUDABI_SOCK_RECV_DATA_TRUNCATED;

	/* Extract file descriptors from SCM_RIGHTS messages. */
	if (control != NULL) {
		struct cmsghdr *chdr;

		hdr.msg_control = mtod(control, void *);
		hdr.msg_controllen = control->m_len;
		for (chdr = CMSG_FIRSTHDR(&hdr); chdr != NULL;
		    chdr = CMSG_NXTHDR(&hdr, chdr)) {
			if (chdr->cmsg_level == SOL_SOCKET &&
			    chdr->cmsg_type == SCM_RIGHTS) {
				size_t nfds;

				nfds = (chdr->cmsg_len - CMSG_LEN(0)) /
				    sizeof(int);
				if (nfds > fdslen) {
					/* Unable to store file descriptors. */
					nfds = fdslen;
					*rflags |=
					    CLOUDABI_SOCK_RECV_FDS_TRUNCATED;
				}
				error = copyout(CMSG_DATA(chdr), fds,
				    nfds * sizeof(int));
				if (error != 0) {
					m_free(control);
					return (error);
				}
				fds += nfds;
				fdslen -= nfds;
				*rfdslen += nfds;
			}
		}
		m_free(control);
	}
	return (0);
}

int
cloudabi_sock_send(struct thread *td, cloudabi_fd_t fd, struct iovec *data,
    size_t datalen, const cloudabi_fd_t *fds, size_t fdslen, size_t *rdatalen)
{
	struct msghdr hdr = {
		.msg_iov = data,
		.msg_iovlen = datalen,
	};
	struct mbuf *control;
	int error;

	/* Convert file descriptor array to an SCM_RIGHTS message. */
	if (fdslen > MCLBYTES || CMSG_SPACE(fdslen * sizeof(int)) > MCLBYTES) {
		return (EINVAL);
	} else if (fdslen > 0) {
		struct cmsghdr *chdr;

		control = m_get2(CMSG_SPACE(fdslen * sizeof(int)),
		    M_WAITOK, MT_CONTROL, 0);
		control->m_len = CMSG_SPACE(fdslen * sizeof(int));

		chdr = mtod(control, struct cmsghdr *);
		chdr->cmsg_len = CMSG_LEN(fdslen * sizeof(int));
		chdr->cmsg_level = SOL_SOCKET;
		chdr->cmsg_type = SCM_RIGHTS;
		error = copyin(fds, CMSG_DATA(chdr), fdslen * sizeof(int));
		if (error != 0) {
			m_free(control);
			return (error);
		}
	} else {
		control = NULL;
	}

	error = kern_sendit(td, fd, &hdr, MSG_NOSIGNAL, control, UIO_USERSPACE);
	if (error != 0)
		return (error);
	*rdatalen = td->td_retval[0];
	td->td_retval[0] = 0;
	return (0);
}
