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
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/consio.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <libutil.h>
#include <stdarg.h>
#include <fcntl.h>
#include <err.h>
#include <dialog.h>
#include <ctype.h>
#include <sysexits.h>
#include <locale.h>
#include <assert.h>
#include <sade.h>
#include <libsade.h>
#include "customdlg.h"

#define MAX_MENU		14
static DMenu devices_menu = {
	.type = DMENU_NORMAL_TYPE,
	.title = "Select Device",
	.prompt =
	    "Please select the device on which you wish to perform an operation.\n\n"
	    "Use the arrow keys to move and press [ENTER] to select.\n"
	    "Use [TAB] to get to the buttons and leave this menu.",
	.helpline = "Press F1 for help"
};

char *
fmtsize(uint64_t size)
{
	static char buf[5];
	humanize_number(buf, sizeof(buf), size, "", HN_AUTOSCALE,
	    HN_B | HN_NOSPACE | HN_DECIMAL);
	return ((char *)buf);
}


char *
hscroll_str(char *buf, int w, const char *str, int sc, int flag)
{
	int len, both;

	assert(buf != NULL);
	assert(sc >= 0);
	assert(w > 2);

	len = strlen(str);
	if (len <= w) {
		strncpy(buf, str, w);
		return (buf);
	}
	if (sc + w > len)
		sc = len - w;
	both = (sc > 0 && len > w + sc) ? 1: 0;
	if (flag == HSCROLL_LEFT) {
		strncpy(buf, str + len - w - sc, w);
		if (both || sc == 0)
			buf[0] = '<';
		if (sc > 0)
			buf[w - 1] = '.';
	} else {
		strncpy(buf, str + sc, w);
		if (both || sc == 0)
			buf[w - 1] = '>';
		if (sc > 0)
			buf[0] = '.';
	}
	buf[w] = 0;
	return (buf);
}

static int
dmenu_height(DMenu *menu, int n)
{
	int max;

	max = MAX_MENU;
	if (LINES > VTY_STATUS_LINE)
		max += LINES - VTY_STATUS_LINE;
	max -= strheight(menu->prompt);
	return (n > max ? max: n);
}

WINDOW *
savescr(void)
{
	WINDOW *win;

	win = dupwin(newscr);
	return (win);
}

void
restorescr(WINDOW *win)
{
	touchwin(win);
	wrefresh(win);
	delwin(win);
}

void
dmenu_open_errormsg(char *msg)
{
	char buf[80];
	int width;
	WINDOW *win = savescr();
	snprintf(buf, sizeof(buf), "An error occured: %s", msg);
	width = (strlen(buf) + 5) % 70;
	dialog_msgbox("Error", buf, 5, width, 1);
	restorescr(win);
}

int
dmenu_open_yesno(char *msg)
{
	int ret;
	WINDOW *win = savescr();
	ret = dialog_yesno("User Confirmation Request", msg, -1, -1);
	restorescr(win);

	return (ret);
}

int
dmenu_open_noyes(char *msg)
{
	int ret;
	WINDOW *win = savescr();
	ret = dialog_noyes("User Confirmation Request", msg, -1, -1);
	restorescr(win);

	return (ret);
}

