/*-
 * Copyright (c) 2005-2007, Joseph Koshy
 * Copyright (c) 2007 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
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

/*
 * Transform a hwpmc(4) log into human readable form, and into
 * gprof(1) compatible profiles.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/gmon.h>
#include <sys/imgact_aout.h>
#include <sys/imgact_elf.h>
#include <sys/mman.h>
#include <sys/pmc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <libgen.h>
#include <limits.h>
#include <netdb.h>
#include <pmc.h>
#include <pmclog.h>
#include <sysexits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pmcstat.h"

#define	min(A,B)		((A) < (B) ? (A) : (B))
#define	max(A,B)		((A) > (B) ? (A) : (B))

#define	PMCSTAT_ALLOCATE		1

#define PMCSTAT_PC_RESERVED	0

/*
 * PUBLIC INTERFACES
 *
 * pmcstat_ct_initialize_logging()	initialize this module, called first
 * pmcstat_ct_shutdown_logging()		orderly shutdown, called last
 * pmcstat_ct_open_log()			open an eventlog for processing
 * pmcstat_ct_process_log()		print/convert an event log
 * pmcstat_ct_close_log()			finish processing an event log
 *
 * IMPLEMENTATION NOTES
 *
 * We correlate each 'callchain' or 'sample' entry seen in the event
 * log back to an executable object in the system. Executable objects
 * include:
 * 	- program executables,
 *	- shared libraries loaded by the runtime loader,
 *	- dlopen()'ed objects loaded by the program,
 *	- the runtime loader itself,
 *	- the kernel and kernel modules.
 *
 * Each process that we know about is treated as a set of regions that
 * map to executable objects.  Processes are described by
 * 'pmcstat_process' structures.  Executable objects are tracked by
 * 'pmcstat_image' structures.  The kernel and kernel modules are
 * common to all processes (they reside at the same virtual addresses
 * for all processes).  Individual processes can have their text
 * segments and shared libraries loaded at process-specific locations.
 *
 * A given executable object can be in use by multiple processes
 * (e.g., libc.so) and loaded at a different address in each.
 * pmcstat_pcmap structures track per-image mappings.
 *
 * The sample log could have samples from multiple PMCs; we
 * generate one 'gmon.out' profile per PMC.
 *
 */

typedef const void *pmcstat_interned_string;

/*
 * 'pmcstat_pmcrecord' is a mapping from PMC ids to human-readable
 * names.
 */

struct pmcstat_pmcrecord {
	LIST_ENTRY(pmcstat_pmcrecord)	pr_next;
	pmc_id_t						pr_pmcid;
	pmcstat_interned_string			pr_pmcname;
	unsigned int					pr_index;
};

static unsigned int pmcstat_npmcs = 0;
static LIST_HEAD(,pmcstat_pmcrecord)	pmcstat_pmcs = LIST_HEAD_INITIALIZER(&pmcstat_pmcs);
static int pmcstat_mergepmc = 1;

/*
 * A 'pmcstat_image' structure describes an executable program on
 * disk.  'pi_execpath' is a cookie representing the pathname of
 * the executable.  'pi_start' and 'pi_end' are the least and greatest
 * virtual addresses for the text segments in the executable.
 * 'pi_gmonlist' contains a linked list of gmon.out files associated
 * with this image.
 */

enum pmcstat_image_type {
	PMCSTAT_IMAGE_UNKNOWN = 0,	/* never looked at the image */
	PMCSTAT_IMAGE_INDETERMINABLE,	/* can't tell what the image is */
	PMCSTAT_IMAGE_ELF32,		/* ELF 32 bit object */
	PMCSTAT_IMAGE_ELF64,		/* ELF 64 bit object */
	PMCSTAT_IMAGE_AOUT		/* AOUT object */
};

struct pmcstat_image {
	LIST_ENTRY(pmcstat_image) pi_next;	/* hash link */
	TAILQ_ENTRY(pmcstat_image) pi_lru;	/* LRU list */
	pmcstat_interned_string	pi_execpath;    /* cookie */
	pmcstat_interned_string pi_samplename;  /* sample path name */
	pmcstat_interned_string pi_fullpath;    /* path to FS object */
	pmcstat_interned_string pi_name;		/* file name */
	enum pmcstat_image_type pi_type;	/* executable type */

	/*
	 * Executables have pi_start and pi_end; these are zero
	 * for shared libraries.
	 */
	uintfptr_t	pi_start;	/* start address (inclusive) */
	uintfptr_t	pi_end;		/* end address (exclusive) */
	uintfptr_t	pi_entry;	/* entry address */
	uintfptr_t	pi_vaddr;	/* virtual address where loaded */
	int		pi_isdynamic;	/* whether a dynamic object */
	int		pi_iskernelmodule;
	pmcstat_interned_string pi_dynlinkerpath; /* path in .interp */

	/* All symbols associated with this object. */
	struct pmcstat_symbol *pi_symbols;
	size_t		pi_symcount;

};

/*
 * All image descriptors are kept in a hash table.
 */
static LIST_HEAD(,pmcstat_image)	pmcstat_image_hash[PMCSTAT_NHASH];

/*
 * A 'pmcstat_pcmap' structure maps a virtual address range to an
 * underlying 'pmcstat_image' descriptor.
 */
struct pmcstat_pcmap {
	TAILQ_ENTRY(pmcstat_pcmap) ppm_next;
	uintfptr_t	ppm_lowpc;
	uintfptr_t	ppm_highpc;
	struct pmcstat_image *ppm_image;
};

/*
 * A 'pmcstat_process' structure models processes.  Each process is
 * associated with a set of pmcstat_pcmap structures that map
 * addresses inside it to executable objects.  This set is implemented
 * as a list, kept sorted in ascending order of mapped addresses.
 *
 * 'pp_pid' holds the pid of the process.  When a process exits, the
 * 'pp_isactive' field is set to zero, but the process structure is
 * not immediately reclaimed because there may still be samples in the
 * log for this process.
 */

struct pmcstat_process {
	LIST_ENTRY(pmcstat_process) pp_next;	/* hash-next */
	pid_t			pp_pid;		/* associated pid */
	int			pp_isactive;	/* whether active */
	uintfptr_t		pp_entryaddr;	/* entry address */
	TAILQ_HEAD(,pmcstat_pcmap) pp_map;	/* address range map */
};

/*
 * All process descriptors are kept in a hash table.
 */
static LIST_HEAD(,pmcstat_process) pmcstat_process_hash[PMCSTAT_NHASH];

static struct pmcstat_process *pmcstat_kernproc; /* kernel 'process' */

/*
 * Each function symbol tracked by pmcstat(8).
 */

struct pmcstat_symbol {
	pmcstat_interned_string ps_name;
	uint64_t	ps_start;
	uint64_t	ps_end;
};

static pmcstat_interned_string pmcstat_previous_filename_printed;

struct pmcstat_ctnode;

struct pmcstat_ctarc {
        uint32_t		pcta_child_count_c;
	uint32_t		*pcta_child_count;
	struct pmcstat_ctnode	*pcta_child;
};

/*
 * Each call tree node is tracked by a pmcstat_ctnode struct.
 */
struct pmcstat_ctnode {
	struct pmcstat_image	*pct_image;
	uintfptr_t		pct_func;
	uint32_t		pct_self_count_c;
	uint32_t 		*pct_self_count;

	uint32_t		pct_narc;
	uint32_t		pct_arc_c;
	struct pmcstat_ctarc 	*pct_arc;
};

struct pmcstat_ctnode_hash {
	struct pmcstat_ctnode  *pch_ctnode;
	LIST_ENTRY(pmcstat_ctnode_hash) pch_next;
};

/*
 * All nodes indexed by function/image name are placed in a hash table.
 */
static LIST_HEAD(,pmcstat_ctnode_hash) pmcstat_ctnode_hash[PMCSTAT_NHASH];

/* Misc. statistics */
static struct pmcstat_stats {
	int ps_exec_aout;	/* # a.out executables seen */
	int ps_exec_elf;	/* # elf executables seen */
	int ps_exec_errors;	/* # errors processing executables */
	int ps_exec_indeterminable; /* # unknown executables seen */
	int ps_samples_total;	/* total number of samples processed */
	int ps_samples_skipped; /* #samples filtered out for any reason */
	int ps_samples_unknown_offset;	/* #samples of rank 0 not in a map */
	int ps_samples_indeterminable;	/* #samples in indeterminable images */
	int ps_callchain_dubious_frames;/* #dubious frame pointers seen */
	int ps_callchain_single_frames; /* #single frame seen */
	/* TODO: add stats for specific calltree error
	 */
} pmcstat_stats;


