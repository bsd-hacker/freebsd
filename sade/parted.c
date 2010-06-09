/*-
 * Copyright (c) 2010 Andrey V. Elsukov <bu7cher@yandex.ru>
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <libutil.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>
#include <dialog.h>
#include <ctype.h>
#include <sysexits.h>
#include <assert.h>
#include <sade.h>
#include <libsade.h>
#include "customdlg.h"

#define VTY_STATUS_LINE		24
#define TTY_STATUS_LINE		23

static DMenu schemes_menu = {
	.type = DMENU_NORMAL_TYPE,
	.title = "Select Partitioning Scheme",
	.prompt =
	    "The selected device does not have any of supported by kernel\n"
	    "partitioning schemes. Please select one scheme to use.\n\n"
	    "Use the arrow keys to move and press [ENTER] to select.\n"
	    "Use [TAB] to get to the buttons and leave this menu.",
	.helpline = "Press F1 for help"
};

static struct {
	int		level;
	char		*name;
	char		*desc;
	char		*module;
} sade_schemes[] = {
	{0,	"MBR",	"Master Boot Record",	"geom_part_mbr"},
	{0,	"GPT",	"GUID Partition Table",	"geom_part_gpt"},
	{0,	"BSD",	"BSD disklabels",	"geom_part_bsd"},
#ifdef WITH_ZFS
	{0,	"ZFS",	"ZFS storage pool",	"zfs"},
	{1,	"ZFS",	"ZFS storage pool",	"zfs"},
#endif
	{0,	"APM",	"Apple Partition Map",	"geom_part_apm"},
	{0,	"PC98",	"NEC PC98 Master Boot Record",	"geom_part_pc98"},
	{0,	"VTOC8","SMI VTOC8 labels",	"geom_part_vtoc8"},
	{2,	"BSD",	"BSD disklabels",	"geom_part_bsd"},
	{0,	NULL,	NULL,			NULL}
};

static char *kldload_msg =
	"Selected scheme is not compiled in kernel. Would you like\n"
	"to load it from kernel module?";
static char *undo_msg = "Are you SURE you want to undo everything?";
static char *write_confirm_msg =
	"WARNING: You are about to save all your changes to device.\n"
	"After that you will can not undo your changes.\n\n"
	"Are you absolutely sure you want to continue?";
static char *pending_write_msg =
	"WARNING: There are some changes pending of write to device.\n\n"
	"Would you like to save these changes?";
static char *destroy_scheme_msg =
	"Would you like to destroy current partitioning scheme and\n"
	"select another one?";
static char *bootcode_confirm_msg =
	"WARNING: You are about to write a bootstrap code to device or\n"
	"partition. This also will lead to saving of all your changes.\n\n"
	"Are you absolutely sure you want to continue?";
static char *mbr_bootmgr_msg =
	"FreeBSD comes with a boot manager that allows you to easily\n"
	"select between FreeBSD and any other operating systems on your machine\n"
	"at boot time.  If you have more than one drive and want to boot\n"
	"from the second one, the boot manager will also make it possible\n"
	"to do so (limitations in the PC BIOS usually prevent this otherwise).\n"
	"Would you like to install boot manager?\n\n"
	"Press \"YES\" to install boot manager or \"NO\" to use standard MBR.";
static char *gpt_bootcode_msg =
	"Would you like to use a ZFS aware bootstrap code?";

static char *add_slice_title =
	"Please specify the slice type (or select it from the list),\n"
	"start offset and slice size, which should be specified in\n"
	"sectors or in size units like K, M, G, T. (e.g. 20M)";
static char *set_type_title =
	"Please specify the slice type or select it from the list.";
static char *set_label_title = "Please enter the slice label:";

static const char *apm_aliases[] = { "freebsd", "freebsd-swap", "freebsd-ufs",
	"freebsd-vinum", "freebsd-zfs", NULL };
static const char *bsd_aliases[] = { "freebsd-swap", "freebsd-ufs", "freebsd-vinum",
	"freebsd-zfs", NULL };
static const char *gpt_aliases[] = { "freebsd", "freebsd-boot", "freebsd-swap",
	"freebsd-ufs", "freebsd-vinum", "freebsd-zfs", "apple-boot",
	"apple-hfs", "apple-label", "apple-raid", "apple-raid-offline",
	"apple-tv-recovery", "apple-ufs", "efi", "linux-data", "linux-lvm",
	"linux-raid", "linux-swap", "ms-basic-data", "ms-ldm-data",
	"ms-ldm-metadata", "ms-reserved", "netbsd-ccd", "netbsd-cgd",
	"netbsd-ffs", "netbsd-lfs", "netbsd-raid", "netbsd-swap", "mbr", NULL };
static const char *mbr_aliases[] = { "freebsd", NULL };
static const char *pc98_aliases[] = { "freebsd", NULL };
static const char *vtoc8_aliases[] = { "freebsd-swap", "freebsd-ufs",
	"freebsd-vinum", "freebsd-zfs", NULL };

static void
set_statusline(char *msg)
{
	if (msg) {
		attrset(title_attr);
		mvprintw(LINES - 1, 0, msg);
		attrset(A_NORMAL);
		beep();
	} else {
		move(LINES - 1, 0);
		clrtoeol();
	}
}

static int
parted_create_scheme(dialogMenuItem *pitem)
{
	int ret, loaded = 0;
	struct de_device *pdev = pitem->data;
again:
	if (strcmp(pitem->prompt, "ZFS") == 0) {
		ret = DITEM_RESTORE | DITEM_CONTINUE;
		return (ret);
	}
	ret = de_dev_scheme_create(pdev, pitem->prompt);
	if (ret) {
		if (ret == EINVAL && loaded == 0) {
			loaded = dmenu_open_yesno(kldload_msg);
			if (loaded == 0) {
				loaded = kldload(sade_schemes[pitem->aux].module);
				if (loaded != -1)
					goto again;
				else
					dmenu_open_errormsg(strerror(loaded));
			}
		} else if (ret != 0)
			dmenu_open_errormsg((ret < 0) ? de_error():
			    strerror(ret));
		ret = DITEM_RESTORE | DITEM_CONTINUE;
	} else
		ret = DITEM_LEAVE_MENU;
	return (ret);
}

static int
dmenu_open_schemes(struct de_device *pdev, int level)
{
	dialogMenuItem *pitem;
	int ret, count, i;

	for (i = 0, count = 0; sade_schemes[i].name; i++)
		if (level == sade_schemes[i].level)
			count++;
	if (count == 0)
		err(EX_SOFTWARE, "schemes list is empty for level %d\n", level);
	schemes_menu.items = malloc(sizeof(dialogMenuItem) * (count + 1));
	pitem = schemes_menu.items;
	if (pitem != NULL) {
		bzero(pitem, sizeof(dialogMenuItem) * (count + 1));
		for (i = 0; sade_schemes[i].name; i++) {
			if (level != sade_schemes[i].level)
				continue;
			pitem->prompt = sade_schemes[i].name;
			pitem->title = sade_schemes[i].desc;
			pitem->fire = parted_create_scheme;
			pitem->data = pdev;
			pitem->aux = i;
			pitem++;
		}
		ret = dmenu_open(&schemes_menu, NULL, NULL, 0);
		free(schemes_menu.items);
	} else
		ret = ENOMEM;

	return (ret);
}

static int
parted_reread_device(struct de_device *pdev)
{
	int error;
	de_dev_partlist_free(pdev);
	error = de_partlist_get(pdev);
	return (error);
}

static int
up_down_keyhndl(int key)
{
	switch (key) {
	case KEY_UP:
	case KEY_DOWN:
		return (key);
	}
	return (0);
}

static int
right_left_keyhndl(int key)
{
	switch (key) {
	case KEY_RIGHT:
	case KEY_LEFT:
		return (key);
	}
	return (0);
}


static int
parted_expand_number(struct de_part *ppart, const char *buf, uint64_t *num)
{
	uint64_t value, tmp;
	char *endp;

	if (buf == NULL)
		return (EINVAL);
	value = (uint64_t)strtoll(buf, &endp, 10);
	if (endp == buf)
		return (EINVAL);
	if (endp != NULL && *endp != '\0') {
		if (expand_number(buf, &tmp) == 0) {
			*num = tmp / ppart->de_device->de_sectorsize;
			return (0);
		} else
			return (EINVAL);
	}
	*num = value;
	return (0);
}

static int
parted_type_aliases(const char *scheme, const char ***list)
{
	int cnt;
	if (strcmp(scheme, "APM") == 0)
		*list = apm_aliases;
	else if (strcmp(scheme, "BSD") == 0)
		*list = bsd_aliases;
	else if (strcmp(scheme, "GPT") == 0)
		*list = gpt_aliases;
	else if (strcmp(scheme, "MBR") == 0)
		*list = mbr_aliases;
	else if (strcmp(scheme, "PC98") == 0)
		*list = pc98_aliases;
	else if (strcmp(scheme, "VTOC8") == 0)
		*list = vtoc8_aliases;
	else
		return (0);
	for (cnt = 0; (*list)[cnt] != NULL; cnt++);
	return (cnt);
}

static int
parted_add_slice(struct de_part *ppart)
{
	struct custom_dlg dlg;
	struct dlg_item *item;
	DLG_EDIT *eType, *eStart, *eSize, *eLabel = NULL;
	DLG_BUTTON *btnOk, *btnCancel;
	DLG_LIST *ltType;
	WINDOW *win;
	char buf[20];
	const char **list = NULL;
	int ret, h, w, cnt, q;

	win = savescr();
	dlg_init(&dlg);
	dlg_add_label(&dlg, 1, 2, strwidth(add_slice_title), 4,
	    add_slice_title);
	eType = dlg_add_edit(&dlg, 5, 2, 25, "Slice Type:", 40, NULL);
	cnt = parted_type_aliases(ppart->de_device->de_scheme, &list);
	ltType = dlg_add_list(&dlg, 9, 2, 25, 8, NULL, cnt, list);
	eStart = dlg_add_edit(&dlg, 5, 28, 25, "Start Offset:", 20, "*");
	snprintf(buf, sizeof(buf) - 1, "%jd", ppart->de_end -
	    ppart->de_start + 1);
	eSize = dlg_add_edit(&dlg, 9, 28, 25, "Size:", 20, buf);
	if (list == apm_aliases || list == gpt_aliases)
		eLabel = dlg_add_edit(&dlg, 13, 28, 25, "Label:", 16, NULL);
	btnOk = dlg_add_button(&dlg, 5, 54, "  OK  ");
	btnCancel = dlg_add_button(&dlg, 7, 54, "Cancel");
	use_helpline("Press F1 for help");
	dlg_autosize(&dlg, &w, &h);
	dlg_open_dialog(&dlg, w + 1, h + 1, "Add Slice");
again:
	q = 0;
	do {
		ret = dlg_proc(&dlg, up_down_keyhndl);
		if (ret == DE_ESC) {
			q = 1;
			break;
		}
		item = dlg_focus_get(&dlg);
		switch (ret) {
		case DE_CR:
			if (item == btnCancel)
				q = 1;
			else if (item == btnOk)
				q = 2;
			else if (item == ltType) {
				dlg_edit_set_value(&dlg, eType,
				    dlg_list_get_choice(&dlg, ltType));
				dlg_focus_next(&dlg);
			} else
				dlg_focus_next(&dlg);
		case KEY_UP:
		case KEY_DOWN:
			if (item == btnCancel)
				dlg_focus_prev(&dlg);
			if (item == btnOk)
				dlg_focus_next(&dlg);
			break;
		}
	} while (q == 0);

	if (q == 2) {
		const char *s;
		char *label = NULL;
		uint64_t start = 0, size = 0;
		s = dlg_edit_get_value(&dlg, eStart);
		if (s != NULL && *s != '*') {
			ret = parted_expand_number(ppart, s, &start);
			if (ret != 0) {
				dmenu_open_errormsg("incorrect start offset");
				goto again;
			}
		}
		s = dlg_edit_get_value(&dlg, eSize);
		if (s != NULL && *s != '*') {
			ret = parted_expand_number(ppart, s, &size);
			if (ret != 0) {
				dmenu_open_errormsg("incorrect slice size");
				goto again;
			}
		}
		if (list == apm_aliases || list == gpt_aliases)
			label = dlg_edit_get_value(&dlg, eLabel);
		ret = de_part_add(ppart->de_device, dlg_edit_get_value(&dlg, eType),
		    (off_t)start, (off_t)size, label, 0);
		if (ret != 0) {
			dmenu_open_errormsg((ret < 0) ? de_error():
			    strerror(ret));
			goto again;
		}
		q = 0;
	}
	dlg_close_dialog(&dlg);
	dlg_free(&dlg);
	restorescr(win);
	return (q);
}

static int
parted_set_type(struct de_part *ppart)
{
	struct custom_dlg dlg;
	struct dlg_item *item;
	DLG_EDIT *eType, *eLabel;
	DLG_LIST *ltType;
	DLG_BUTTON *btnOk, *btnCancel;
	WINDOW *win;
	const char **list = NULL;
	int ret, h, w, cnt, q;

	win = savescr();
	dlg_init(&dlg);
	cnt = parted_type_aliases(ppart->de_device->de_scheme, &list);
	dlg_add_label(&dlg, 1, 2, strwidth(set_type_title), 1,
	    set_type_title);
	ltType = dlg_add_list(&dlg, 3, 2, 25, 8, "Slice Types:", cnt, list);
	eType = dlg_add_edit(&dlg, 3, 28, 25, "Slice Type:", 40, NULL);
	if (list == apm_aliases || list == gpt_aliases)
		eLabel = dlg_add_edit(&dlg, 7, 28, 25, "Label:", 16, NULL);
	btnOk = dlg_add_button(&dlg, 3, 54, "  OK  ");
	btnCancel = dlg_add_button(&dlg, 5, 54, "Cancel");
	use_helpline("Press F1 for help");
	dlg_autosize(&dlg, &w, &h);
	dlg_open_dialog(&dlg, w + 1, h + 1, "Select Slice Type");
again:
	q = 0;
	do {
		ret = dlg_proc(&dlg, up_down_keyhndl);
		if (ret == DE_ESC) {
			q = 1;
			break;
		}
		item = dlg_focus_get(&dlg);
		switch (ret) {
		case DE_CR:
			if (item == btnCancel)
				q = 1;
			else if (item == btnOk)
				q = 2;
			else if (item == ltType) {
				dlg_edit_set_value(&dlg, eType,
				    dlg_list_get_choice(&dlg, ltType));
				dlg_focus_next(&dlg);
			} else
				dlg_focus_next(&dlg);
		case KEY_UP:
		case KEY_DOWN:
			if (item == btnCancel)
				dlg_focus_prev(&dlg);
			if (item == btnOk)
				dlg_focus_next(&dlg);
			break;
		}
	} while (q == 0);

	if (q == 2) {
		const char *type, *label = NULL;
		assert(ppart->de_index > 0);
		type = dlg_edit_get_value(&dlg, eType);
		if (list == apm_aliases || list == gpt_aliases) {
			label = dlg_edit_get_value(&dlg, eLabel);
			/* do not reset current label if it was
			 * not touched.
			 */
			if (*label == '\0')
				label = NULL;
		}
		ret = de_part_mod(ppart->de_device, type,
		    label, ppart->de_index);
		if (ret != 0) {
			dmenu_open_errormsg((ret < 0) ? de_error():
			    strerror(ret));
			goto again;
		}
		q = 0;
	}
	restorescr(win);
	dlg_close_dialog(&dlg);
	dlg_free(&dlg);
	return (q);
}

