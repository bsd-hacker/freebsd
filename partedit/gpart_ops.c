#include <sys/param.h>
#include <errno.h>
#include <libutil.h>
#include <inttypes.h>

#include <libgeom.h>
#include <cdialog/dialog.h>
#include <cdialog/dlg_keys.h>

#include "partedit.h"

#define GPART_FLAGS "x" /* Do not commit changes by default */

static void set_part_metadata(const char *name, const char *scheme,
    const char *type, const char *mountpoint, int newfs);

static void
gpart_show_error(const char *title, const char *explanation, const char *errstr)
{
	char *errmsg;
	char message[512];
	int error;

	if (explanation == NULL)
		explanation = "";

	error = strtol(errstr, &errmsg, 0);
	if (errmsg != errstr) {
		while (errmsg[0] == ' ')
			errmsg++;
		if (errmsg[0] != '\0')
			sprintf(message, "%s%s. %s", explanation,
			    strerror(error), errmsg);
		else
			sprintf(message, "%s%s", explanation, strerror(error));
	} else {
		sprintf(message, "%s%s", explanation, errmsg);
	}

	dialog_msgbox(title, message, 0, 0, TRUE);
}

static int
gpart_partition(const char *lg_name, const char *scheme)
{
	int cancel, choice;
	struct gctl_req *r;
	const char *errstr;

	DIALOG_LISTITEM items[] = {
		{"APM", "Apple Partition Map",
		    "Bootable on PowerPC Apple Hardware", 0 },
		{"BSD", "BSD Labels",
		    "Bootable on most x86 systems", 0 },
		{"GPT", "GUID Partition Table",
		    "Bootable on most x86 systems", 0 },
		{"MBR", "DOS Partitions",
		    "Bootable on most x86 systems", 0 },
		{"PC98", "NEC PC9801 Partition Table",
		    "Bootable on NEC PC9801 systems", 0 },
		{"VTOC8", "Sun VTOC8 Partition Table",
		    "Bootable on Sun SPARC systems", 0 },
	};

schememenu:
	if (scheme == NULL) {
		cancel = dlg_menu("Partition Scheme",
		    "Select a partition scheme for this volume:", 0, 0, 0,
		    sizeof(items) / sizeof(items[0]), items, &choice, NULL);

		if (cancel)
			return (-1);

		if (!is_scheme_bootable(items[choice].name)) {
			char message[512];
			sprintf(message, "This partition scheme (%s) is not "
			    "bootable on this platform. Are you sure you want "
			    "to proceed?", items[choice].name);
			dialog_vars.defaultno = TRUE;
			cancel = dialog_yesno("Warning", message, 0, 0);
			dialog_vars.defaultno = FALSE;
			if (cancel) /* cancel */
				goto schememenu;
		}

		scheme = items[choice].name;
	}

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "scheme", -1, scheme);
	gctl_ro_param(r, "verb", -1, "create");

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
		scheme = NULL;
		goto schememenu;
	}
	gctl_free(r);

	if (bootcode_path(scheme) != NULL)
		get_part_metadata(lg_name, 1)->bootcode = 1;
	return (0);
}

static void
gpart_activate(struct gprovider *pp)
{
	struct gconfig *gc;
	struct gctl_req *r;
	const char *errstr, *scheme;
	const char *attribute = NULL;
	intmax_t index;

	/*
	 * Some partition schemes need this partition to be marked 'active'
	 * for it to be bootable.
	 */
	LIST_FOREACH(gc, &pp->lg_geom->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "scheme") == 0) {
			scheme = gc->lg_val;
			break;
		}
	}

	if (strcmp(scheme, "MBR") == 0 || strcmp(scheme, "EBR") == 0 ||
	    strcmp(scheme, "PC98") == 0)
		attribute = "active";
	else
		return;

	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "index") == 0) {
			index = atoi(gc->lg_val);
			break;
		}
	}

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, pp->lg_geom->lg_name);
	gctl_ro_param(r, "verb", -1, "set");
	gctl_ro_param(r, "attrib", -1, attribute);
	gctl_ro_param(r, "index", sizeof(index), &index);

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') 
		gpart_show_error("Error", "Error marking partition active:",
		    errstr);
	gctl_free(r);
}