/*
 * Prototypes
 */

static void pmcstat_image_determine_type(struct pmcstat_image *_image,
    struct pmcstat_args *_a);
static struct pmcstat_image *pmcstat_image_from_path(pmcstat_interned_string
    _path, int _iskernelmodule);
static void pmcstat_image_get_aout_params(struct pmcstat_image *_image,
    struct pmcstat_args *_a);
static void pmcstat_image_get_elf_params(struct pmcstat_image *_image,
    struct pmcstat_args *_a);
static void	pmcstat_image_link(struct pmcstat_process *_pp,
    struct pmcstat_image *_i, uintfptr_t _lpc);

static void	pmcstat_pmcid_add(pmc_id_t _pmcid, pmcstat_interned_string _name);
static void	pmcstat_process_aout_exec(struct pmcstat_process *_pp,
    struct pmcstat_image *_image, uintfptr_t _entryaddr,
    struct pmcstat_args *_a);
static void	pmcstat_process_elf_exec(struct pmcstat_process *_pp,
    struct pmcstat_image *_image, uintfptr_t _entryaddr,
    struct pmcstat_args *_a);
static void	pmcstat_process_exec(struct pmcstat_process *_pp,
    pmcstat_interned_string _path, uintfptr_t _entryaddr,
    struct pmcstat_args *_ao);
static struct pmcstat_process *pmcstat_process_lookup(pid_t _pid,
    int _allocate);
static struct pmcstat_pcmap *pmcstat_process_find_map(
    struct pmcstat_process *_p, uintfptr_t _pc);

static int	pmcstat_string_compute_hash(const char *_string);
static void pmcstat_string_initialize(void);
static pmcstat_interned_string pmcstat_string_intern(const char *_s);
static pmcstat_interned_string pmcstat_string_lookup(const char *_s);
static int	pmcstat_string_lookup_hash(pmcstat_interned_string _is);
static void pmcstat_string_shutdown(void);
static const char *pmcstat_string_unintern(pmcstat_interned_string _is);


/*
 * A simple implementation of interned strings.  Each interned string
 * is assigned a unique address, so that subsequent string compares
 * can be done by a simple pointer comparision instead of using
 * strcmp().  This speeds up hash table lookups and saves memory if
 * duplicate strings are the norm.
 */
struct pmcstat_string {
	LIST_ENTRY(pmcstat_string)	ps_next;	/* hash link */
	int		ps_len;
	int		ps_hash;
	char		*ps_string;
};

static LIST_HEAD(,pmcstat_string)	pmcstat_string_hash[PMCSTAT_NHASH];


/*
 * Block realloc items
 */
static void
pmcstat_growit(uint32_t item, uint32_t *count, uint32_t size, void **items)
{
#define GROWIT_BLOCKSIZE	4
        uint32_t	new_count;

        if (item < *count)
                return;
        
        new_count = *count + max(item + 1 - *count, GROWIT_BLOCKSIZE);
        *items = realloc(*items, new_count * size);
.        if (*items == NULL)
                errx(EX_SOFTWARE, "ERROR: out of memory");
        bzero((char *)*items + *count * size, (new_count - *count) * size);
        *count = new_count;
}

/*
 * Compute a 'hash' value for a string.
 */

static int
pmcstat_string_compute_hash(const char *s)
{
	int hash;

	for (hash = 0; *s; s++)
		hash ^= *s;

	return (hash & PMCSTAT_HASH_MASK);
}

/*
 * Intern a copy of string 's', and return a pointer to the
 * interned structure.
 */

static pmcstat_interned_string
pmcstat_string_intern(const char *s)
{
	struct pmcstat_string *ps;
	const struct pmcstat_string *cps;
	int hash, len;

	if ((cps = pmcstat_string_lookup(s)) != NULL)
		return (cps);

	hash = pmcstat_string_compute_hash(s);
	len  = strlen(s);

	if ((ps = malloc(sizeof(*ps))) == NULL)
		err(EX_OSERR, "ERROR: Could not intern string");
	ps->ps_len = len;
	ps->ps_hash = hash;
	ps->ps_string = strdup(s);
	LIST_INSERT_HEAD(&pmcstat_string_hash[hash], ps, ps_next);
	return ((pmcstat_interned_string) ps);
}

static const char *
pmcstat_string_unintern(pmcstat_interned_string str)
{
	const char *s;

	s = ((const struct pmcstat_string *) str)->ps_string;
	return (s);
}

static pmcstat_interned_string
pmcstat_string_lookup(const char *s)
{
	struct pmcstat_string *ps;
	int hash, len;

	hash = pmcstat_string_compute_hash(s);
	len = strlen(s);

	LIST_FOREACH(ps, &pmcstat_string_hash[hash], ps_next)
	    if (ps->ps_len == len && ps->ps_hash == hash &&
		strcmp(ps->ps_string, s) == 0)
		    return (ps);
	return (NULL);
}

static int
pmcstat_string_lookup_hash(pmcstat_interned_string s)
{
	const struct pmcstat_string *ps;

	ps = (const struct pmcstat_string *) s;
	return (ps->ps_hash);
}

/*
 * Initialize the string interning facility.
 */

static void
pmcstat_string_initialize(void)
{
	int i;

	for (i = 0; i < PMCSTAT_NHASH; i++)
		LIST_INIT(&pmcstat_string_hash[i]);
}

/*
 * Destroy the string table, free'ing up space.
 */

static void
pmcstat_string_shutdown(void)
{
	int i;
	struct pmcstat_string *ps, *pstmp;

	for (i = 0; i < PMCSTAT_NHASH; i++)
		LIST_FOREACH_SAFE(ps, &pmcstat_string_hash[i], ps_next,
		    pstmp) {
			LIST_REMOVE(ps, ps_next);
			free(ps->ps_string);
			free(ps);
		}
}

/*
 * Determine whether a given executable image is an A.OUT object, and
 * if so, fill in its parameters from the text file.
 * Sets image->pi_type.
 */

static void
pmcstat_image_get_aout_params(struct pmcstat_image *image,
    struct pmcstat_args *a)
{
	int fd;
	ssize_t nbytes;
	struct exec ex;
	const char *path;
	char buffer[PATH_MAX];

	path = pmcstat_string_unintern(image->pi_execpath);
	assert(path != NULL);

	if (image->pi_iskernelmodule)
		errx(EX_SOFTWARE, "ERROR: a.out kernel modules are "
		    "unsupported \"%s\"", path);

	(void) snprintf(buffer, sizeof(buffer), "%s%s",
	    a->pa_fsroot, path);

	if ((fd = open(buffer, O_RDONLY, 0)) < 0 ||
	    (nbytes = read(fd, &ex, sizeof(ex))) < 0) {
		warn("WARNING: Cannot determine type of \"%s\"", path);
		image->pi_type = PMCSTAT_IMAGE_INDETERMINABLE;
		if (fd != -1)
			(void) close(fd);
		return;
	}

	(void) close(fd);

	if ((unsigned) nbytes != sizeof(ex) ||
	    N_BADMAG(ex))
		return;

	image->pi_type = PMCSTAT_IMAGE_AOUT;

	/* TODO: the rest of a.out processing */

	return;
}

/*
 * Helper function.
 */

static int
pmcstat_symbol_compare(const void *a, const void *b)
{
	const struct pmcstat_symbol *sym1, *sym2;

	sym1 = (const struct pmcstat_symbol *) a;
	sym2 = (const struct pmcstat_symbol *) b;

	if (sym1->ps_end <= sym2->ps_start)
		return (-1);
	if (sym1->ps_start >= sym2->ps_end)
		return (1);
	return (0);
}

/*
 * Map an address to a symbol in an image.
 */

static struct pmcstat_symbol *
pmcstat_symbol_search(struct pmcstat_image *image, uintfptr_t addr)
{
	struct pmcstat_symbol sym;

	if (image->pi_symbols == NULL)
		return (NULL);

	sym.ps_name  = NULL;
	sym.ps_start = addr;
	sym.ps_end   = addr + 1;

	return (bsearch((void *) &sym, image->pi_symbols,
		    image->pi_symcount, sizeof(struct pmcstat_symbol),
		    pmcstat_symbol_compare));
}

/*
 * Add the list of symbols in the given section to the list associated
 * with the object.
 */
