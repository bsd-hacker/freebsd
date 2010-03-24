# $FreeBSD$

.include <bsd.own.mk>

SUBDIR= csu libgcc libgcov libdialog libregex libreadline libssp

# XXX/juli
# The values to substitute for libgomp are elusive for MIPS N64.  Using the
# definitions used by other LP64 architectures does not seem to work.  It seems
# that not building it is the best available choice for now.  Will run the
# configure script and see what it comes up with on a running system.
.if ${MACHINE_ARCH} != "mips" || !defined(TARGET_ABI) || ${TARGET_ABI} != "n64"
SUBDIR+= libgomp
.endif

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_CXX} != "no"
SUBDIR+= libstdc++ libsupc++
.endif

.if ${MK_OBJC} != "no"
SUBDIR+= libobjc
.endif

.include <bsd.subdir.mk>
