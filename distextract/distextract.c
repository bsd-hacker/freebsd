#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <archive.h>
#include <dialog.h>

static int extract_files(int nfiles, const char **files);

int
main(void)
{
	char *diststring = strdup(getenv("DISTRIBUTIONS"));
	const char **dists;
	int i, retval, ndists = 0;
	for (i = 0; diststring[i] != 0; i++)
		if (isspace(diststring[i]) && !isspace(diststring[i+1]))
			ndists++;
	ndists++; /* Last one */

	dists = calloc(ndists, sizeof(const char *));
	for (i = 0; i < ndists; i++)
		dists[i] = strsep(&diststring, " \t");

	chdir(getenv("BSDINSTALL_CHROOT"));
	retval = extract_files(ndists, dists);

	free(diststring);
	free(dists);

	return (retval);
}

static int
extract_files(int nfiles, const char **files)
{
	const char *items[nfiles*2];
	char path[PATH_MAX];
	int archive_files[nfiles];
	int total_files, current_files, archive_file;
	struct archive *archive, *disk;
	struct archive_entry *entry;
	char errormsg[512];
	char status[8];
	const void *block;
	size_t bsize;
	off_t offset;
	int i, err, progress, last_progress;

	err = 0;
	progress = 0;
	
	/* Make the transfer list for dialog */
	for (i = 0; i < nfiles; i++) {
		items[i*2] = strrchr(files[i], '/');
		if (items[i*2] != NULL)
			items[i*2]++;
		else
			items[i*2] = files[i];
		items[i*2 + 1] = "Pending";
	}

	init_dialog(stdin, stdout);
	dialog_vars.backtitle = __DECONST(char *, "FreeBSD Installer");
	dlg_put_backtitle();
	dialog_msgbox("",
	    "Checking distribution archives.\nPlease wait...", 0, 0, FALSE);

	/* Open all the archives */
	total_files = 0;
	for (i = 0; i < nfiles; i++) {
		archive = archive_read_new();
		archive_read_support_format_all(archive);
		archive_read_support_compression_all(archive);
		sprintf(path, "%s/%s", getenv("BSDINSTALL_DISTDIR"), files[i]);
		err = archive_read_open_filename(archive, path, 4096);
		if (err != ARCHIVE_OK) {
			snprintf(errormsg, sizeof(errormsg),
			    "Error while extracting %s: %s\n", items[i*2],
			    archive_error_string(archive));
			items[i*2 + 1] = "Failed";
			dialog_msgbox("Extract Error", errormsg, 0, 0,
			    TRUE);
			goto exit;
		}
		archive_files[i] = 0;
		while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
			archive_files[i]++;
		total_files += archive_files[i];
		archive_read_free(archive);
	}

	current_files = 0;
	disk = archive_write_disk_new();
	archive_write_disk_set_options(disk, ARCHIVE_EXTRACT_TIME |
	    ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
	    ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_OWNER |
	    ARCHIVE_EXTRACT_XATTR);
	archive_write_disk_set_standard_lookup(disk);

	for (i = 0; i < nfiles; i++) {
		archive = archive_read_new();
		archive_read_support_format_all(archive);
		archive_read_support_compression_all(archive);
		sprintf(path, "%s/%s", getenv("BSDINSTALL_DISTDIR"), files[i]);
		err = archive_read_open_filename(archive, path, 4096);

		items[i*2 + 1] = "In Progress";
		archive_file = 0;

		while ((err = archive_read_next_header(archive, &entry)) ==
		    ARCHIVE_OK) {
			last_progress = progress;
			progress = (current_files*100)/total_files; 

			sprintf(status, "-%d",
			    (archive_file*100)/archive_files[i]);
			items[i*2 + 1] = status;

			if (progress > last_progress)
				dialog_mixedgauge("Archive Extraction",
				    "Extracting distribution files...", 0, 0,
				    progress, nfiles,
				    __DECONST(char **, items));

			err = archive_write_header(disk, entry);
			if (err != ARCHIVE_OK)
				break;

			while (1) {
				err = archive_read_data_block(archive,
				    &block, &bsize, &offset);
				if (err != ARCHIVE_OK)
					break;
				err = archive_write_data_block(disk,
				    block, bsize, offset);
				if (err != ARCHIVE_OK)
					break;
			}

			if (err != ARCHIVE_EOF)
				break;
			archive_write_finish_entry(disk);

			archive_file++;
			current_files++;
		}

		items[i*2 + 1] = "Done";

		if (err != ARCHIVE_EOF) {
			const char *errstring;
			if (archive_errno(archive) != 0)
				errstring = archive_error_string(archive);
			else
				errstring = archive_error_string(disk);

			snprintf(errormsg, sizeof(errormsg),
			    "Error while extracting %s: %s\n", items[i*2],
			    errstring);
			items[i*2 + 1] = "Failed";
			dialog_msgbox("Extract Error", errormsg, 0, 0,
			    TRUE);
			goto exit;
		}

		archive_read_free(archive);
	}

	err = 0;
exit:
	end_dialog();

	return (err);
}