static void
pmcstat_image_add_symbols(struct pmcstat_image *image, Elf *e,
    Elf_Scn *scn, GElf_Shdr *sh)
{
	int firsttime;
	size_t n, newsyms, nshsyms, nfuncsyms;
	struct pmcstat_symbol *symptr;
	char *fnname;
	GElf_Sym sym;
	Elf_Data *data;

	if ((data = elf_getdata(scn, NULL)) == NULL)
		return;

	/*
	 * Determine the number of functions named in this
	 * section.
	 */

	nshsyms = sh->sh_size / sh->sh_entsize;
	for (n = nfuncsyms = 0; n < nshsyms; n++) {
		if (gelf_getsym(data, (int) n, &sym) != &sym)
			return;
		if (GELF_ST_TYPE(sym.st_info) == STT_FUNC)
			nfuncsyms++;
	}

	if (nfuncsyms == 0)
		return;

	/*
	 * Allocate space for the new entries.
	 */
	firsttime = image->pi_symbols == NULL;
	symptr = realloc(image->pi_symbols,
	    sizeof(*symptr) * (image->pi_symcount + nfuncsyms));
	if (symptr == image->pi_symbols) /* realloc() failed. */
		return;
	image->pi_symbols = symptr;

	/*
	 * Append new symbols to the end of the current table.
	 */
	symptr += image->pi_symcount;

	for (n = newsyms = 0; n < nshsyms; n++) {
		if (gelf_getsym(data, (int) n, &sym) != &sym)
			return;
		if (GELF_ST_TYPE(sym.st_info) != STT_FUNC)
			continue;

		if (!firsttime && pmcstat_symbol_search(image, sym.st_value))
			continue; /* We've seen this symbol already. */

		if ((fnname = elf_strptr(e, sh->sh_link, sym.st_name))
		    == NULL)
			continue;

		symptr->ps_name  = pmcstat_string_intern(fnname);
		symptr->ps_start = sym.st_value - image->pi_vaddr;
		symptr->ps_end   = symptr->ps_start + sym.st_size;
		symptr++;

		newsyms++;
	}

	image->pi_symcount += newsyms;

	assert(newsyms <= nfuncsyms);

	/*
	 * Return space to the system if there were duplicates.
	 */
	if (newsyms < nfuncsyms)
		image->pi_symbols = realloc(image->pi_symbols,
		    sizeof(*symptr) * image->pi_symcount);

	/*
	 * Keep the list of symbols sorted.
	 */
	qsort(image->pi_symbols, image->pi_symcount, sizeof(*symptr),
	    pmcstat_symbol_compare);

	/*
	 * Deal with function symbols that have a size of 'zero' by
	 * making them extend to the next higher address.  These
	 * symbols are usually defined in assembly code.
	 */
	for (symptr = image->pi_symbols;
	     symptr < image->pi_symbols + (image->pi_symcount - 1);
	     symptr++)
		if (symptr->ps_start == symptr->ps_end)
			symptr->ps_end = (symptr+1)->ps_start;
}

/*
 * Examine an ELF file to determine the size of its text segment.
 * Sets image->pi_type if anything conclusive can be determined about
 * this image.
 */

static void
pmcstat_image_get_elf_params(struct pmcstat_image *image,
    struct pmcstat_args *a)
{
	int fd;
	size_t i, nph, nsh;
	const char *path, *elfbase;
	uintfptr_t minva, maxva;
	Elf *e;
	Elf_Scn *scn;
	GElf_Ehdr eh;
	GElf_Phdr ph;
	GElf_Shdr sh;
	enum pmcstat_image_type image_type;
	char buffer[PATH_MAX], *p, *q;

	assert(image->pi_type == PMCSTAT_IMAGE_UNKNOWN);

	image->pi_start = minva = ~(uintfptr_t) 0;
	image->pi_end = maxva = (uintfptr_t) 0;
	image->pi_type = image_type = PMCSTAT_IMAGE_INDETERMINABLE;
	image->pi_isdynamic = 0;
	image->pi_dynlinkerpath = NULL;
	image->pi_vaddr = 0;

	path = pmcstat_string_unintern(image->pi_execpath);
	assert(path != NULL);

	/*
	 * Look for kernel modules under FSROOT/KERNELPATH/NAME,
	 * and user mode executable objects under FSROOT/PATHNAME.
	 */
	if (image->pi_iskernelmodule)
		(void) snprintf(buffer, sizeof(buffer), "%s%s/%s",
		    a->pa_fsroot, a->pa_kernel, path);
	else
		(void) snprintf(buffer, sizeof(buffer), "%s%s",
		    a->pa_fsroot, path);

	e = NULL;
	if ((fd = open(buffer, O_RDONLY, 0)) < 0 ||
	    (e = elf_begin(fd, ELF_C_READ, NULL)) == NULL ||
	    (elf_kind(e) != ELF_K_ELF)) {
		warnx("WARNING: Cannot determine the type of \"%s\".",
		    buffer);
		goto done;
	}

	if (gelf_getehdr(e, &eh) != &eh) {
		warnx("WARNING: Cannot retrieve the ELF Header for "
		    "\"%s\": %s.", buffer, elf_errmsg(-1));
		goto done;
	}

	if (eh.e_type != ET_EXEC && eh.e_type != ET_DYN &&
	    !(image->pi_iskernelmodule && eh.e_type == ET_REL)) {
		warnx("WARNING: \"%s\" is of an unsupported ELF type.",
		    buffer);
		goto done;
	}

	image_type = eh.e_ident[EI_CLASS] == ELFCLASS32 ?
	    PMCSTAT_IMAGE_ELF32 : PMCSTAT_IMAGE_ELF64;

	/*
	 * Determine the virtual address where an executable would be
	 * loaded.  Additionally, for dynamically linked executables,
	 * save the pathname to the runtime linker.
	 */
	if (eh.e_type == ET_EXEC) {
		if (elf_getphnum(e, &nph) == 0) {
			warnx("WARNING: Could not determine the number of "
			    "program headers in \"%s\": %s.", buffer,
			    elf_errmsg(-1));
			goto done;
		}
		for (i = 0; i < eh.e_phnum; i++) {
			if (gelf_getphdr(e, i, &ph) != &ph) {
				warnx("WARNING: Retrieval of PHDR entry #%ju "
				    "in \"%s\" failed: %s.", (uintmax_t) i,
				    buffer, elf_errmsg(-1));
				goto done;
			}
			switch (ph.p_type) {
			case PT_DYNAMIC:
				image->pi_isdynamic = 1;
				break;
			case PT_INTERP:
				if ((elfbase = elf_rawfile(e, NULL)) == NULL) {
					warnx("WARNING: Cannot retrieve the "
					    "interpreter for \"%s\": %s.",
					    buffer, elf_errmsg(-1));
					goto done;
				}
				image->pi_dynlinkerpath =
				    pmcstat_string_intern(elfbase +
					ph.p_offset);
				break;
			case PT_LOAD:
				if (ph.p_offset == 0)
					image->pi_vaddr = ph.p_vaddr;
				break;
			}
		}
	}

	/*
	 * Get the min and max VA associated with this ELF object.
	 */
	if (elf_getshnum(e, &nsh) == 0) {
		warnx("WARNING: Could not determine the number of sections "
		    "for \"%s\": %s.", buffer, elf_errmsg(-1));
		goto done;
	}

	for (i = 0; i < nsh; i++) {
		if ((scn = elf_getscn(e, i)) == NULL ||
		    gelf_getshdr(scn, &sh) != &sh) {
			warnx("WARNING: Could not retrieve section header "
			    "#%ju in \"%s\": %s.", (uintmax_t) i, buffer,
			    elf_errmsg(-1));
			goto done;
		}
		if (sh.sh_flags & SHF_EXECINSTR) {
			minva = min(minva, sh.sh_addr);
			maxva = max(maxva, sh.sh_addr + sh.sh_size);
		}
		if (sh.sh_type == SHT_SYMTAB || sh.sh_type == SHT_DYNSYM)
			pmcstat_image_add_symbols(image, e, scn, &sh);
	}

	image->pi_start = minva;
	image->pi_end   = maxva;
	image->pi_type  = image_type;
	image->pi_fullpath = pmcstat_string_intern(buffer);

	for (p = q = buffer; *p ; p++) {
		if ( *p == '\\' || *p == '/' )
			q = p;
	}
	image->pi_name = pmcstat_string_intern(q);
 done:
	(void) elf_end(e);
	if (fd >= 0)
		(void) close(fd);
	return;
}

/*
 * Given an image descriptor, determine whether it is an ELF, or AOUT.
 * If no handler claims the image, set its type to 'INDETERMINABLE'.
 */

static void
pmcstat_image_determine_type(struct pmcstat_image *image,
    struct pmcstat_args *a)
{
	assert(image->pi_type == PMCSTAT_IMAGE_UNKNOWN);

