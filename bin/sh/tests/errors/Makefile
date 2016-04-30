# $FreeBSD$

PACKAGE=	tests
TESTSPACKAGE=	${PACKAGE}
TESTSDIR=	${TESTSBASE}/bin/sh/${.CURDIR:T}

.PATH: ${.CURDIR:H}
ATF_TESTS_SH=	functional_test

TESTS=		assignment-error1.0
TESTS+=		assignment-error2.0
TESTS+=		backquote-error1.0
TESTS+=		backquote-error2.0
TESTS+=		bad-binary1.126
TESTS+=		bad-keyword1.0
TESTS+=		bad-parm-exp1.0
TESTS+=		bad-parm-exp2.2 bad-parm-exp2.2.stderr
TESTS+=		bad-parm-exp3.2 bad-parm-exp3.2.stderr
TESTS+=		bad-parm-exp4.2 bad-parm-exp4.2.stderr
TESTS+=		bad-parm-exp5.2 bad-parm-exp5.2.stderr
TESTS+=		bad-parm-exp6.2 bad-parm-exp6.2.stderr
TESTS+=		bad-parm-exp7.0
TESTS+=		bad-parm-exp8.0
TESTS+=		option-error.0
TESTS+=		redirection-error.0
TESTS+=		redirection-error2.2
TESTS+=		redirection-error3.0
TESTS+=		redirection-error4.0
TESTS+=		redirection-error5.0
TESTS+=		redirection-error6.0
TESTS+=		redirection-error7.0
TESTS+=		write-error1.0

.include <bsd.test.mk>