static void
gpart_bootcode(struct ggeom *gp)
{
	const char *bootcode;
	struct gconfig *gc;
	struct gctl_req *r;
	const char *errstr, *scheme;
	uint8_t *boot;
	size_t bootsize, bytes;
	int bootfd;

	/*
	 * Write default bootcode to the newly partitioned disk, if that
	 * applies on this platform.
	 */
	LIST_FOREACH(gc, &gp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "scheme") == 0) {
			scheme = gc->lg_val;
			break;
		}
	}

	bootcode = bootcode_path(scheme);
	if (bootcode == NULL) 
		return;

	bootfd = open(bootcode, O_RDONLY);
	if (bootfd <= 0) {
		dialog_msgbox("Bootcode Error", strerror(errno), 0, 0,
		    TRUE);
		return;
	}
		
	bootsize = lseek(bootfd, 0, SEEK_END);
	boot = malloc(bootsize);
	lseek(bootfd, 0, SEEK_SET);
	bytes = 0;
	while (bytes < bootsize)
		bytes += read(bootfd, boot + bytes, bootsize - bytes);
	close(bootfd);

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, gp->lg_name);
	gctl_ro_param(r, "verb", -1, "bootcode");
	gctl_ro_param(r, "bootcode", bootsize, boot);

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') 
		gpart_show_error("Bootcode Error", NULL, errstr);
	gctl_free(r);
	free(boot);
}

static void
gpart_partcode(struct gprovider *pp)
{
	struct gconfig *gc;
	const char *scheme;
	const char *indexstr;
	char message[255], command[255];

	LIST_FOREACH(gc, &pp->lg_geom->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "scheme") == 0) {
			scheme = gc->lg_val;
			break;
		}
	}

	/* Make sure this partition scheme needs partcode on this platform */
	if (partcode_path(scheme) == NULL)
		return;

	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "index") == 0) {
			indexstr = gc->lg_val;
			break;
		}
	}

	/* Shell out to gpart for partcode for now */
	sprintf(command, "gpart bootcode -p %s -i %s %s",
	    partcode_path(scheme), indexstr, pp->lg_geom->lg_name);
	if (system(command) != 0) {
		sprintf(message, "Error installing partcode on partition %s",
		    pp->lg_name);
		dialog_msgbox("Error", message, 0, 0, TRUE);
	}
}