	/* Try each kind of handler in turn */
	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		pmcstat_image_get_elf_params(image, a);
	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		pmcstat_image_get_aout_params(image, a);

	/*
	 * Otherwise, remember that we tried to determine
	 * the object's type and had failed.
	 */
	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		image->pi_type = PMCSTAT_IMAGE_INDETERMINABLE;
}

/*
 * Locate an image descriptor given an interned path, adding a fresh
 * descriptor to the cache if necessary.  This function also finds a
 * suitable name for this image's sample file.
 *
 * We defer filling in the file format specific parts of the image
 * structure till the time we actually see a sample that would fall
 * into this image.
 */

static struct pmcstat_image *
pmcstat_image_from_path(pmcstat_interned_string internedpath,
    int iskernelmodule)
{
	int hash;
	struct pmcstat_image *pi;

	hash = pmcstat_string_lookup_hash(internedpath);

	/* First, look for an existing entry. */
	LIST_FOREACH(pi, &pmcstat_image_hash[hash], pi_next)
	    if (pi->pi_execpath == internedpath &&
		  pi->pi_iskernelmodule == iskernelmodule)
		    return (pi);

	/*
	 * Allocate a new entry and place it at the head of the hash
	 * and LRU lists.
	 */
	pi = malloc(sizeof(*pi));
	if (pi == NULL)
		return (NULL);

	pi->pi_type = PMCSTAT_IMAGE_UNKNOWN;
	pi->pi_execpath = internedpath;
	pi->pi_start = ~0;
	pi->pi_end = 0;
	pi->pi_entry = 0;
	pi->pi_vaddr = 0;
	pi->pi_isdynamic = 0;
	pi->pi_iskernelmodule = iskernelmodule;
	pi->pi_dynlinkerpath = NULL;
	pi->pi_symbols = NULL;
	pi->pi_symcount = 0;

	LIST_INSERT_HEAD(&pmcstat_image_hash[hash], pi, pi_next);

	return (pi);
}

/*
 * Record the fact that PC values from 'start' to 'end' come from
 * image 'image'.
 */

static void
pmcstat_image_link(struct pmcstat_process *pp, struct pmcstat_image *image,
    uintfptr_t start)
{
	struct pmcstat_pcmap *pcm, *pcmnew;
	uintfptr_t offset;

	assert(image->pi_type != PMCSTAT_IMAGE_UNKNOWN &&
	    image->pi_type != PMCSTAT_IMAGE_INDETERMINABLE);

	if ((pcmnew = malloc(sizeof(*pcmnew))) == NULL)
		err(EX_OSERR, "ERROR: Cannot create a map entry");

	/*
	 * Adjust the map entry to only cover the text portion
	 * of the object.
	 */

	offset = start - image->pi_vaddr;
	pcmnew->ppm_lowpc  = image->pi_start + offset;
	pcmnew->ppm_highpc = image->pi_end + offset;
	pcmnew->ppm_image  = image;

	assert(pcmnew->ppm_lowpc < pcmnew->ppm_highpc);

	/* Overlapped mmap()'s are assumed to never occur. */
	TAILQ_FOREACH(pcm, &pp->pp_map, ppm_next)
	    if (pcm->ppm_lowpc >= pcmnew->ppm_highpc)
		    break;

	if (pcm == NULL)
		TAILQ_INSERT_TAIL(&pp->pp_map, pcmnew, ppm_next);
	else
		TAILQ_INSERT_BEFORE(pcm, pcmnew, ppm_next);
}

/*
 * Unmap images in the range [start..end) associated with process
 * 'pp'.
 */

static void
pmcstat_image_unmap(struct pmcstat_process *pp, uintfptr_t start,
    uintfptr_t end)
{
	struct pmcstat_pcmap *pcm, *pcmtmp, *pcmnew;

	assert(pp != NULL);
	assert(start < end);

	/*
	 * Cases:
	 * - we could have the range completely in the middle of an
	 *   existing pcmap; in this case we have to split the pcmap
	 *   structure into two (i.e., generate a 'hole').
	 * - we could have the range covering multiple pcmaps; these
	 *   will have to be removed.
	 * - we could have either 'start' or 'end' falling in the
	 *   middle of a pcmap; in this case shorten the entry.
	 */
	TAILQ_FOREACH_SAFE(pcm, &pp->pp_map, ppm_next, pcmtmp) {
		assert(pcm->ppm_lowpc < pcm->ppm_highpc);
		if (pcm->ppm_highpc <= start)
			continue;
		if (pcm->ppm_lowpc >= end)
			return;
		if (pcm->ppm_lowpc >= start && pcm->ppm_highpc <= end) {
			/*
			 * The current pcmap is completely inside the
			 * unmapped range: remove it entirely.
			 */
			TAILQ_REMOVE(&pp->pp_map, pcm, ppm_next);
			free(pcm);
		} else if (pcm->ppm_lowpc < start && pcm->ppm_highpc > end) {
			/*
			 * Split this pcmap into two; curtail the
			 * current map to end at [start-1], and start
			 * the new one at [end].
			 */
			if ((pcmnew = malloc(sizeof(*pcmnew))) == NULL)
				err(EX_OSERR, "ERROR: Cannot split a map "
				    "entry");

			pcmnew->ppm_image = pcm->ppm_image;

			pcmnew->ppm_lowpc = end;
			pcmnew->ppm_highpc = pcm->ppm_highpc;

			pcm->ppm_highpc = start;

			TAILQ_INSERT_AFTER(&pp->pp_map, pcm, pcmnew, ppm_next);

			return;
		} else if (pcm->ppm_lowpc < start && pcm->ppm_highpc <= end)
			pcm->ppm_highpc = start;
		else if (pcm->ppm_lowpc >= start && pcm->ppm_highpc > end)
			pcm->ppm_lowpc = end;
		else
			assert(0);
	}
}

/*
 * Add a {pmcid,name} mapping.
 */

static void
pmcstat_pmcid_add(pmc_id_t pmcid, pmcstat_interned_string ps)
{
	struct pmcstat_pmcrecord *pr;
	int max_index = -1, name_index = -1;
	
	/* Replace an existing name for the PMC. */
	LIST_FOREACH(pr, &pmcstat_pmcs, pr_next) {
	    if (pr->pr_pmcid == pmcid) {
		    pr->pr_pmcname = ps;
		    return;
	    }
	    if (pmcstat_mergepmc && ps == pr->pr_pmcname)
	    	name_index = pr->pr_index;
		if ((int)pr->pr_index > max_index)
			max_index = pr->pr_index;
	}
	max_index++;
	/*
	 * Otherwise, allocate a new descriptor and create the
	 * appropriate directory to hold gmon.out files.
	 */
#define PCT_MAXPMCID	128
	if ((pr = malloc(sizeof(*pr))) == NULL || (name_index < 0 && max_index >= PCT_MAXPMCID))
		err(EX_OSERR, "ERROR: Cannot allocate pmc record");

	pr->pr_pmcid = pmcid;
	pr->pr_pmcname = ps;
	pr->pr_index = name_index < 0 ? max_index : name_index;
#ifdef DEBUG
	printf("adding pmcid=%lu index=%u name=%s\n", (unsigned long)pr->pr_pmcid, pr->pr_index, pmcstat_string_unintern(ps));
#endif
	LIST_INSERT_HEAD(&pmcstat_pmcs, pr, pr_next);
	pmcstat_npmcs = pr->pr_index + 1;
}

static const char *
pmcstat_pmcid_index_to_name(unsigned int pmcid_index)
{
	struct pmcstat_pmcrecord *pr;

	LIST_FOREACH(pr, &pmcstat_pmcs, pr_next)
	    if (pr->pr_index == pmcid_index)
	    	return pmcstat_string_unintern(pr->pr_pmcname);

	err(EX_OSERR, "ERROR: cannot find pmcid name");
	return NULL;
}

static unsigned int pmcstat_pmcid_to_index(pmc_id_t pmcid)
{
	struct pmcstat_pmcrecord *pr;

	LIST_FOREACH(pr, &pmcstat_pmcs, pr_next)
	    if (pr->pr_pmcid == pmcid)
		    return pr->pr_index;
		
	err(EX_OSERR, "ERROR: invalid pmcid");
}

/*
 * Associate an AOUT image with a process.
 */

static void
pmcstat_process_aout_exec(struct pmcstat_process *pp,
    struct pmcstat_image *image, uintfptr_t entryaddr,
    struct pmcstat_args *a)
{
	(void) pp;
	(void) image;
	(void) entryaddr;
	(void) a;
	/* TODO Implement a.out handling */
}

