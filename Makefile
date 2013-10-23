ALL:    build

build:
	@echo "Building individual chapters..."
	cd 01.intro; make
	cd 02.entering_kernel; make
	cd 03.processes\&threads; make
	cd 04.synchronisation; make
	cd 05.memory; make

course:
	@echo "Creating combined pdf..."
	pdfjoin 01.intro/lection.pdf \
		02.entering_kernel/lection.pdf \
		03.processes\&threads/lection.pdf \
		04.synchronisation/lection.pdf \
		05.memory/lection.pdf \
		-o course.pdf

clean:
	@echo "Cleanup temp files..."
	cd 01.intro; make clean
	cd 02.entering_kernel; make clean
	cd 03.processes\&threads; make clean
	cd 04.synchronisation; make clean
	cd 05.memory; make clean

cleanall:
	@echo "Cleanup all files..."
	cd 01.intro; make cleanall
	cd 02.entering_kernel; make cleanall
	cd 03.processes\&threads; make cleanall
	cd 04.synchronisation; make cleanall
	cd 05.memory; make cleanall
	rm -f course.pdf