void
gpart_edit(struct gprovider *pp)
{
	struct gctl_req *r;
	struct gconfig *gc;
	struct gconsumer *cp;
	struct gprovider *spp;
	struct ggeom *geom;
	const char *errstr, *oldtype, *scheme;
	struct partition_metadata *md;
	char sizestr[32];
	intmax_t index;
	int hadlabel, choice, junk, nitems;
	unsigned i;

	DIALOG_FORMITEM items[] = {
		{0, "Type:", 5, 0, 0, FALSE, "", 11, 0, 12, 15, 0,
		    FALSE, "Filesystem type (e.g. freebsd-ufs, freebsd-swap)",
		    FALSE},
		{0, "Size:", 5, 1, 0, FALSE, "", 11, 1, 12, 0, 0,
		    FALSE, "Partition size. Append K, M, G for kilobytes, "
		    "megabytes or gigabytes.", FALSE},
		{0, "Mountpoint:", 11, 2, 0, FALSE, "", 11, 2, 12, 15, 0,
		    FALSE, "Path at which to mount this partition (leave blank "
		    "for swap)", FALSE},
		{0, "Label:", 7, 3, 0, FALSE, "", 11, 3, 12, 15, 0, FALSE,
		    "Partition name. Not all partition schemes support this.",
		    FALSE},
	};

	/*
	 * Find the PART geom we are manipulating. This may be a consumer of
	 * this provider, or its parent. Check the consumer case first.
	 */
	geom = NULL;
	LIST_FOREACH(cp, &pp->lg_consumers, lg_consumers)
		if (strcmp(cp->lg_geom->lg_class->lg_name, "PART") == 0) {
			char message[512];
			/*
			 * The PART object is a consumer, so the user wants to
			 * edit the partition table. gpart doesn't really
			 * support this, so we have to hose the whole table
			 * first.
			 */

			sprintf(message, "Changing the partition scheme on "
			    "this disk (%s) requires deleting all existing "
			    "partitions on this drive. This will PERMANENTLY "
			    "ERASE any data stored here. Are you sure you want "
			    "to proceed?", cp->lg_geom->lg_name);
			dialog_vars.defaultno = TRUE;
			choice = dialog_yesno("Warning", message, 0, 0);
			dialog_vars.defaultno = FALSE;

			if (choice == 1) /* cancel */
				return;

			/* Begin with the hosing: delete all partitions */
			LIST_FOREACH(spp, &cp->lg_geom->lg_provider,
			    lg_provider)
				gpart_delete(spp);

			/* Now destroy the geom itself */
			r = gctl_get_handle();
			gctl_ro_param(r, "class", -1, "PART");
			gctl_ro_param(r, "arg0", -1, cp->lg_geom->lg_name);
			gctl_ro_param(r, "flags", -1, GPART_FLAGS);
			gctl_ro_param(r, "verb", -1, "destroy");
			errstr = gctl_issue(r);
			if (errstr != NULL && errstr[0] != '\0') 
				gpart_show_error("Error", NULL, errstr);
			gctl_free(r);

			/* And any metadata */
			delete_part_metadata(cp->lg_geom->lg_name);

			/* Now re-partition and return */
			gpart_partition(cp->lg_geom->lg_name, NULL);
			return;
		}

	if (geom == NULL && strcmp(pp->lg_geom->lg_class->lg_name, "PART") == 0)
		geom = pp->lg_geom;

	if (geom == NULL) {
		/* Disk not partitioned, so partition it */
		gpart_partition(pp->lg_geom->lg_name, NULL);
		return;
	}

	LIST_FOREACH(gc, &geom->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "scheme") == 0) {
			scheme = gc->lg_val;
			break;
		}
	}

	/* Labels only supported on GPT and APM */
	if (strcmp(scheme, "GPT") == 0 || strcmp(scheme, "APM") == 0)
		nitems = 4;
	else
		nitems = 3;

	/* Edit editable parameters of a partition */
	hadlabel = 0;
	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "type") == 0) {
			oldtype = gc->lg_val;
			items[0].text = gc->lg_val;
		}
		if (strcmp(gc->lg_name, "label") == 0) {
			hadlabel = 1;
			items[3].text = gc->lg_val;
		}
		if (strcmp(gc->lg_name, "index") == 0)
			index = atoi(gc->lg_val);
	}

	TAILQ_FOREACH(md, &part_metadata, metadata) {
		if (md->name != NULL && strcmp(md->name, pp->lg_name) == 0) {
			if (md->fstab != NULL)
				items[2].text = md->fstab->fs_file;
			break;
		}
	}

	humanize_number(sizestr, 7, pp->lg_mediasize, "B", HN_AUTOSCALE,
	    HN_NOSPACE | HN_DECIMAL);
	items[1].text = sizestr;