/*
 * Associate an ELF image with a process.
 */

static void
pmcstat_process_elf_exec(struct pmcstat_process *pp,
    struct pmcstat_image *image, uintfptr_t entryaddr,
    struct pmcstat_args *a)
{
	uintmax_t libstart;
	struct pmcstat_image *rtldimage;

	assert(image->pi_type == PMCSTAT_IMAGE_ELF32 ||
	    image->pi_type == PMCSTAT_IMAGE_ELF64);

	/* Create a map entry for the base executable. */
	pmcstat_image_link(pp, image, image->pi_vaddr);

	/*
	 * For dynamically linked executables we need to determine
	 * where the dynamic linker was mapped to for this process,
	 * Subsequent executable objects that are mapped in by the
	 * dynamic linker will be tracked by log events of type
	 * PMCLOG_TYPE_MAP_IN.
	 */

	if (image->pi_isdynamic) {

		/*
		 * The runtime loader gets loaded just after the maximum
		 * possible heap address.  Like so:
		 *
		 * [  TEXT DATA BSS HEAP -->*RTLD  SHLIBS   <--STACK]
		 * ^					            ^
		 * 0				   VM_MAXUSER_ADDRESS

		 *
		 * The exact address where the loader gets mapped in
		 * will vary according to the size of the executable
		 * and the limits on the size of the process'es data
		 * segment at the time of exec().  The entry address
		 * recorded at process exec time corresponds to the
		 * 'start' address inside the dynamic linker.  From
		 * this we can figure out the address where the
		 * runtime loader's file object had been mapped to.
		 */
		rtldimage = pmcstat_image_from_path(image->pi_dynlinkerpath,
		    0);
		if (rtldimage == NULL) {
			warnx("WARNING: Cannot find image for \"%s\".",
			    pmcstat_string_unintern(image->pi_dynlinkerpath));
			pmcstat_stats.ps_exec_errors++;
			return;
		}

		if (rtldimage->pi_type == PMCSTAT_IMAGE_UNKNOWN)
			pmcstat_image_get_elf_params(rtldimage, a);

		if (rtldimage->pi_type != PMCSTAT_IMAGE_ELF32 &&
		    rtldimage->pi_type != PMCSTAT_IMAGE_ELF64) {
			warnx("WARNING: rtld not an ELF object \"%s\".",
			    pmcstat_string_unintern(image->pi_dynlinkerpath));
			return;
		}

		libstart = entryaddr - rtldimage->pi_entry;
		pmcstat_image_link(pp, rtldimage, libstart);
	}
}

/*
 * Find the process descriptor corresponding to a PID.  If 'allocate'
 * is zero, we return a NULL if a pid descriptor could not be found or
 * a process descriptor process.  If 'allocate' is non-zero, then we
 * will attempt to allocate a fresh process descriptor.  Zombie
 * process descriptors are only removed if a fresh allocation for the
 * same PID is requested.
 */

static struct pmcstat_process *
pmcstat_process_lookup(pid_t pid, int allocate)
{
	uint32_t hash;
	struct pmcstat_pcmap *ppm, *ppmtmp;
	struct pmcstat_process *pp, *pptmp;

	hash = (uint32_t) pid & PMCSTAT_HASH_MASK;	/* simplicity wins */

	LIST_FOREACH_SAFE(pp, &pmcstat_process_hash[hash], pp_next, pptmp)
	    if (pp->pp_pid == pid) {
		    /* Found a descriptor, check and process zombies */
		    if (allocate && pp->pp_isactive == 0) {
			    /* remove maps */
			    TAILQ_FOREACH_SAFE(ppm, &pp->pp_map, ppm_next,
				ppmtmp) {
				    TAILQ_REMOVE(&pp->pp_map, ppm, ppm_next);
				    free(ppm);
			    }
			    /* remove process entry */
			    LIST_REMOVE(pp, pp_next);
			    free(pp);
			    break;
		    }
		    return (pp);
	    }

	if (!allocate)
		return (NULL);

	if ((pp = malloc(sizeof(*pp))) == NULL)
		err(EX_OSERR, "ERROR: Cannot allocate pid descriptor");

	pp->pp_pid = pid;
	pp->pp_isactive = 1;

	TAILQ_INIT(&pp->pp_map);

	LIST_INSERT_HEAD(&pmcstat_process_hash[hash], pp, pp_next);
	return (pp);
}

/*
 * Associate an image and a process.
 */

static void
pmcstat_process_exec(struct pmcstat_process *pp,
    pmcstat_interned_string path, uintfptr_t entryaddr,
    struct pmcstat_args *a)
{
	struct pmcstat_image *image;

	if ((image = pmcstat_image_from_path(path, 0)) == NULL) {
		pmcstat_stats.ps_exec_errors++;
		return;
	}

	if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
		pmcstat_image_determine_type(image, a);

	assert(image->pi_type != PMCSTAT_IMAGE_UNKNOWN);

	switch (image->pi_type) {
	case PMCSTAT_IMAGE_ELF32:
	case PMCSTAT_IMAGE_ELF64:
		pmcstat_stats.ps_exec_elf++;
		pmcstat_process_elf_exec(pp, image, entryaddr, a);
		break;

	case PMCSTAT_IMAGE_AOUT:
		pmcstat_stats.ps_exec_aout++;
		pmcstat_process_aout_exec(pp, image, entryaddr, a);
		break;

	case PMCSTAT_IMAGE_INDETERMINABLE:
		pmcstat_stats.ps_exec_indeterminable++;
		break;

	default:
		err(EX_SOFTWARE, "ERROR: Unsupported executable type for "
		    "\"%s\"", pmcstat_string_unintern(path));
	}
}


/*
 * Find the map entry associated with process 'p' at PC value 'pc'.
 */

static struct pmcstat_pcmap *
pmcstat_process_find_map(struct pmcstat_process *p, uintfptr_t pc)
{
	struct pmcstat_pcmap *ppm;

	TAILQ_FOREACH(ppm, &p->pp_map, ppm_next) {
		if (pc >= ppm->ppm_lowpc && pc < ppm->ppm_highpc)
			return (ppm);
		if (pc < ppm->ppm_lowpc)
			return (NULL);
	}

	return (NULL);
}

static struct pmcstat_ctnode *
pmcstat_ctnode_allocate(struct pmcstat_image *image, uintfptr_t pc)
{
	struct pmcstat_ctnode *ct;
	
	if ((ct = malloc(sizeof(*ct))) == NULL)
		err(EX_OSERR, "ERROR: Cannot allocate callgraph node");

	ct->pct_image = image;
	ct->pct_func = pc;

	ct->pct_self_count_c = 0;
	ct->pct_self_count = NULL;

	ct->pct_narc = 0;
	ct->pct_arc_c = 0;
	ct->pct_arc = NULL;
	return (ct);
}

static struct pmcstat_ctnode *
pmcstat_ctnode_hash_lookup_image(struct pmcstat_image *image)
{
	struct pmcstat_ctnode *ct;
	struct pmcstat_ctnode_hash *h;
	unsigned int i, hash;
	uintfptr_t pc = image->pi_end;

	for (hash = i = 0; i < sizeof(uintfptr_t); i++)
		hash += (pc >> i) & 0xFF;

	hash &= PMCSTAT_HASH_MASK;

	ct = NULL;
	LIST_FOREACH(h, &pmcstat_ctnode_hash[hash], pch_next)
	{
		ct = h->pch_ctnode;

		assert(ct != NULL);

		if (ct->pct_image == image && ct->pct_func == pc)
			return (ct);
	}

	/*
	 * We haven't seen this (pmcid, pc) tuple yet, so allocate a
	 * new callgraph node and a new hash table entry for it.
	 */
	ct = pmcstat_ctnode_allocate(image, pc);
	if ((h = malloc(sizeof(*h))) == NULL)
		err(EX_OSERR, "ERROR: Could not allocate callgraph node");

	h->pch_ctnode = ct;
	LIST_INSERT_HEAD(&pmcstat_ctnode_hash[hash], h, pch_next);

	return (ct);
}

/*
 * Look for a callgraph node associated with pmc `pmcid' in the global
 * hash table that corresponds to the given `pc' value in the process
 * `pp'.
 */
