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
#include <paths.h>
#include <sade.h>
#include <libsade.h>
#include "customdlg.h"
#include "mntopts.h"


static char *ask_recreate_msg =
	"WARNING: Selected partition already contains a file system!\n\n"
	"Are you absolutely sure you want to recreate it?";
static char *custom_newfs_title = "Please enter custom parameters for newfs:";
static char *undo_msg = "Are you SURE you want to undo everything?";
static char *write_confirm_msg =
	"WARNING: You are about to save all your changes to device.\n"
	"After that you will can not undo your changes.\n\n"
	"Are you absolutely sure you want to continue?";
static char *pending_write_msg =
	"WARNING: There are some changes pending of write to device.\n\n"
	"Would you like to save these changes?";

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


static DMenu details_menu = {
	.type = DMENU_CHECKLIST_TYPE,
	.title = "File System Details",
	.helpline = "Press F1 for help"
};

static int
details_check(dialogMenuItem *pitem)
{
	return ((int)pitem->aux);
}

static dialogMenuItem details_items[] = {
	{"1", "File system type: " , .lbra = ' ', .mark = ' ', .rbra = ' '},
	{"2", "Volume name: ", .lbra = ' ', .mark = ' ', .rbra = ' '},
	{"3", "Soft Updates enabled", .checked = details_check },
	{"4", "Journalled Soft Updates enabled", .checked = details_check },
	{"5", "GEOM Journal enabled", .checked = details_check },
	{"6", "POSIX.1e ACLS enabled", .checked = details_check },
	{"7", "NFSv4 ACLS enabled", .checked = details_check },
	{"8", "MAC labels enabled", .checked = details_check },
	{NULL, NULL}
};

static int
fsed_inspect(struct de_fs *pfs)
{
	char vol_buf[100], fs_buf[100];
	char prompt_buf[100];
	struct de_ufs_priv *pu;

	assert(pfs != NULL);
	assert(pfs->de_partname != NULL);
	if (pfs->de_private == NULL)
		return (-1);
	pu = (struct de_ufs_priv *)pfs->de_private;
	snprintf(prompt_buf, sizeof(prompt_buf),
	    "File System parameters for %s", pfs->de_partname);
	snprintf(fs_buf, sizeof(fs_buf), "File System: %s",
	    pu->de_ufs1 ? "UFS1": "UFS2");
	details_items[0].title = (char *)fs_buf;
	snprintf(vol_buf, sizeof(vol_buf), "Volume Label: %s", pu->de_volname);
	details_items[1].title = (char *)vol_buf;
	details_items[2].aux = pu->de_su;
	details_items[3].aux = pu->de_suj;
	details_items[4].aux = pu->de_gj;
	details_items[5].aux = pu->de_acl;
	details_items[6].aux = pu->de_nfs4acl;
	details_items[7].aux = pu->de_mac;

	details_menu.items = (dialogMenuItem *)details_items;
	details_menu.prompt = (char *)prompt_buf;

	return (dmenu_open(&details_menu, NULL, NULL, 0));
}

static int
ufs_create_keyhndl(int key)
{
	switch (key) {
	case ' ':
	case KEY_UP:
	case KEY_DOWN:
	case KEY_RIGHT:
	case KEY_LEFT:
		return (key);
	}
	return (0);
}

static int
fsed_custom_ufs_create(struct de_fs *pfs)
{
	struct custom_dlg dlg;
	struct dlg_item *item;
	DLG_EDIT *eCmd;
	DLG_BUTTON *btnCancel;
	WINDOW *win;
	int ret, h, w, q;

	win = savescr();
	dlg_init(&dlg);
	eCmd = dlg_add_edit(&dlg, 1, 2, 50, custom_newfs_title,
	    200, "-O 2 -b 16384 -f 2048");
	dlg_add_button(&dlg, 5, 15, "  OK  ");
	btnCancel = dlg_add_button(&dlg, 5, 31, "Cancel");
	use_helpline("Press F1 for help");
	dlg_autosize(&dlg, &w, &h);
	dlg_open_dialog(&dlg, w + 1, h, "Create File System");
//again:
	q = 0;
	do {
		ret = dlg_proc(&dlg, ufs_create_keyhndl);
		if (ret == DE_ESC) {
			q = 1;
			break;
		}
		item = dlg_focus_get(&dlg);
		switch (ret) {
		case ' ':
		case DE_CR:
			if (item == btnCancel)
				q = 1;
			else
				q = 2;
			break;
		case KEY_UP:
		case KEY_LEFT:
			dlg_focus_prev(&dlg);
			break;
		case KEY_DOWN:
		case KEY_RIGHT:
			dlg_focus_next(&dlg);
			break;
		}
	} while (q == 0);

	if (q == 2) {
		/*
		const char *args;
		args = dlg_edit_get_value(&dlg, eCmd);
		q = 0; */
	}
	restorescr(win);
	dlg_close_dialog(&dlg);
	dlg_free(&dlg);
	return (q);

}