static int
parted_set_label(struct de_part *ppart)
{
	struct custom_dlg dlg;
	struct dlg_item *item;
	DLG_EDIT *eLabel;
	DLG_BUTTON *btnCancel;
	WINDOW *win;
	int ret, h, w, q;

	win = savescr();
	dlg_init(&dlg);
	eLabel = dlg_add_edit(&dlg, 1, 2, 40, set_label_title,
	    16, NULL);
	dlg_add_button(&dlg, 5, 10, "  OK  ");
	btnCancel = dlg_add_button(&dlg, 5, 26, "Cancel");
	use_helpline("Press F1 for help");
	dlg_autosize(&dlg, &w, &h);
	dlg_open_dialog(&dlg, w + 1, h, "Set Label");
again:
	q = 0;
	do {
		ret = dlg_proc(&dlg, right_left_keyhndl);
		if (ret == DE_ESC) {
			q = 1;
			break;
		}
		item = dlg_focus_get(&dlg);
		switch (ret) {
		case DE_CR:
			if (item == btnCancel)
				q = 1;
			else
				q = 2;
			break;
		case KEY_RIGHT:
		case KEY_LEFT:
			if (item == btnCancel)
				dlg_focus_prev(&dlg);
			else
				dlg_focus_next(&dlg);
			break;
		}
	} while (q == 0);

	if (q == 2) {
		const char *label;
		assert(ppart->de_index > 0);
		label = dlg_edit_get_value(&dlg, eLabel);
		ret = de_part_mod(ppart->de_device, NULL, label,
		    ppart->de_index);
		if (ret != 0) {
			dmenu_open_errormsg((ret < 0) ? de_error():
			    strerror(ret));
			goto again;
		}
		q = 0;
	}
	restorescr(win);
	dlg_close_dialog(&dlg);
	dlg_free(&dlg);
	return (q);

}