static struct pmcstat_ctnode *
pmcstat_ctnode_hash_lookup_pc(struct pmcstat_ctnode *parent, struct pmcstat_process *pp, uintfptr_t pc, int usermode, unsigned int pmcid_index)
{
	struct pmcstat_pcmap *ppm;
	struct pmcstat_symbol *sym;
	struct pmcstat_image *image;
	struct pmcstat_ctnode *ct;
	struct pmcstat_ctnode_hash *h;
	uintfptr_t loadaddress;
	unsigned int i, hash;

	ppm = pmcstat_process_find_map(usermode ? pp : pmcstat_kernproc, pc);
	if (ppm == NULL)
		return (NULL);

	image = ppm->ppm_image;

	loadaddress = ppm->ppm_lowpc + image->pi_vaddr - image->pi_start;
	pc -= loadaddress;	/* Convert to an offset in the image. */

	/*
	 * Try determine the function at this offset.  If we can't
	 * find a function round leave the `pc' value alone.
	 */
	if ((sym = pmcstat_symbol_search(image, pc)) != NULL)
		pc = sym->ps_start;

	for (hash = i = 0; i < sizeof(uintfptr_t); i++)
		hash += (pc >> i) & 0xFF;

	hash &= PMCSTAT_HASH_MASK;

	ct = NULL;
	LIST_FOREACH(h, &pmcstat_ctnode_hash[hash], pch_next)
	{
		ct = h->pch_ctnode;

		assert(ct != NULL);

		if (ct->pct_image == image && ct->pct_func == pc)
		{
			if (parent == NULL) {
				parent = pmcstat_ctnode_hash_lookup_image(image);
				if (parent == NULL)
					err(EX_OSERR, "ERROR: Could not allocate default image node");
			}
			for (i = 0; i < parent->pct_narc; i++) {
				if (parent->pct_arc[i].pcta_child == ct) {
				    pmcstat_growit(pmcid_index, &parent->pct_arc[i].pcta_child_count_c, sizeof(uint32_t), (void **)&parent->pct_arc[i].pcta_child_count);
					parent->pct_arc[i].pcta_child_count[pmcid_index]++;
					return (ct); 
				}
			}
			pmcstat_growit(parent->pct_narc, &parent->pct_arc_c, sizeof(struct pmcstat_ctarc), (void **)&parent->pct_arc);
            pmcstat_growit(pmcid_index, &parent->pct_arc[parent->pct_narc].pcta_child_count_c, sizeof(uint32_t), (void **)&parent->pct_arc[parent->pct_narc].pcta_child_count);
			parent->pct_arc[parent->pct_narc].pcta_child_count[pmcid_index] = 1;
			parent->pct_arc[parent->pct_narc++].pcta_child = ct;
			return (ct);
		}
	}

	/*
	 * We haven't seen this (pmcid, pc) tuple yet, so allocate a
	 * new callgraph node and a new hash table entry for it.
	 */
	ct = pmcstat_ctnode_allocate(image, pc);
	if ((h = malloc(sizeof(*h))) == NULL)
		err(EX_OSERR, "ERROR: Could not allocate callgraph node");

	h->pch_ctnode = ct;
	LIST_INSERT_HEAD(&pmcstat_ctnode_hash[hash], h, pch_next);

	if (parent == NULL) {
		parent = pmcstat_ctnode_hash_lookup_image(image);
		if (parent == NULL)
			err(EX_OSERR, "ERROR: Could not allocate default image node");
	}
	pmcstat_growit(parent->pct_narc, &parent->pct_arc_c, sizeof(struct pmcstat_ctarc), (void **)&parent->pct_arc);
	pmcstat_growit(pmcid_index, &parent->pct_arc[parent->pct_narc].pcta_child_count_c, sizeof(uint32_t), (void **)&parent->pct_arc[parent->pct_narc].pcta_child_count);
	parent->pct_arc[parent->pct_narc].pcta_child_count[pmcid_index] = 1;
	parent->pct_arc[parent->pct_narc++].pcta_child = ct;

	return (ct);
}

/*
 * Record a callchain for calltree.
 */

static void
pmcstat_ct_record_callchain(struct pmcstat_process *pp, uint32_t pmcid,
    uint32_t nsamples, uintfptr_t *cc, int usermode, struct pmcstat_args *a)
{
	uintfptr_t pc;
	int n;
	struct pmcstat_ctnode *parent, *child;
	unsigned int pmcid_index;
	
#ifdef DEBUG
	struct pmcstat_symbol *sym;
#endif

	a = a;
	pmcid_index = pmcstat_pmcid_to_index(pmcid); 

	/*
	 * Find the callgraph node recorded in the global hash table for this pc.
	 */
#ifdef DEBUG
printf("pmcstat_ct_record_callchain: pmcid=%lu pmcid_index=%lu %lu samples to process\n", (unsigned long)pmcid, (unsigned long)pmcid_index, (unsigned long)nsamples);
#endif
	n  = (int)nsamples - 1;
	do {
		if (n < 0)
			return;
		
		pc = cc[n--];
		parent = pmcstat_ctnode_hash_lookup_pc(NULL, pp, pc, usermode, pmcid_index);
		if (parent == NULL) {
#ifdef DEBUG
			printf("ERROR: cannot add parent pc=%p\n", (void *)pc);
#endif
		}
	} while (parent == NULL);


#ifdef DEBUG
	sym = pmcstat_symbol_search(parent->pct_image, parent->pct_func);
	if (sym)
		printf("parent=%s (%p)\n", pmcstat_string_unintern(sym->ps_name), (void *)pc);
#endif
	if (n < 0) {
	    pmcstat_growit(pmcid_index, &parent->pct_self_count_c, sizeof(uint32_t), (void **)&parent->pct_self_count);
		parent->pct_self_count[pmcid_index]++;
		pmcstat_stats.ps_callchain_single_frames++;
		return;
	}

	for ( ; n>=0 ; ) {
		pc = cc[n--];
		child = pmcstat_ctnode_hash_lookup_pc(parent, pp, pc, usermode, pmcid_index);
		if (child == NULL) {
#ifdef DEBUG
			printf("ERROR: cannot add child (%p)\n", (void *)pc);
#endif
                        pmcstat_stats.ps_callchain_dubious_frames++;
			continue;
		}
		
#ifdef DEBUG
		sym = pmcstat_symbol_search(child->pct_image, child->pct_func);
		if (sym)
			printf("child=%s (%p)\n", pmcstat_string_unintern(sym->ps_name), (void *)pc);
#endif
		if (n < 0) {
		    pmcstat_growit(pmcid_index, &child->pct_self_count_c, sizeof(uint32_t), (void **)&child->pct_self_count);
			child->pct_self_count[pmcid_index]++;
			break;
		}
			
		parent = child;
	}
}

/*
 * Print one calltree node.  The output format is:
 *
 * ob=object
 * fn=functions
 * address nsamples
 */
static void
pmcstat_ctnode_print(struct pmcstat_args *a, struct pmcstat_ctnode *ct)
{
	struct pmcstat_symbol *sym;
	struct pmcstat_ctnode *child;
	unsigned int i, j;
	
	/* display ob only when changed from previous node
	 */
#if 0
	if (pmcstat_previous_filename_printed !=
	    ct->pct_image->pi_fullpath) {
#endif
		pmcstat_previous_filename_printed = ct->pct_image->pi_fullpath;
		fprintf(a->pa_graphfile, "ob=%s\n",
		    pmcstat_string_unintern(pmcstat_previous_filename_printed));
#if 0
	}
#endif

	if ( ct->pct_image->pi_end == ct->pct_func )
		fprintf(a->pa_graphfile, "fn=%s\n", pmcstat_string_unintern(ct->pct_image->pi_name));
	else
	{
	sym = pmcstat_symbol_search(ct->pct_image, ct->pct_func);
	if (sym)
		fprintf(a->pa_graphfile, "fn=%s\n",
		    pmcstat_string_unintern(sym->ps_name));
	else
		fprintf(a->pa_graphfile, "fn=%p\n",
		    (void *) (ct->pct_image->pi_vaddr + ct->pct_func));
	}
#ifdef DEBUG
printf("parent: %s cost=", pmcstat_string_unintern(sym->ps_name));
#endif
	fprintf(a->pa_graphfile, "*");
	pmcstat_growit(pmcstat_npmcs-1, &ct->pct_self_count_c, sizeof(uint32_t), (void **)&ct->pct_self_count);
	for (i = 0; i<pmcstat_npmcs ; i++) {
#ifdef DEBUG
printf(" %u", ct->pct_self_count[i]);
#endif
		fprintf(a->pa_graphfile, " %u", ct->pct_self_count[i]);
	}
#ifdef DEBUG
printf("\n");
#endif
	fprintf(a->pa_graphfile, "\n");

	for (i=0 ; i<ct->pct_narc; i++) {
		child = ct->pct_arc[i].pcta_child;

		/* display cob only when changed from previous node
		 */
#if 0
		if (pmcstat_previous_filename_printed != child->pct_image->pi_fullpath) {
#endif
			pmcstat_previous_filename_printed = child->pct_image->pi_fullpath;
			fprintf(a->pa_graphfile, "cob=%s\n", pmcstat_string_unintern(pmcstat_previous_filename_printed));
#if 0
		}
#endif

		sym = pmcstat_symbol_search(child->pct_image, child->pct_func);
		if (sym)
			fprintf(a->pa_graphfile, "cfn=%s\n", pmcstat_string_unintern(sym->ps_name));
		else
			fprintf(a->pa_graphfile, "cfn=%p\n", (void *)(child->pct_image->pi_vaddr + child->pct_func));

#ifdef DEBUG
printf("    %s cost=", pmcstat_string_unintern(sym->ps_name));
#endif
		fprintf(a->pa_graphfile, "calls=1 *\n");
		fprintf(a->pa_graphfile, "*");
		pmcstat_growit(pmcstat_npmcs-1, &ct->pct_arc[i].pcta_child_count_c, sizeof(uint32_t), (void **)&ct->pct_arc[i].pcta_child_count);
		for (j = 0; j<pmcstat_npmcs ; j++) {
#ifdef DEBUG
printf(" %u", ct->pct_arc[i].pcta_child_count[j]);
#endif
			fprintf(a->pa_graphfile, " %u", ct->pct_arc[i].pcta_child_count[j]);
		}
		fprintf(a->pa_graphfile, "\n");
#ifdef DEBUG
printf("\n");
#endif
	}
}