editpart:
	choice = dlg_form("Edit Partition", "", 0, 0, 0, nitems, items, &junk);

	if (choice) /* Cancel pressed */
		return;

	if (strncmp(items[0].text, "freebsd-", 8) != 0 &&
	    items[0].text[0] != '\0') {
		char message[512];

		sprintf(message, "Cannot mount unknown file system %s!\n",
		    items[0].text);
		dialog_msgbox("Error", message, 0, 0, TRUE);
		goto editpart;
	}

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, geom->lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "verb", -1, "modify");
	gctl_ro_param(r, "index", sizeof(index), &index);
	if (hadlabel || items[3].text[0] != '\0')
		gctl_ro_param(r, "label", -1, items[3].text);
	gctl_ro_param(r, "type", -1, items[0].text);
	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
		goto editpart;
	}
	gctl_free(r);

	set_part_metadata(pp->lg_name, scheme, items[0].text, items[2].text,
	    strcmp(oldtype, items[0].text) != 0);

	for (i = 0; i < (sizeof(items) / sizeof(items[0])); i++)
		if (items[i].text_free)
			free(items[i].text);
}

static void
set_part_metadata(const char *name, const char *scheme, const char *type,
    const char *mountpoint, int newfs)
{
	struct partition_metadata *md;

	/* Set part metadata */
	md = get_part_metadata(name, 1);

	if (newfs) {
		if (md->newfs != NULL) {
			free(md->newfs);
			md->newfs = NULL;
		}

		if (strcmp(type, "freebsd-ufs") == 0) {
			md->newfs = malloc(255);
			sprintf(md->newfs, "newfs /dev/%s", name);
		}
	}

	if (strcmp(type, "freebsd-swap") == 0)
		mountpoint = "none";
	if (strcmp(type, "freebsd-boot") == 0)
		md->bootcode = 1;

	/* VTOC8 needs partcode in UFS partitions */
	if (strcmp(scheme, "VTOC8") == 0 && strcmp(type, "freebsd-ufs") == 0)
		md->bootcode = 1;

	if (mountpoint == NULL || mountpoint[0] == '\0') {
		if (md->fstab != NULL) {
			free(md->fstab->fs_spec);
			free(md->fstab->fs_file);
			free(md->fstab->fs_vfstype);
			free(md->fstab->fs_mntops);
			free(md->fstab->fs_type);
			free(md->fstab);
			md->fstab = NULL;
		}
	} else {
		if (md->fstab == NULL) {
			md->fstab = malloc(sizeof(struct fstab));
		} else {
			free(md->fstab->fs_spec);
			free(md->fstab->fs_file);
			free(md->fstab->fs_vfstype);
			free(md->fstab->fs_mntops);
			free(md->fstab->fs_type);
		}
		md->fstab->fs_spec = malloc(strlen(name) + 6);
		sprintf(md->fstab->fs_spec, "/dev/%s", name);
		md->fstab->fs_file = strdup(mountpoint);
		/* Get VFS from text after freebsd-, if possible */
		if (strncmp("freebsd-", type, 8))
			md->fstab->fs_vfstype = strdup(&type[8]);
		else
			md->fstab->fs_vfstype = strdup(type); /* Guess */
		md->fstab->fs_vfstype = strdup(&type[8]);
		if (strcmp(type, "freebsd-swap") == 0) {
			md->fstab->fs_type = strdup(FSTAB_SW);
			md->fstab->fs_freq = 0;
			md->fstab->fs_passno = 0;
		} else {
			md->fstab->fs_type = strdup(FSTAB_RW);
			if (strcmp(mountpoint, "/") == 0) {
				md->fstab->fs_freq = 1;
				md->fstab->fs_passno = 1;
			} else {
				md->fstab->fs_freq = 2;
				md->fstab->fs_passno = 2;
			}
		}
		md->fstab->fs_mntops = strdup(md->fstab->fs_type);
	}
}

