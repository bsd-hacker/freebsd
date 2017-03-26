# $FreeBSD$

.include <src.opts.mk>

PROG_CXX=	llvm-as

SRCDIR=		tools/llvm-as
SRCS+=		llvm-as.cpp

LIBADD+=	z
 
.if ${MK_LLVM_AS_IS_AS} != "no"
SYMLINKS=	${PROG_CXX} ${BINDIR}/as
.endif

.include "../llvm.prog.mk"
