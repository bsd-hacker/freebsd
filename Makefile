# $FreeBSD$

DIRS=head 
STABLE=7 6
RELENG=7.0 6.3 6.2 6.1 6.0

build:
.for d in ${DIRS} ${STABLE:S/^/stable-/} ${RELENG:S/^/releng-/} 
	@echo "==>> BUILDING IN ${d}"; \
	cd ${d}/release/doc/en_US.ISO8859-1/relnotes/; \
	make
.endfor

clean:
.for d in ${DIRS} ${STABLE:S/^/stable-/} ${RELENG:S/^/releng-/} 
	@echo "==>> CLEANING IN ${d}"; \
	cd ${d}/release/doc/en_US.ISO8859-1/relnotes/; \
	make clean
.endfor

perl:
.for d in ${DIRS} ${STABLE:S/^/stable-/} ${RELENG:S/^/releng-/} 
	@echo "==>> PERL IN ${d}"; \
	cd ${d}/release/doc/en_US.ISO8859-1/relnotes/; \
	make contrib.ent
.endfor

html:
	> links.html
.for d in ${DIRS} ${STABLE:S/^/stable-/} ${RELENG:S/^/releng-/} 
	@A=$$(find ${d} -name article.html | egrep '(relnotes/article|i386)'); \
	echo "<a href=\"$${A}\">${d}</a><br>" >> links.html
.endfor

edit:
	vi */release/doc/en_US*/relnotes/common/new.sgml */*/*/*/relnotes/article.sgml