#define MBR_PATH	"/boot/mbr"
#define BOOT0_PATH	"/boot/boot0"
#define PMBR_PATH	"/boot/pmbr"
#define GPTBOOT_PATH	"/boot/gptboot"
#define GPTZFSBOOT_PATH	"/boot/gptzfsboot"
#define BSD_PATH	"/boot/boot"
#define HAS_SCHEME(pdev)	\
	((pdev)->de_scheme != NULL && strcmp((pdev)->de_scheme, "(none)") != 0)

static int
parted_write_bootcode(struct de_part *ppart, char **msg)
{
	int error = 0, ret;
	struct de_device *pdev = ppart->de_device;
	const char *partcode = NULL, *devcode = NULL;

	ret = dmenu_open_noyes(bootcode_confirm_msg);
	if (ret)
		return (1);
	/* MBR Scheme */
	if (strcmp(pdev->de_scheme, "MBR") == 0) {
		ret = dmenu_open_yesno(mbr_bootmgr_msg);
		devcode = ret ? MBR_PATH: BOOT0_PATH;
	} else if (strcmp(pdev->de_scheme, "GPT") == 0) {
		if (ppart->de_type == NULL) {
			*msg = "Slice is unused, create it first or move to"
			    " another one.";
			return (1);
		}
		if (strcmp(ppart->de_type, "freebsd-boot") != 0) {
			*msg = "This is not a freebsd-boot slice.";
			return (1);
		}
		ret = dmenu_open_yesno(gpt_bootcode_msg);
		partcode = ret ? GPTBOOT_PATH: GPTZFSBOOT_PATH;
		devcode = PMBR_PATH;
	} else if (strcmp(pdev->de_scheme, "BSD") == 0) {
		devcode = BSD_PATH;
	} else {
		*msg = "Writing bootstrap code for this scheme is not"
		    " implemented.";
		return (1);
	}
	if (partcode)
		error = de_part_bootcode(ppart, partcode);
	if (error == 0 && devcode)
		error = de_dev_bootcode(pdev, devcode);
	if (error == 0)
		error = de_dev_commit(pdev);
	if (error != 0) {
		*msg = (error < 0) ? de_error(): strerror(error);
		error = 1;
	}
	return (error);
}