int
dmenu_open(DMenu *menu, int *ch, int *sc, int buttons)
{
	int n, rval = 0;
	dialogMenuItem *items;
	WINDOW *win;

	items = menu->items;
	if (buttons)
		items += 2;
	/* Count up all the items */
	for (n = 0; items[n].title; n++);

	win = savescr();
	/* Any helpful hints, put 'em up! */
	use_helpline(menu->helpline);
#if 0
	use_helpfile(systemHelpFile(menu->helpfile, buf));
#endif
	dialog_clear_norefresh();
	/* Pop up that dialog! */
	if (menu->type & DMENU_NORMAL_TYPE)
		rval = dialog_menu((u_char *)menu->title,
		    (u_char *)menu->prompt, -1, -1, dmenu_height(menu, n), -n,
		    items, (char *)(uintptr_t)buttons, ch, sc);

	else if (menu->type & DMENU_RADIO_TYPE)
		rval = dialog_radiolist((u_char *)menu->title,
		    (u_char *)menu->prompt, -1, -1, dmenu_height(menu, n), -n,
		    items, (char *)(uintptr_t)buttons);

	else if (menu->type & DMENU_CHECKLIST_TYPE)
			rval = dialog_checklist((u_char *)menu->title,
			    (u_char *)menu->prompt, -1, -1, dmenu_height(menu, n),
			    -n, items, (char *)(uintptr_t)buttons);
	else
		err(EX_SOFTWARE, "Menu: `%s' is of an unknown type\n",
		    menu->title);

	restorescr(win);
	return (rval);
}

static int
check_device(dialogMenuItem *pitem)
{
	return ((int)pitem->aux);
}

static int
select_device(dialogMenuItem *pitem)
{
	pitem->aux = pitem->aux ? 0: 1;
	return (DITEM_SUCCESS);
}

static int
open_device(dialogMenuItem *pitem)
{
	int ret;
	struct de_device *pdev = pitem->data;
	ret = parted_open(pdev, 0);
	return (DITEM_SUCCESS);
}

int
dmenu_open_devices(int *ch, int *sc, int (*checked)(struct _dmenu_item *),
    int (*fire)(struct _dmenu_item *))
{
	struct de_devlist devlist;
	struct de_device *pdev;
	dialogMenuItem *pitem;
	int ret, count;

	ret = de_devlist_get(&devlist);
	if (ret)
		return (ret);
	count = de_devlist_count(&devlist);
	devices_menu.items = malloc(sizeof(dialogMenuItem) * (count + 1));
	pitem = devices_menu.items;
	if (pitem != NULL) {
		devices_menu.type = checked ? DMENU_CHECKLIST_TYPE:
		    DMENU_NORMAL_TYPE;
		bzero(pitem, sizeof(dialogMenuItem) * (count + 1));
		TAILQ_FOREACH(pdev, &devlist, de_device) {
			pitem->prompt = pdev->de_name;
			pitem->title = pdev->de_desc;
			pitem->fire = fire;
			pitem->checked = checked;
			pitem->data = pdev;
			pitem++;
		}
		do {
			ret = dmenu_open(&devices_menu, ch, sc, 0);
		} while (ret == 0);
		free(devices_menu.items);
	} else
		ret = ENOMEM;
	de_devlist_free(&devlist);
	return (ret);
}

static int
open_raid(dialogMenuItem *pitem)
{
	int ch, sc;
	ch = sc = 0;
	dmenu_open_devices(&ch, &sc, check_device, select_device);
	return (DITEM_SUCCESS);
}

static int
open_parts(dialogMenuItem *pitem)
{
	int ch, sc;
	ch = sc = 0;
	dmenu_open_devices(&ch, &sc, NULL, open_device);
	return (DITEM_SUCCESS);
}

static int
open_fs(dialogMenuItem *pitem)
{
	fsed_open();
	return (DITEM_SUCCESS);
}

static DMenu main_menu = {
	.type = DMENU_NORMAL_TYPE,
	.title = "Main Menu",
	.prompt = "This is a utility for partitioning and managing your disks.",
	.helpline = "SysAdmin Disk Editor"
};

static dialogMenuItem main_menu_items[] = {
	{ "1 RAID", "Managing softare RAID configurations", NULL, open_raid },
	{ "2 Partitions", "Managing disk partitions", NULL, open_parts },
	{ "3 File Systems", "Managing file systems", NULL, open_fs },
	{ NULL },
};

int
dmenu_open_main(int *ch, int *sc)
{
	int ret;
	main_menu.items = &main_menu_items[0];
	do {
		ret = dmenu_open(&main_menu, ch, sc, 0);
	} while (ret == 0);
	return (ret);
}