/*
 * Printing a calltree (KCachegrind) for a PMC.
 */
static void
pmcstat_calltree_print(struct pmcstat_args *a)
{
	unsigned int n, i;
	uint32_t nsamples_c;
	uint32_t *nsamples;
	struct pmcstat_ctnode_hash *pch;

	nsamples_c = 0;
	nsamples = NULL;
	pmcstat_growit(pmcstat_npmcs, &nsamples_c, sizeof(uint32_t), (void **)&nsamples);
	
	for (n = 0; n < PMCSTAT_NHASH; n++)
		LIST_FOREACH(pch, &pmcstat_ctnode_hash[n], pch_next) {
                        pmcstat_growit(pmcstat_npmcs-1, &pch->pch_ctnode->pct_self_count_c, sizeof(uint32_t), (void **)&pch->pch_ctnode->pct_self_count);
                        for (i=0; i<pmcstat_npmcs ; i++)
                                nsamples[i] += pch->pch_ctnode->pct_self_count[i];
                }

	fprintf(a->pa_graphfile, 
		"version: 1\n"\
		"creator: pmcstat\n"\
		"positions: instr\n"\
		"events:");
	for (i=0; i<pmcstat_npmcs ; i++)
		fprintf(a->pa_graphfile, " %s", pmcstat_pmcid_index_to_name(i));
	fprintf(a->pa_graphfile, "\nsummary:");
	for (i=0; i<pmcstat_npmcs ; i++)
		fprintf(a->pa_graphfile, " %u", nsamples[i]);
	fprintf(a->pa_graphfile, "\n\n");

	pmcstat_previous_filename_printed = NULL;
	for (n = 0; n < PMCSTAT_NHASH; n++) {
		LIST_FOREACH(pch, &pmcstat_ctnode_hash[n], pch_next) {
			pmcstat_ctnode_print(a, pch->pch_ctnode);
		}
	}
	
	free(nsamples);
}

/*
 * Convert a hwpmc(4) log to profile information.  A system-wide
 * callgraph is generated if FLAG_DO_CALLGRAPHS is set.  gmon.out
 * files usable by gprof(1) are created if FLAG_DO_GPROF is set.
 */
int
pmcstat_ct_process_log(struct pmcstat_args *a)
{
	uint32_t cpu, cpuflags;
	pid_t pid;
	struct pmcstat_image *image;
	struct pmcstat_process *pp, *ppnew;
	struct pmcstat_pcmap *ppm, *ppmtmp;
	struct pmclog_ev ev;
	pmcstat_interned_string image_path;

	assert(a->pa_flags & FLAG_DO_ANALYSIS);

	if (elf_version(EV_CURRENT) == EV_NONE)
		err(EX_UNAVAILABLE, "Elf library intialization failed");

	while (pmclog_read(a->pa_logparser, &ev) == 0) {
		assert(ev.pl_state == PMCLOG_OK);

		switch (ev.pl_type) {
		case PMCLOG_TYPE_INITIALIZE:
			if ((ev.pl_u.pl_i.pl_version & 0xFF000000) !=
			    PMC_VERSION_MAJOR << 24 && a->pa_verbosity > 0)
				warnx("WARNING: Log version 0x%x does not "
				    "match compiled version 0x%x.",
				    ev.pl_u.pl_i.pl_version,
				    PMC_VERSION_MAJOR);
			break;

		case PMCLOG_TYPE_MAP_IN:
			/*
			 * Introduce an address range mapping for a
			 * userland process or the kernel (pid == -1).
			 *
			 * We always allocate a process descriptor so
			 * that subsequent samples seen for this
			 * address range are mapped to the current
			 * object being mapped in.
			 */
			pid = ev.pl_u.pl_mi.pl_pid;
			if (pid == -1)
				pp = pmcstat_kernproc;
			else
				pp = pmcstat_process_lookup(pid,
				    PMCSTAT_ALLOCATE);

			assert(pp != NULL);

			image_path = pmcstat_string_intern(ev.pl_u.pl_mi.
			    pl_pathname);
			image = pmcstat_image_from_path(image_path, pid == -1);
			if (image->pi_type == PMCSTAT_IMAGE_UNKNOWN)
				pmcstat_image_determine_type(image, a);
			if (image->pi_type != PMCSTAT_IMAGE_INDETERMINABLE)
				pmcstat_image_link(pp, image,
				    ev.pl_u.pl_mi.pl_start);
			break;

		case PMCLOG_TYPE_MAP_OUT:
			/*
			 * Remove an address map.
			 */
			pid = ev.pl_u.pl_mo.pl_pid;
			if (pid == -1)
				pp = pmcstat_kernproc;
			else
				pp = pmcstat_process_lookup(pid, 0);

			if (pp == NULL)	/* unknown process */
				break;

			pmcstat_image_unmap(pp, ev.pl_u.pl_mo.pl_start,
			    ev.pl_u.pl_mo.pl_end);
			break;

		case PMCLOG_TYPE_CALLCHAIN:
			pmcstat_stats.ps_samples_total++;

			cpuflags = ev.pl_u.pl_cc.pl_cpuflags;
			cpu = PMC_CALLCHAIN_CPUFLAGS_TO_CPU(cpuflags);

			/* Filter on the CPU id. */
			if ((a->pa_cpumask & (1 << cpu)) == 0) {
				pmcstat_stats.ps_samples_skipped++;
				break;
			}

			pp = pmcstat_process_lookup(ev.pl_u.pl_cc.pl_pid,
			    PMCSTAT_ALLOCATE);

			pmcstat_ct_record_callchain(pp,
				    ev.pl_u.pl_cc.pl_pmcid, ev.pl_u.pl_cc.pl_npc,
                    ev.pl_u.pl_cc.pl_pc,
				    PMC_CALLCHAIN_CPUFLAGS_TO_USERMODE(cpuflags), a);

			break;

		case PMCLOG_TYPE_PMCALLOCATE:
			/*
			 * Record the association pmc id between this
			 * PMC and its name.
			 */
			pmcstat_pmcid_add(ev.pl_u.pl_a.pl_pmcid,
			    pmcstat_string_intern(ev.pl_u.pl_a.pl_evname));
			break;

		case PMCLOG_TYPE_PROCEXEC:

			/*
			 * Change the executable image associated with
			 * a process.
			 */
			pp = pmcstat_process_lookup(ev.pl_u.pl_x.pl_pid,
			    PMCSTAT_ALLOCATE);

			/* delete the current process map */
			TAILQ_FOREACH_SAFE(ppm, &pp->pp_map, ppm_next, ppmtmp) {
				TAILQ_REMOVE(&pp->pp_map, ppm, ppm_next);
				free(ppm);
			}

			/* associate this process  image */
			image_path = pmcstat_string_intern(
				ev.pl_u.pl_x.pl_pathname);
			assert(image_path != NULL);
			pmcstat_process_exec(pp, image_path,
			    ev.pl_u.pl_x.pl_entryaddr, a);
			break;

		case PMCLOG_TYPE_PROCEXIT:

			/*
			 * Due to the way the log is generated, the
			 * last few samples corresponding to a process
			 * may appear in the log after the process
			 * exit event is recorded.  Thus we keep the
			 * process' descriptor and associated data
			 * structures around, but mark the process as
			 * having exited.
			 */
			pp = pmcstat_process_lookup(ev.pl_u.pl_e.pl_pid, 0);
			if (pp == NULL)
				break;
			pp->pp_isactive = 0;	/* mark as a zombie */
			break;

		case PMCLOG_TYPE_SYSEXIT:
			pp = pmcstat_process_lookup(ev.pl_u.pl_se.pl_pid, 0);
			if (pp == NULL)
				break;
			pp->pp_isactive = 0;	/* make a zombie */
			break;

		case PMCLOG_TYPE_PROCFORK:

			/*
			 * Allocate a process descriptor for the new
			 * (child) process.
			 */
			ppnew =
			    pmcstat_process_lookup(ev.pl_u.pl_f.pl_newpid,
				PMCSTAT_ALLOCATE);

			/*
			 * If we had been tracking the parent, clone
			 * its address maps.
			 */
			pp = pmcstat_process_lookup(ev.pl_u.pl_f.pl_oldpid, 0);
			if (pp == NULL)
				break;
			TAILQ_FOREACH(ppm, &pp->pp_map, ppm_next)
			    pmcstat_image_link(ppnew, ppm->ppm_image,
				ppm->ppm_lowpc);
			break;

		default:	/* other types of entries are not relevant */
			break;
		}
	}

	if (ev.pl_state == PMCLOG_EOF)
		return (PMCSTAT_FINISHED);
	else if (ev.pl_state == PMCLOG_REQUIRE_DATA)
		return (PMCSTAT_RUNNING);

	err(EX_DATAERR, "ERROR: event parsing failed (record %jd, "
	    "offset 0x%jx)", (uintmax_t) ev.pl_count + 1, ev.pl_offset);
}