#define PARTED_MENU_TOP		5
#define PARTED_BOTTOM_HEIGHT	10
#define LABEL(l)	(l) ? (l): "-"
#define FLAGS(l)	(l) ? "A": "-"

int
parted_open(struct de_device *pdev, int level)
{
	int ret = 0, error = 0, q = 0;
	int sc = 0, ch = 0;
	int changed = 0;

	error = de_partlist_get(pdev);
	if (error)
		return (error);
reload:
	if (!HAS_SCHEME(pdev)) {
		ret = dmenu_open_schemes(pdev, level);
		if (ret == 0) {
			changed = 1;
			error = parted_reread_device(pdev);
		} else if (changed) {
			ret = dmenu_open_yesno(pending_write_msg);
			if (ret)
				error = de_dev_undo(pdev);
			else
				error = de_dev_commit(pdev);
			ret = 1; /* any way we are exiting */
		}
	}
	if (ret == 0 && error == 0) {
		WINDOW *win;
		int count, height, row, i, key, hsc;
		struct de_part *ppart, *selected = NULL;
		struct de_device dev;
		char dname[20], dtype[20], dlabel[20];
		char *msg = NULL;

		win = savescr();
		keypad(stdscr, TRUE);
		dialog_clear_norefresh(); clear();
		count = de_partlist_count(&pdev->de_part);
resize:
		if (LINES > VTY_STATUS_LINE)
			height = LINES - 1;
		else
			height = VTY_STATUS_LINE;
		height -= PARTED_MENU_TOP + PARTED_BOTTOM_HEIGHT;
		hsc = 0;
		do {
			attrset(A_NORMAL);
			mvprintw(0, 0, "%-15s", "Device:");
			attrset(A_REVERSE);
			addstr(pdev->de_name);
			attrset(A_NORMAL);
			clrtobot(); attrset(A_REVERSE);
			mvprintw(0, 64, "Partition Editor");
			attrset(A_NORMAL);
			mvprintw(1, 0,"%-15s%s scheme, %jd sectors (%s), "
			    "sector size %d", "Device Info:", pdev->de_scheme,
			    pdev->de_mediasize / pdev->de_sectorsize,
			    fmtsize(pdev->de_mediasize),
			    pdev->de_sectorsize);
			mvprintw(3, 0,"%12s%20s%12s%11s%11s%6s%7s", "Name",
			    "Type", "Label", "Start", "End", "Size", "Flags");
			row = PARTED_MENU_TOP - 1;
			if (sc > 0)
				mvprintw(row, 11, "^(-)");
			else {
				move(row, 0);
				clrtoeol();
			}
			i = 0;
			TAILQ_FOREACH(ppart, &pdev->de_part, de_part) {
				if (i++ < sc)
					continue;
				if (++row - PARTED_MENU_TOP > height - 1)
					break;
				if (ch == row - PARTED_MENU_TOP) {
					attrset(A_REVERSE);
					selected = ppart;
				}
				mvprintw(row, 0, "%12s%20s%12s%11jd%11jd%6s%7s",
				    hscroll_str(dname, 12, LABEL(ppart->de_name),
					selected == ppart ? hsc: 0, HSCROLL_LEFT),
				    hscroll_str(dtype, 19, LABEL(ppart->de_type), 0, HSCROLL_RIGHT),
				    hscroll_str(dlabel,11, LABEL(ppart->de_label), 0, HSCROLL_RIGHT),
				    ppart->de_start, ppart->de_end,
				    fmtsize((ppart->de_end -
					ppart->de_start + 1) *
					pdev->de_sectorsize),
				    FLAGS(ppart->de_private));
				if (ch == row - PARTED_MENU_TOP)
					attrset(A_NORMAL);
			}
			if (sc + height < count)
				mvprintw(height + PARTED_MENU_TOP, 11, "v(+)");
			else {
				move(height + PARTED_MENU_TOP, 0);
				clrtoeol();
			}
			mvprintw(height + PARTED_MENU_TOP + 1, 0,
			    "The following commands are supported:");
			mvprintw(height + PARTED_MENU_TOP + 3, 0,
			    "C = Create Slice          D = Delete Slice    "
			    "      T = Change Type");
			mvprintw(height + PARTED_MENU_TOP + 4, 0,
			    "Q = Finish                U = Undo All Changes"
			    "      W = Write Changes");
			mvprintw(height + PARTED_MENU_TOP + 5, 0,
			    "B = Write Boot Code       ENTER = Inspect     ");
			if (level == 0) {
				addstr("      S = Set Active");
				mvprintw(height + PARTED_MENU_TOP + 6, 0,
				    "L = Set Label");
			}
			mvprintw(height + PARTED_MENU_TOP + 8, 0,
			    "Use F1 or ? to get more help, arrow keys to select");
			set_statusline(msg);
			if (msg)
				msg = NULL;

			key = toupper(getch());
			switch (key) {
			case '\r':
			case '\n':
				if (level > 1)
					break;
				if (selected->de_type == NULL || (
				    strcmp(selected->de_type, "freebsd") != 0
#ifdef WITH_ZFS
				    && strcmp(selected->de_type, "freebsd-zfs") != 0
#endif
				    )) {
					msg = "This is not a freebsd slice "
					    "or ZFS pool.";
					break;
				}
				bzero(&dev, sizeof(dev));
				TAILQ_INIT(&dev.de_part);
				dev.de_name = selected->de_name;
				dev.de_sectorsize = pdev->de_sectorsize;
				dev.de_mediasize = (selected->de_end -
				    selected->de_start + 1) * dev.de_sectorsize;
				if (strcmp(selected->de_type, "freebsd") == 0)
					error = parted_open(&dev, 2);
#ifdef WITH_ZFS
				else
					error = zfsed_open(&dev);
#endif
				if (error == 1) /* cancelled */
					error = 0;
				break;
			case '\014': /* ^L (redraw) */
				clear();
				break;
			case KEY_UP:
			case KEY_DOWN:
			case KEY_PPAGE:
			case KEY_HOME:
			case KEY_NPAGE:
			case KEY_END:
				dlg_list_handle_move(key, &ch, &sc, count,
				    height);
				break;
			case KEY_LEFT:
				if (hsc < 20)
					hsc++;
				break;
			case 'B': /* Write bootstrap code */
				ret = parted_write_bootcode(selected, &msg);
				if (ret == 0)
					changed = 0;
				break;
			case KEY_ESC:
			case 'Q':
				if (!changed) {
					ret = 1; q = 1;
					break;
				}
				if (key == KEY_ESC)
					ret = dmenu_open_noyes(pending_write_msg);
				else
					ret = dmenu_open_yesno(pending_write_msg);
				if (ret)
					error = de_dev_undo(pdev);
				else
					error = de_dev_commit(pdev);
				if (error != 0 && key != KEY_ESC)
					break;
				q = 1;
				break;
			case 'C':
				if (selected->de_type != NULL) {
					msg = "Slice in use, delete it first "
					    "or move to an unused one.";
					break;
				}
				ret = parted_add_slice(selected);
				if (ret == 0) {
					changed = 1;
					error = parted_reread_device(pdev);
					if (error == 0)
						goto reload;
				}
				break;
			case 'D':
				if (count == 1 && selected->de_type == NULL) {
					if (dmenu_open_noyes(destroy_scheme_msg))
						break;
					error = de_dev_scheme_destroy(pdev);
					if (error != 0)
						break;
					changed = 1;
					error = parted_reread_device(pdev);
					if (error != 0)
						break;
					sc = ch = 0;
					goto reload;
				}
				if (selected->de_type == NULL) {
					msg = "Slice is already unused!";
					break;
				}
				error = de_part_del(pdev, selected->de_index);
				if (error == 0) {
					changed = 1;
					error = parted_reread_device(pdev);
					if (error == 0) {
						sc = ch = 0;
						goto reload;
					}
				}
				break;
			case 'L':
				if (level > 1)
					break;
				if (strcmp(pdev->de_scheme, "APM") != 0 &&
				    strcmp(pdev->de_scheme, "GPT") != 0) {
					msg = "Labes are not supported "
					    "by current scheme.";
					break;
				}
				if (selected->de_type == NULL) {
					msg = "Slice is unused, create it first "
					    "or move to another one.";
					break;
				}
				ret = parted_set_label(selected);
				if (ret == 0) {
					changed = 1;
					error = parted_reread_device(pdev);
					if (error == 0)
						goto reload;
				}
				break;
			case 'S':
				if (level > 1)
					break;
				if (strcmp(pdev->de_scheme, "MBR") != 0 &&
				    strcmp(pdev->de_scheme, "PC98") != 0) {
					msg = "Active attribute is not "
					    "supported by current scheme.";
					break;
				}
				if (selected->de_type == NULL) {
					msg = "You should select an existing "
					    "slice to set it active.";
					break;
				}
				error = de_part_setattr(pdev, "active",
				    selected->de_index);
				if (error != 0)
					break;
				changed = 1;
				error = parted_reread_device(pdev);
				if (error == 0)
					goto reload;
				break;
			case 'T':
				if (selected->de_type == NULL) {
					msg = "Slice in unused, create it first "
					    "or move to another one.";
					break;
				}
				ret = parted_set_type(selected);
				if (ret == 0) {
					changed = 1;
					error = parted_reread_device(pdev);
					if (error == 0)
						goto reload;
				}
				break;
			case 'U':
				if (!changed) {
					msg = "Nothing to undo.";
					break;
				}
				if (dmenu_open_noyes(undo_msg))
					break;
				error = de_dev_undo(pdev);
				if (error != 0)
					break;
				changed = 0;
				error = parted_reread_device(pdev);
				if (error == 0) {
					sc = ch = 0;
					goto reload;
				}
				break;
			case 'W':
				if (!changed) {
					msg = "Nothing to save.";
					break;
				}
				if (dmenu_open_noyes(write_confirm_msg))
					break;
				error = de_dev_commit(pdev);
				if (error != 0)
					break;
				changed = 0;
				break;
			case KEY_RESIZE:
				sc = ch = 0;
				goto resize;
			default:
				msg = "Type F1 or ? for help";
			}
			if (error != 0) {
				msg = (error < 0) ? de_error(): strerror(error);
				error = 0;
			}
			if (key != KEY_LEFT)
				hsc = 0;
		} while (q == 0);
		restorescr(win);
	}
	de_dev_partlist_free(pdev);
	return (ret);
}