static int
fsed_ufs_create(struct de_fs *pfs)
{
	struct custom_dlg dlg;
	struct dlg_item *item;
	DLG_BUTTON *btnOk, *btnCancel, *btnCustom;
	DLG_EDIT *eLabel, *eBlock, *eFragment;
	DLG_CHECKBOX *cSU, *cSUJ, *cGJ, *cACL, *cNFS4ACL, *cMAC;
	WINDOW *win;
	char title_buf[100];
//	char newfs_cmd[255], tunefs_cmd[255];
	int q, h, w, ret;

	assert(pfs != NULL);
	win = savescr();

	dlg_init(&dlg);
	snprintf(title_buf, sizeof(title_buf),
	    "Options for a new file system that will be created on %s:",
	    pfs->de_partname);
	dlg_add_label(&dlg, 1, 2, 64, 2, title_buf);
	eLabel = dlg_add_edit(&dlg, 4, 2, 30, "Volume Label:",
	    MAXVOLLEN, NULL);
	eBlock = dlg_add_edit(&dlg, 4, 34, 13, "Block Size:",
	    10, "16384");
	eFragment = dlg_add_edit(&dlg, 4, 49, 15, "Fragment Size:",
	    10, "2048");
	cSU = dlg_add_checkbox(&dlg, 8, 2, 30, 1, 0, "Soft Updates");
	cSUJ = dlg_add_checkbox(&dlg, 9, 2, 30, 1, 0, "Soft Updates Journaling");
	cGJ = dlg_add_checkbox(&dlg, 10, 2, 30, 1, 0, "GEOM Journal");
	cACL = dlg_add_checkbox(&dlg, 8, 34, 30, 1, 0, "POSIX.1e ACL");
	cNFS4ACL = dlg_add_checkbox(&dlg, 9, 34, 30, 1, 0, "NFSv4 ACL");
	cMAC = dlg_add_checkbox(&dlg, 10, 34, 30, 1, 0, "MAC Multilabel");
	btnOk = dlg_add_button(&dlg, 12, 18, "Create");
	btnCustom = dlg_add_button(&dlg, 12, 30, "Custom");
	btnCancel = dlg_add_button(&dlg, 12, 42, "Cancel");
	use_helpline("Press F1 for help");
	dlg_autosize(&dlg, &w, &h);
	dlg_open_dialog(&dlg, w + 1, h + 1, "Create File System");
	q = 0;
	do {
		ret = dlg_proc(&dlg, ufs_create_keyhndl);
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
			else if (item == btnCustom)
				q = 3;
			else
				dlg_focus_next(&dlg);
			break;
		case KEY_UP:
		case KEY_LEFT:
			dlg_focus_prev(&dlg);
			break;
		case KEY_DOWN:
		case KEY_RIGHT:
			dlg_focus_next(&dlg);
			break;
		case ' ':
			if (item->type == CHECKBOX)
				dlg_checkbox_toggle(&dlg, item);
			break;
		}
	} while (q == 0);
	if (q == 2) {
		/* issue delayed command via history */
	}
	restorescr(win);
	dlg_close_dialog(&dlg);
	dlg_free(&dlg);
	if (q == 3) {
		fsed_custom_ufs_create(pfs);
	}
	return (0);
}

#define	MNT_OPS_CNT	10
static struct opt {
	int		o_opt;
	const char*	o_name;
} mntopt_names[MNT_OPS_CNT] = {
	{ MNT_RDONLY,		"read-only" },
	{ MNT_ASYNC,		"async" },
	{ MNT_NOATIME,		"noatime" },
	{ MNT_NOEXEC,		"noexec" },
	{ MNT_SUIDDIR,		"suiddir" },
	{ MNT_NOSUID,		"nosuid" },
	{ MNT_MULTILABEL,	"multilabel" },
	{ MNT_ACLS,		"acls" },
	{ MNT_NFS4ACLS,		"nfsv4acls" },
	{ 0,			"noauto"}
};

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_END
};