void
gpart_create(struct gprovider *pp)
{
	struct gctl_req *r;
	struct gconfig *gc;
	struct gconsumer *cp;
	struct ggeom *geom;
	const char *errstr, *scheme;
	char sizestr[32], startstr[32], output[64];
	intmax_t maxsize, size, start, end, sector, firstfree, stripe;
	uint64_t bytes;
	int nitems, choice, junk;
	unsigned i;

	DIALOG_FORMITEM items[] = {
		{0, "Type:", 5, 0, 0, FALSE, "freebsd-ufs", 11, 0, 12, 15, 0,
		    FALSE, "Filesystem type (e.g. freebsd-ufs, freebsd-swap)",
		    FALSE},
		{0, "Size:", 5, 1, 0, FALSE, "", 11, 1, 12, 15, 0,
		    FALSE, "Partition size. Append K, M, G for kilobytes, "
		    "megabytes or gigabytes.", FALSE},
		{0, "Mountpoint:", 11, 2, 0, FALSE, "", 11, 2, 12, 15, 0,
		    FALSE, "Path at which to mount this partition (leave blank "
		    "for swap)", FALSE},
		{0, "Label:", 7, 3, 0, FALSE, "", 11, 3, 12, 15, 0, FALSE,
		    "Partition name. Not all partition schemes support this.",
		    FALSE},
	};

	/* Record sector and stripe sizes */
	sector = pp->lg_sectorsize;
	stripe = pp->lg_stripesize;

	/*
	 * Find the PART geom we are manipulating. This may be a consumer of
	 * this provider, or its parent. Check the consumer case first.
	 */
	geom = NULL;
	LIST_FOREACH(cp, &pp->lg_consumers, lg_consumers)
		if (strcmp(cp->lg_geom->lg_class->lg_name, "PART") == 0) {
			geom = cp->lg_geom;
			break;
		}

	if (geom == NULL && strcmp(pp->lg_geom->lg_class->lg_name, "PART") == 0)
		geom = pp->lg_geom;

	if (geom == NULL) {
		if (gpart_partition(pp->lg_geom->lg_name, NULL) == 0)
			dialog_msgbox("",
			    "The partition table has been successfully created."
			    " Please press Create again to create partitions.",
			    0, 0, TRUE);

		return;
	}

	/*
	 * If we still don't have a geom, either the user has
	 * canceled partitioning or there has been an error which has already
	 * been displayed, so bail.
	 */
	if (geom == NULL)
		return;

	/* Now get the maximum free size and free start */
	start = end = 0;
	LIST_FOREACH(gc, &geom->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "first") == 0)
			start = strtoimax(gc->lg_val, NULL, 0);
		if (strcmp(gc->lg_name, "last") == 0)
			end = strtoimax(gc->lg_val, NULL, 0);
		if (strcmp(gc->lg_name, "scheme") == 0)
			scheme = gc->lg_val;
	}

	firstfree = start;
	LIST_FOREACH(pp, &geom->lg_provider, lg_provider) {
		LIST_FOREACH(gc, &pp->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "end") == 0) {
				intmax_t partend;
				partend = strtoimax(gc->lg_val, NULL, 0);
				if (partend > firstfree)
					firstfree = partend;
			}
		}
	}

	/* Compute beginning of new partition and maximum available space */
	firstfree++;
	if (stripe > 0 && (firstfree*sector % stripe) != 0) 
		firstfree += (stripe - ((firstfree*sector) % stripe)) / sector;

	size = maxsize = end - firstfree;
	if (size <= 0) {
		dialog_msgbox("Error", "No free space left on device.", 0, 0,
		    TRUE);
		return;
	}

	/* Leave a free megabyte in case we need to write a boot partition */
	if (size*sector >= 1024*1024)
		size -= 1024*1024/sector;

	humanize_number(sizestr, 7, size*sector, "B", HN_AUTOSCALE,
	    HN_NOSPACE | HN_DECIMAL);
	items[1].text = sizestr;

	/* Special-case the MBR default type for nested partitions */
	if (strcmp(scheme, "MBR") == 0 || strcmp(scheme, "PC98") == 0)
		items[0].text = "freebsd";

	/* Labels only supported on GPT and APM */
	if (strcmp(scheme, "GPT") == 0 || strcmp(scheme, "APM") == 0)
		nitems = 4;
	else
		nitems = 3;

