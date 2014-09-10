.PATH: ${.CURDIR:H:H:H:H:H}/contrib/pjdfstest/tests/${.CURDIR:T}

KYUAFILE=	no

.for script in ${SCRIPTS}
.if ${script:E} == "t"
SCRIPTSNAME_${script}=	${script}
.endif
.endfor