static int
fsed_ufs_mountops(struct de_fs *pfs)
{
	struct custom_dlg dlg, popup;
	struct dlg_item *item;
	struct de_ufs_priv *priv;
	DLG_LABEL *lLabel;
	DLG_CHECKBOX *cLabel, *cOps[MNT_OPS_CNT];
	DLG_BUTTON *btnOk, *btnCancel;
	DLG_EDIT *eMnt, *eDumpFreq, *ePassno;
	DLG_LIST *lMenu;
	WINDOW *win;
	char title_buf[100], label_buf[100], buf[32];
	char **labels;
	int mntflags;
//	char newfs_cmd[255], tunefs_cmd[255];
	int q, h, w, ret, cnt, i;

	assert(pfs != NULL);
	priv = (struct de_ufs_priv *)pfs->de_private;
	win = savescr();
	dlg_init(&dlg);
	snprintf(title_buf, sizeof(title_buf),
	    "Options for UFS file system on %s:", pfs->de_partname);
	dlg_add_label(&dlg, 1, 2, 64, 2, title_buf);
	eMnt = dlg_add_edit(&dlg, 3, 2, 30, "Mountpoint:",
	    MAXVOLLEN, pfs->de_mntto);
	snprintf(buf, sizeof(buf), "%d", pfs->de_freq);
	eDumpFreq = dlg_add_edit(&dlg, 3, 34, 13, "Dump Freq:",
	    10, buf);
	snprintf(buf, sizeof(buf), "%d", pfs->de_pass);
	ePassno = dlg_add_edit(&dlg, 3, 49, 15, "Passno:",
	    10, buf);

	labels = de_dev_aliases_get(pfs->de_partname);
	assert(labels != NULL);
	for (cnt = 0; labels[cnt]; cnt++);
	/* If FS is not yet in fstab and it has a volname, we can
	 * suggest use volname by default.
	 */
	if (priv && priv->de_volname[0] != '\0') {
		if (pfs->de_mntfrom == NULL)
			ret = 1;
		else if (strcmp(pfs->de_mntfrom + sizeof(_PATH_DEV) + 3,
		    priv->de_volname) == 0)
			ret = 1;
		else
			ret = 0;
	} else
		ret = 0;
	if (cnt > 1 || ret)
		cLabel = dlg_add_checkbox(&dlg, 7, 2, 14, 1, ret, "Use Label");
	else
		dlg_add_label(&dlg, 7, 2, 16, 2, "Special device:");
	if (ret)
		snprintf(label_buf, sizeof(label_buf), "%sufs/%s", _PATH_DEV,
		    priv->de_volname);
	else if (pfs->de_mntfrom != NULL)
		snprintf(label_buf, sizeof(label_buf), "%s", pfs->de_mntfrom);
	else
		snprintf(label_buf, sizeof(label_buf), "%s%s", _PATH_DEV,
		    pfs->de_partname);
	lLabel = dlg_add_label(&dlg, 7, 18, 48, 1, label_buf);

	mntflags = 0;
	if (pfs->de_mntops != NULL)
		getmntopts(pfs->de_mntops, mopts, &mntflags, 0);
	for (i = 0; i < MNT_OPS_CNT - 1; i++) {
		cOps[i] = dlg_add_checkbox(&dlg, 8 + i % (MNT_OPS_CNT / 3),
				2 + 16 * (i / 3), 14, 1,
				(mntflags & mntopt_names[i].o_opt) != 0,
				mntopt_names[i].o_name);
	}
	cOps[i] = dlg_add_checkbox(&dlg, 8 + i % (MNT_OPS_CNT / 3),
			2 + 16 * (i / 3), 14, 1,
			(pfs->de_mntops != NULL) ?  strstr(pfs->de_mntops,
			    mntopt_names[i].o_name) != NULL: 0,
			mntopt_names[i].o_name);

	btnOk = dlg_add_button(&dlg, 12, 24, "  Ok  ");
	btnCancel = dlg_add_button(&dlg, 12, 36, "Cancel");
	use_helpline("Press F1 for help");
	dlg_autosize(&dlg, &w, &h);
	dlg_open_dialog(&dlg, w + 1, h + 1, "Select Mount Options");
	q = 0;
	do {
		ret = dlg_proc(&dlg, ufs_create_keyhndl);
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
			else
				dlg_focus_next(&dlg);
			break;
		case KEY_UP:
		case KEY_LEFT:
			dlg_focus_prev(&dlg);
			break;
		case KEY_DOWN:
		case KEY_RIGHT:
			dlg_focus_next(&dlg);
			break;
		case ' ':
			if (item->type == CHECKBOX)
				dlg_checkbox_toggle(&dlg, item);
			if (item == cLabel) {
				if (dlg_checkbox_checked(&dlg, item)) {
					WINDOW *sw;
					dlg_init(&popup);
					sw = savescr();
					dlg_open_popupmenu(&popup, dlg.y + 8,
					    dlg.x + 18, 48,
					    cnt > 5 ? 7: cnt + 1,
					    cnt - 1, (const char **)&labels[1]);
					ret = dlg_popupmenu_proc(&popup, NULL);
					if (ret == DE_CR) {
						snprintf(label_buf,
						    sizeof(label_buf),
						    "%s%s", _PATH_DEV,
						    dlg_popupmenu_get_choice(
							&popup));
						dlg_item_set_title(&dlg, lLabel,
						    label_buf);
					} else
						dlg_checkbox_toggle(&dlg, item);
					dlg_close_dialog(&popup);
					dlg_free(&popup);
					restorescr(sw);
				} else {
					snprintf(label_buf, sizeof(label_buf),
					    "%s%s", _PATH_DEV, labels[0]);
					dlg_item_set_title(&dlg, lLabel,
					    label_buf);
				}
			}
			break;
		}
	} while (q == 0);
	if (q == 2) {
		/* issue delayed command via history */
	}
	restorescr(win);
	dlg_close_dialog(&dlg);
	dlg_free(&dlg);
	de_dev_aliases_free(labels);
	return (0);
}