/*
 * Public Interfaces.
 */

/*
 * Close a logfile, after first flushing all in-module queued data.
 */

int
pmcstat_ct_close_log(struct pmcstat_args *a)
{
	if (pmc_flush_logfile() < 0 ||
	    pmc_configure_logfile(-1) < 0)
		err(EX_OSERR, "ERROR: logging failed");
	a->pa_flags &= ~(FLAG_HAS_OUTPUT_LOGFILE | FLAG_HAS_PIPE);
	return (a->pa_flags & FLAG_HAS_PIPE ? PMCSTAT_EXITING :
	    PMCSTAT_FINISHED);
}



/*
 * Open a log file, for reading or writing.
 *
 * The function returns the fd of a successfully opened log or -1 in
 * case of failure.
 */

int
pmcstat_ct_open_log(const char *path, int mode)
{
	int error, fd;
	size_t hlen;
	const char *p, *errstr;
	struct addrinfo hints, *res, *res0;
	char hostname[MAXHOSTNAMELEN];

	errstr = NULL;
	fd = -1;

	/*
	 * If 'path' is "-" then open one of stdin or stdout depending
	 * on the value of 'mode'.
	 *
	 * If 'path' contains a ':' and does not start with a '/' or '.',
	 * and is being opened for writing, treat it as a "host:port"
	 * specification and open a network socket.
	 *
	 * Otherwise, treat 'path' as a file name and open that.
	 */
	if (path[0] == '-' && path[1] == '\0')
		fd = (mode == PMCSTAT_OPEN_FOR_READ) ? 0 : 1;
	else if (mode == PMCSTAT_OPEN_FOR_WRITE && path[0] != '/' &&
	    path[0] != '.' && strchr(path, ':') != NULL) {

		p = strrchr(path, ':');
		hlen = p - path;
		if (p == path || hlen >= sizeof(hostname)) {
			errstr = strerror(EINVAL);
			goto done;
		}

		assert(hlen < sizeof(hostname));
		(void) strncpy(hostname, path, hlen);
		hostname[hlen] = '\0';

		(void) memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		if ((error = getaddrinfo(hostname, p+1, &hints, &res0)) != 0) {
			errstr = gai_strerror(error);
			goto done;
		}

		fd = -1;
		for (res = res0; res; res = res->ai_next) {
			if ((fd = socket(res->ai_family, res->ai_socktype,
			    res->ai_protocol)) < 0) {
				errstr = strerror(errno);
				continue;
			}
			if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
				errstr = strerror(errno);
				(void) close(fd);
				fd = -1;
				continue;
			}
			errstr = NULL;
			break;
		}
		freeaddrinfo(res0);

	} else if ((fd = open(path, mode == PMCSTAT_OPEN_FOR_READ ?
		    O_RDONLY : (O_WRONLY|O_CREAT|O_TRUNC),
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
			errstr = strerror(errno);

  done:
	if (errstr)
		errx(EX_OSERR, "ERROR: Cannot open \"%s\" for %s: %s.", path,
		    (mode == PMCSTAT_OPEN_FOR_READ ? "reading" : "writing"),
		    errstr);

	return (fd);
}

/*
 * Initialize module.
 */

void
pmcstat_ct_initialize_logging(struct pmcstat_args *a)
{
	int i;

	(void) a;

	/* use a convenient format for 'ldd' output */
	if (setenv("LD_TRACE_LOADED_OBJECTS_FMT1","%o \"%p\" %x\n",1) != 0)
		err(EX_OSERR, "ERROR: Cannot setenv");

	/* Initialize hash tables */
	pmcstat_string_initialize();
	for (i = 0; i < PMCSTAT_NHASH; i++) {
		LIST_INIT(&pmcstat_image_hash[i]);
		LIST_INIT(&pmcstat_process_hash[i]);
	}

	/*
	 * Create a fake 'process' entry for the kernel with pid -1.
	 * hwpmc(4) will subsequently inform us about where the kernel
	 * and any loaded kernel modules are mapped.
	 */
	if ((pmcstat_kernproc = pmcstat_process_lookup((pid_t) -1,
		 PMCSTAT_ALLOCATE)) == NULL)
		err(EX_OSERR, "ERROR: Cannot initialize logging");
}

/*
 * Shutdown module.
 */

void
pmcstat_ct_shutdown_logging(struct pmcstat_args *a)
{
	int i;
	struct pmcstat_image *pi, *pitmp;
	struct pmcstat_process *pp, *pptmp;
#if 0
	struct pmcstat_ctnode_hash *pch, *pchtmp;
#endif

	pmcstat_calltree_print(a);
	
#if 0
	/*
	 * Free memory.
	 */
	for (i = 0; i < PMCSTAT_NHASH; i++) {
		LIST_FOREACH_SAFE(pch, &pmcstat_ctnode_hash[i], pch_next,
		    pchtmp) {
			pmcstat_ctnode_free(pch->pch_ctnode);
			free(pch);
		}
	}
#endif

	for (i = 0; i < PMCSTAT_NHASH; i++) {
		LIST_FOREACH_SAFE(pi, &pmcstat_image_hash[i], pi_next, pitmp)
		{
			if (pi->pi_symbols)
				free(pi->pi_symbols);

			LIST_REMOVE(pi, pi_next);
			free(pi);
		}

		LIST_FOREACH_SAFE(pp, &pmcstat_process_hash[i], pp_next,
		    pptmp) {
			LIST_REMOVE(pp, pp_next);
			free(pp);
		}
	}

	pmcstat_string_shutdown();

	/*
	 * Print errors unless -q was specified.  Print all statistics
	 * if verbosity > 1.
	 */
#define	PRINT(N,V,A) do {						\
		if (pmcstat_stats.ps_##V || (A)->pa_verbosity >= 2)	\
			(void) fprintf((A)->pa_printfile, " %-40s %d\n",\
			    N, pmcstat_stats.ps_##V);			\
	} while (0)

	if (a->pa_verbosity >= 1) {
		(void) fprintf(a->pa_printfile, "CONVERSION STATISTICS:\n");
		PRINT("#exec/a.out", exec_aout, a);
		PRINT("#exec/elf", exec_elf, a);
		PRINT("#exec/unknown", exec_indeterminable, a);
		PRINT("#exec handling errors", exec_errors, a);
		PRINT("#samples/total", samples_total, a);
		PRINT("#samples/unclaimed", samples_unknown_offset, a);
		PRINT("#samples/unknown-object", samples_indeterminable, a);
		PRINT("#callchain/dubious-frames", callchain_dubious_frames, a);
		PRINT("#callchain/single-frames", callchain_single_frames, a);
	}

}

