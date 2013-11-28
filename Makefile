LECTURES=	01.intro \
		02.entering_kernel \
		03.processes\&threads \
		04.synchronisation \
		05.memory \
		06.filedesc \
		07.io \
		08.io2

.MAIN: build

build:
	@echo "Building individual chapters..."
	@for l in ${LECTURES}; do \
		cd $${l}; make; cd -; \
	done

course:
	@echo "Creating combined pdf..."
	@PDFS=""; \
	for l in ${LECTURES}; do \
		PDFS="$${PDFS} $${l}/lection.pdf"; \
	done; \
	pdfjoin $${PDFS} -o course.pdf

clean:
	@echo "Cleanup temp files..."
	@for l in ${LECTURES}; do \
		cd $${l}; make ${.TARGET}; cd -; \
	done

cleanall:
	@echo "Cleanup all files..."
	@for l in ${LECTURES}; do \
		cd $${l}; make ${.TARGET}; cd -; \
	done
	rm -f course.pdf