static int
fsed_history_rollback(void *pentry)
{
	return (0);
}

static int
fsed_history_play(void *pentry)
{
	return (0);
}

enum fsed_cmd {
	NEWFS, TUNEFS
};

static int
fsed_history_add_entry(history_t hist, enum fsed_cmd cmd, const char *title)
{
	return (0);
}

static int
fsed_fslist_reread(struct de_fslist *fslist)
{
	int error;

	de_fslist_free(fslist);
	error = de_fslist_get(fslist);

	return (error);
}

#define FSED_MENU_TOP		4
#define FSED_BOTTOM_HEIGHT	10
#define LABEL(l)	(l) ? (l): "-"

int
fsed_open(void)
{
	struct de_fslist fslist;
	struct de_fs *pfs, *selected;
	history_t hist;
	WINDOW *win;
	int count, height, row, i, key, ret;
	int sc = 0, ch = 0, q = 0;
	char *msg = NULL;
	int error = 0;
	int view_mode = 0;

	error = de_fslist_get(&fslist);
	if (error)
		return (error);
	if (TAILQ_EMPTY(&fslist)) {
		dmenu_open_errormsg("Suitable partitions are not found! "
		    "Create partitions and try again.");
		return (0);
	}
	getmnt_silent = 1;	/* make getmntopts() silent */
	hist = history_init();
	win = savescr();
	keypad(stdscr, TRUE);
	dialog_clear_norefresh(); clear();
	count = de_fslist_count(&fslist);
resize:
	if (LINES > VTY_STATUS_LINE)
		height = LINES - 1;
	else
		height = VTY_STATUS_LINE;
	height -= FSED_MENU_TOP + FSED_BOTTOM_HEIGHT;
	do {
		attrset(A_NORMAL);
		mvprintw(0, 0, "%-12s", "Device:");
		clrtobot(); attrset(A_REVERSE);
		mvprintw(0, 61, "File Systems Editor");
		attrset(A_NORMAL);
		if (view_mode)
			mvprintw(2, 0, "%20s%30s%9s%15s%3s%3s", "Device",
			    "Mountpoint", "FSType", "Options", "D#", "P#");
		else
			mvprintw(2, 0, "%12s%20s%9s%6s%33s", "Device",
			    "Part. Type", "FSType", "Size", "Mountpoint");
		row = FSED_MENU_TOP - 1;
		if (sc > 0)
			mvprintw(row, 11, "^(-)");
		else {
			move(row, 0);
			clrtoeol();
		}
		i = 0;
		TAILQ_FOREACH(pfs, &fslist, de_fs) {
			if (i++ < sc)
				continue;
			if (++row - FSED_MENU_TOP > height - 1)
				break;
			if (ch == row - FSED_MENU_TOP) {
				attrset(A_REVERSE);
				selected = pfs;
			}
			if (view_mode)
				mvprintw(row, 0, "%20s%30s%9s%15s%3d%3d",
				    LABEL(pfs->de_mntfrom),
				    LABEL(pfs->de_mntto),
				    de_fstypestr(pfs->de_type),
				    LABEL(pfs->de_mntops),
				    pfs->de_freq, pfs->de_pass);
			else
				mvprintw(row, 0, "%12s%20s%9s%6s%33s",
				    LABEL(pfs->de_partname),
				    LABEL(pfs->de_parttype),
				    de_fstypestr(pfs->de_type),
				    fmtsize(pfs->de_size),
				    LABEL(pfs->de_mntto));
			if (ch == row - FSED_MENU_TOP)
				attrset(A_NORMAL);
		}
		attrset(A_REVERSE);
		if (view_mode)
			mvprintw(0, 12, "%s (%s), %s scheme", selected->de_partname,
			    selected->de_devname, selected->de_scheme);
		else
			mvprintw(0, 12, "%s, %s scheme", selected->de_devname,
			    selected->de_scheme);
		attrset(A_NORMAL);
		if (sc + height < count)
			mvprintw(height + FSED_MENU_TOP, 11, "v(+)");
		else {
			move(height + FSED_MENU_TOP, 0);
			clrtoeol();
		}
		mvprintw(height + FSED_MENU_TOP + 1, 0,
		    "The following commands are supported:");
		mvprintw(height + FSED_MENU_TOP + 3, 0,
		    "C = Create File System    M = Mount/FS Options      F = Add to fstab");
		mvprintw(height + FSED_MENU_TOP + 4, 0,
		    "Q = Finish                U = Undo All Changes      W = Write Changes");
		mvprintw(height + FSED_MENU_TOP + 5, 0,
		    "Enter = Inspect           T = Toggle View Mode");
		mvprintw(height + FSED_MENU_TOP + 8, 0,
		    "Use F1 or ? to get more help, arrow keys to select");
		set_statusline(msg);
		if (msg)
			msg = NULL;

		key = toupper(getch());
		switch (key) {
		case '\r':
		case '\n':
			if (selected->de_type != SWAP)
				fsed_inspect(selected);
			break;
		case 'C':
			if (selected->de_type == EMPTY)
				fsed_ufs_create(selected);
			else if (!strcmp(selected->de_parttype, "freebsd-ufs")) {
				ret = dmenu_open_noyes(ask_recreate_msg);
				if (ret)
					break;
				fsed_ufs_create(selected);
			} else {
				msg = "Select one partition with type "
				    "\"freebsd-ufs\".";
			}
			break;
		case 'M':
			if (selected->de_type == UFS)
				fsed_ufs_mountops(selected);
#ifdef WITH_ZFS
			else if (selected->de_type == ZFS)
				fsed_zfs_mountops(selected);
#endif
			break;
		case KEY_ESC:
		case 'Q':
			q = 1;
			break;
		case 'W':
			if (history_isempty(hist)) {
				msg = "Nothing to save.";
				break;
			}
			if (dmenu_open_noyes(write_confirm_msg))
				break;
			error = history_play(hist, fsed_history_play);
			if (error != 0) {
				/* XXX: report about completed commands */
				history_rollback(hist, fsed_history_rollback);
			}
			error = fsed_fslist_reread(&fslist);
			break;
		case 'U':
			if (history_isempty(hist)) {
				msg = "Nothing to undo.";
				break;
			}
			if (dmenu_open_noyes(undo_msg))
				break;
			history_rollback(hist, fsed_history_rollback);
			error = fsed_fslist_reread(&fslist);
			break;
		case 'T':
			view_mode = view_mode ? 0: 1;
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
		case KEY_RESIZE:
			sc = ch = 0;
			goto resize;
		default:
			msg = "Type F1 or ? for help";
		};
	} while (q == 0);
	if (!history_isempty(hist))
		history_rollback(hist, fsed_history_rollback);
	history_free(hist);
	restorescr(win);
	de_fslist_free(&fslist);
	return (0);
}