addpartform:
	choice = dlg_form("Add Partition", "", 0, 0, 0, nitems, items, &junk);

	if (choice) /* Cancel pressed */
		return;

	size = maxsize;
	if (strlen(items[1].text) > 0) {
		if (expand_number(items[1].text, &bytes) != 0) {
			char error[512];

			sprintf(error, "Invalid size: %s\n", strerror(errno));
			dialog_msgbox("Error", error, 0, 0, TRUE);
			goto addpartform;
		}
		size = MIN((intmax_t)(bytes/sector), maxsize);
	}

	/* If this is the root partition, check that this scheme is bootable */
	if (strcmp(items[2].text, "/") == 0 && !is_scheme_bootable(scheme)) {
		char message[512];
		sprintf(message, "This partition scheme (%s) is not bootable "
		    "on this platform. Are you sure you want to proceed?",
		    scheme);
		dialog_vars.defaultno = TRUE;
		choice = dialog_yesno("Warning", message, 0, 0);
		dialog_vars.defaultno = FALSE;
		if (choice == 1) /* cancel */
			goto addpartform;
	}

	/*
	 * If this is the root partition, and we need a boot partition, ask
	 * the user to add one.
	 */
	if (strcmp(items[2].text, "/") == 0 && bootpart_size(scheme) > 0) {
		choice = dialog_yesno("Boot Partition", "This partition scheme "
		    "requires a boot partition for the disk to be bootable. "
		    "Would you like to make one now?", 0, 0);

		if (choice == 0) { /* yes */
			r = gctl_get_handle();
			gctl_ro_param(r, "class", -1, "PART");
			gctl_ro_param(r, "arg0", -1, geom->lg_name);
			gctl_ro_param(r, "flags", -1, GPART_FLAGS);
			gctl_ro_param(r, "verb", -1, "add");
			gctl_ro_param(r, "type", -1, "freebsd-boot");
			snprintf(sizestr, sizeof(sizestr), "%jd",
			    bootpart_size(scheme) / sector);
			gctl_ro_param(r, "size", -1, sizestr);
			snprintf(startstr, sizeof(startstr), "%jd", firstfree);
			gctl_ro_param(r, "start", -1, startstr);
			gctl_rw_param(r, "output", sizeof(output), output);
			errstr = gctl_issue(r);
			if (errstr != NULL && errstr[0] != '\0') 
				gpart_show_error("Error", NULL, errstr);
			gctl_free(r);

			get_part_metadata(strtok(output, " "), 1)->bootcode = 1;

			/* Now adjust the part we are really adding forward */
			firstfree += bootpart_size(scheme) / sector;
			if (stripe > 0 && (firstfree*sector % stripe) != 0) 
				firstfree += (stripe - ((firstfree*sector) %
				    stripe)) / sector;
		}
	}
	
	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, geom->lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "verb", -1, "add");

	gctl_ro_param(r, "type", -1, items[0].text);
	snprintf(sizestr, sizeof(sizestr), "%jd", size);
	gctl_ro_param(r, "size", -1, sizestr);
	snprintf(startstr, sizeof(startstr), "%jd", firstfree);
	gctl_ro_param(r, "start", -1, startstr);
	if (items[3].text[0] != '\0')
		gctl_ro_param(r, "label", -1, items[3].text);
	gctl_rw_param(r, "output", sizeof(output), output);

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
		goto addpartform;
	}

	if (strcmp(items[0].text, "freebsd-boot") == 0)
		get_part_metadata(strtok(output, " "), 1)->bootcode = 1;
	else if (strcmp(items[0].text, "freebsd") == 0)
		gpart_partition(strtok(output, " "), "BSD");
	else
		set_part_metadata(strtok(output, " "), scheme, items[0].text,
		    items[2].text, 1);

	for (i = 0; i < (sizeof(items) / sizeof(items[0])); i++)
		if (items[i].text_free)
			free(items[i].text);
	gctl_free(r);
}
	
void
gpart_delete(struct gprovider *pp)
{
	struct gconfig *gc;
	struct gctl_req *r;
	const char *errstr;
	intmax_t index;

	if (strcmp(pp->lg_geom->lg_class->lg_name, "PART") != 0) {
		dialog_msgbox("Error", "Only partitions can be deleted.", 0, 0,
		    TRUE);
		return;
	}

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, pp->lg_geom->lg_class->lg_name);
	gctl_ro_param(r, "arg0", -1, pp->lg_geom->lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "verb", -1, "delete");

	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "index") == 0) {
			index = atoi(gc->lg_val);
			gctl_ro_param(r, "index", sizeof(index), &index);
			break;
		}
	}

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
		return;
	}

	gctl_free(r);

	delete_part_metadata(pp->lg_name);
}

void
gpart_revert_all(struct gmesh *mesh)
{
	struct gclass *classp;
	struct gconfig *gc;
	struct ggeom *gp;
	struct gctl_req *r;
	const char *errstr;
	const char *modified;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, "PART") == 0)
			break;
	}

	if (strcmp(classp->lg_name, "PART") != 0) {
		dialog_msgbox("Error", "gpart not found!", 0, 0, TRUE);
		return;
	}

	LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
		modified = "true"; /* XXX: If we don't know (kernel too old),
				    * assume there are modifications. */
		LIST_FOREACH(gc, &gp->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "modified") == 0) {
				modified = gc->lg_val;
				break;
			}
		}

		if (strcmp(modified, "false") == 0)
			continue;

		r = gctl_get_handle();
		gctl_ro_param(r, "class", -1, "PART");
		gctl_ro_param(r, "arg0", -1, gp->lg_name);
		gctl_ro_param(r, "verb", -1, "undo");

		errstr = gctl_issue(r);
		if (errstr != NULL && errstr[0] != '\0') 
			gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
	}
}

void
gpart_commit(struct gmesh *mesh)
{
	struct partition_metadata *md;
	struct gclass *classp;
	struct ggeom *gp;
	struct gconfig *gc;
	struct gconsumer *cp;
	struct gprovider *pp;
	struct gctl_req *r;
	const char *errstr;
	const char *modified;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, "PART") == 0)
			break;
	}

	if (strcmp(classp->lg_name, "PART") != 0) {
		dialog_msgbox("Error", "gpart not found!", 0, 0, TRUE);
		return;
	}

	LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
		modified = "true"; /* XXX: If we don't know (kernel too old),
				    * assume there are modifications. */
		LIST_FOREACH(gc, &gp->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "modified") == 0) {
				modified = gc->lg_val;
				break;
			}
		}

		if (strcmp(modified, "false") == 0)
			continue;

		/* Add bootcode if necessary, before the commit */
		md = get_part_metadata(gp->lg_name, 0);
		if (md != NULL && md->bootcode)
			gpart_bootcode(gp);

		/* Now install partcode on its partitions, if necessary */
		LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
			md = get_part_metadata(pp->lg_name, 0);
			if (md == NULL || !md->bootcode)
				continue;
		
			/* Mark this partition active if that's required */
			gpart_activate(pp);

			/* Check if the partition has sub-partitions */
			LIST_FOREACH(cp, &pp->lg_consumers, lg_consumers)
				if (strcmp(cp->lg_geom->lg_class->lg_name,
				    "PART") == 0)
					break;

			if (cp == NULL) /* No sub-partitions */
				gpart_partcode(pp);
		}

		r = gctl_get_handle();
		gctl_ro_param(r, "class", -1, "PART");
		gctl_ro_param(r, "arg0", -1, gp->lg_name);
		gctl_ro_param(r, "verb", -1, "commit");

		errstr = gctl_issue(r);
		if (errstr != NULL && errstr[0] != '\0') 
			gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
	}
}

