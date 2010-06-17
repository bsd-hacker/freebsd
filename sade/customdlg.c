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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libutil.h>
#include <stdarg.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <dialog.h>
#include <sysexits.h>
#include <ncurses.h>
#include <assert.h>
#include "dialog.priv.h"
#include "customdlg.h"


static void dlg_item_draw(struct custom_dlg *dlg, struct dlg_item *item,
    int focused);
static void dlg_edit_draw(struct custom_dlg *dlg, DLG_EDIT *item, int focused);
static void dlg_list_draw(struct custom_dlg *dlg, DLG_LIST *item, int focused);
static void dlg_checkbox_draw(struct custom_dlg *dlg, DLG_CHECKBOX *item,
    int focused);
static void dlg_draw(struct custom_dlg *dlg);
void dlg_draw_arrows(WINDOW *win, int scroll, int height, int choice, int x,
    int y, int tag_x, int cur_x, int cur_y, chtype box, chtype border);

void
dlg_init(struct custom_dlg *dlg)
{
	bzero(dlg, sizeof(struct custom_dlg));
	TAILQ_INIT(&dlg->items);
}

void
dlg_free(struct custom_dlg *dlg)
{
	struct dlg_item *item;

	while (!TAILQ_EMPTY(&dlg->items)) {
		item = TAILQ_FIRST(&dlg->items);
		free(item->priv);
		free(item->title);
		TAILQ_REMOVE(&dlg->items, item, item);
		free(item);
	}
	free(dlg->title);
	TAILQ_INIT(&dlg->items);
}

struct dlg_item *
dlg_focus_get(struct custom_dlg *dlg)
{
	if (dlg == NULL)
		return (NULL);
	return (dlg->focus);
}

struct dlg_item *
dlg_focus_next(struct custom_dlg *dlg)
{
	struct dlg_item *item, *f = NULL;

	if (dlg == NULL)
		return (NULL);
	if (TAILQ_EMPTY(&dlg->items))
		return (NULL);
	/* Do we have any focusable items? */
	TAILQ_FOREACH(item, &dlg->items, item) {
		if (item->type != LABEL) {
			f = item;
			break;
		}
	}
	if (f == NULL)
		return (NULL);

	if (dlg->focus == NULL) {
		dlg->focus = f;
		return (f);
	} else
		item = TAILQ_NEXT(dlg->focus, item);
	do {
		if (item == NULL)
			item = TAILQ_FIRST(&dlg->items);
		else if (item->type != LABEL) {
			dlg->focus = item;
			return (item);
		} else
			item = TAILQ_NEXT(item, item);
	} while (1);
	/* NOTREACHED */
	return (item);
}

struct dlg_item *
dlg_focus_prev(struct custom_dlg *dlg)
{
	struct dlg_item *item, *f = NULL;

	if (dlg == NULL)
		return (NULL);
	if (TAILQ_EMPTY(&dlg->items))
		return (NULL);
	/* Do we have any focusable items? */
	TAILQ_FOREACH(item, &dlg->items, item) {
		if (item->type != LABEL) {
			f = item;
			break;
		}
	}
	if (f == NULL)
		return (NULL);

	if (dlg->focus == NULL) {
		dlg->focus = f;
		return (f);
	} else
		item = TAILQ_PREV(dlg->focus, dlg_items, item);
	do {
		if (item == NULL)
			item = TAILQ_LAST(&dlg->items, dlg_items);
		else if (item->type != LABEL) {
			dlg->focus = item;
			return (item);
		} else
			item = TAILQ_PREV(item, dlg_items, item);
	} while (1);
	/* NOTREACHED */
	return (item);
}

DLG_LABEL *
dlg_add_label(struct custom_dlg *dlg, int y, int x, int w, int h,
    const char *title)
{
	DLG_LABEL *item;

	item = malloc(sizeof(DLG_LABEL));
	if (item == NULL)
		return (NULL);
	bzero(item, sizeof(DLG_LABEL));
	item->type = LABEL;
	item->x = x;
	item->y = y;
	item->h = h;
	item->w = w;
	if (title)
		item->title = strdup(title);
	TAILQ_INSERT_TAIL(&dlg->items, item, item);
	return (item);
}

static void
dlg_label_draw(struct custom_dlg *dlg, DLG_LABEL *item, int focused __unused)
{
	if (item->title != NULL) {
		wattrset(dlg->win, dialog_attr);
		wmove(dlg->win, item->y, item->x);
		print_autowrap(dlg->win, item->title, dlg->h - 1, dlg->w - 2,
		    item->w, item->y, item->x, FALSE, FALSE);
	}
}

int
dlg_item_set_title(struct custom_dlg *dlg, DLG_LABEL *item,
    const char *title)
{
	char *t;
	int len;

	if (title == NULL) {
		free(item->title);
		item->title = NULL;
	} else {
		len = strlen(title);
		t = realloc(item->title, len + 1);
		if (t == NULL)
			return (ENOMEM);
		item->title = t;
		strcpy(item->title, title);
		wattrset(dlg->win, dialog_attr);
		mvwprintw(dlg->win, item->y, item->x, "%*s",
		    -item->w, item->title);
	}
	return (0);
}

DLG_EDIT *
dlg_add_edit(struct custom_dlg *dlg, int y, int x, int w, const char *title,
    int maxlen, const char *value)
{
	DLG_EDIT *item;
	struct dlg_edit *e;

	if (maxlen < 1)
		return (NULL);
	item = malloc(sizeof(DLG_EDIT));
	if (item == NULL)
		return (NULL);
	e = malloc(sizeof(*e));
	if (e == NULL) {
		free(item);
		return (NULL);
	}
	bzero(item, sizeof(DLG_EDIT));
	bzero(e, sizeof(*e));
	e->value = malloc(maxlen + 1);
	if (e->value == NULL) {
		free(item);
		free(e);
		return (NULL);
	}
	if (value)
		strncpy(e->value, value, maxlen);
	else
		bzero(e->value, maxlen + 1);
	e->len = maxlen;
	item->type = EDIT;
	item->priv = (void *)e;
	item->x = x;
	item->y = y;
	item->w = w;
	item->h = 3;
	if (title)
		item->title = strdup(title);
	if (item->title)
		item->h += 1;
	TAILQ_INSERT_TAIL(&dlg->items, item, item);
	return (item);
}

static void
dlg_edit_draw(struct custom_dlg *dlg, DLG_EDIT *item, int focused)
{
	struct dlg_edit *e = (struct dlg_edit *)item->priv;
	int y, len, scroll = 0;

	y = item->title ? item->y + 2: item->y + 1;
	len = strlen(e->value);
	wattrset(dlg->win, inputbox_attr);
	if (len > item->w - 2) {
		if (len > item->w - 2)
			scroll = len - item->w + 2;
		mvwprintw(dlg->win, y, item->x + 1, "%s",
		    e->value + scroll);
	} else
		mvwprintw(dlg->win, y, item->x + 1, "%*s",
		    -item->w + 2, e->value);
}

char *
dlg_edit_get_value(struct custom_dlg *dlg __unused, DLG_EDIT *item)
{
	struct dlg_edit *e;
	if (item->type != EDIT)
		return (NULL);
	e = (struct dlg_edit *)item->priv;
	return (e->value);
}

int
dlg_edit_set_value(struct custom_dlg *dlg, DLG_EDIT *item,
    const char *value)
{
	struct dlg_edit *e;
	assert(item != NULL);
	if (item->type != EDIT)
		return (-1);
	e = (struct dlg_edit *)item->priv;
	if (value == NULL)
		*e->value = '\0';
	else
		strncpy(e->value, value, e->len);
	dlg_edit_draw(dlg, item, dlg->focus == item);
	wrefresh(dlg->win);
	return (0);
}

static int
dlg_edit_proc(struct custom_dlg *dlg, DLG_EDIT *item, int (*key_handler)(int ))
{
	int key, y, ret = 0;
	struct dlg_edit *e = (struct dlg_edit *)item->priv;
	char tmp[e->len + 1];

	y = item->title ? item->y + 2: item->y + 1;
	do {
		strncpy(tmp, e->value, e->len);
		key = line_edit(dlg->win, y, item->x + 1, e->len, item->w - 2,
		    inputbox_attr, 1, tmp, 0);
		if (key == '\r' || key == '\n' ||key == KEY_ENTER ||
		    key == KEY_TAB || key == KEY_BTAB)
			strncpy(e->value, tmp, e->len);
		dlg_edit_draw(dlg, item, 1);
		wrefresh(dlg->win);
		switch(key) {
		case KEY_ESC:
			return (DE_ESC);
		case '\r':
		case '\n':
		case KEY_ENTER:
			return (DE_CR);
		case KEY_BTAB:
			return (DE_BTAB);
		case KEY_TAB:
			return (DE_TAB);
		default:
			if (key_handler){
				ret = key_handler(key);
				if (ret != 0)
					return (ret);
			}
			beep();
		}
	} while (1);
	/* NOTREACHED */
	return (ret);
}


DLG_BUTTON *
dlg_add_button(struct custom_dlg *dlg, int y, int x, const char *title)
{
	DLG_BUTTON *item;

	item = malloc(sizeof(DLG_BUTTON));
	if (item == NULL)
		return (NULL);
	bzero(item, sizeof(DLG_BUTTON));
	item->type = BUTTON;
	item->x = x;
	item->y = y;
	item->h = 1;
	item->w = strlen(title) + 2;
	if (title)
		item->title = strdup(title);
	TAILQ_INSERT_TAIL(&dlg->items, item, item);
	return (item);
}

static int
dlg_button_proc(struct custom_dlg *dlg, DLG_BUTTON *item,
    int (*key_handler)(int ))
{
	int key, ret = 0;

	do {
		dlg_item_draw(dlg, item, 1);
		wrefresh(dlg->win);
		key = wgetch(dlg->win);
		switch (key) {
		case KEY_F(1):
			display_helpfile();
			break;
		case KEY_ESC:
			return (DE_ESC);
		case '\n':
		case '\r':
		case ' ':
		case KEY_ENTER:
			return (DE_CR);
		case KEY_BTAB:
			return (DE_BTAB);
		case KEY_TAB:
			return (DE_TAB);
		default:
			if (key_handler) {
				ret = key_handler(key);
				if (ret != 0)
					return (ret);
			}
			beep();
		}
	} while(1);
	/* NOTREACHED */
	return (ret);
}


DLG_LIST *
dlg_add_list(struct custom_dlg *dlg, int y, int x, int w, int h,
    const char *title, int cnt, const char **items)
{
	DLG_LIST *item;
	struct dlg_list *l;

	item = malloc(sizeof(DLG_LIST));
	if (item == NULL)
		return (NULL);
	l = malloc(sizeof(*l));
	if (l == NULL) {
		free(item);
		return (NULL);
	}
	bzero(item, sizeof(DLG_LIST));
	bzero(l, sizeof(*l));
	l->items = items;
	l->cnt = cnt;
	if (l->items)
		l->selected = (char *)l->items[0];
	item->type = LIST;
	item->priv = (void *)l;
	item->x = x;
	item->y = y;
	item->w = w;
	item->h = h;
	if (title)
		item->title = strdup(title);
	TAILQ_INSERT_TAIL(&dlg->items, item, item);
	return (item);
}

static void
dlg_list_draw(struct custom_dlg *dlg, DLG_LIST *item, int focused)
{
	int i, row, h, y;
	struct dlg_list *l = (struct dlg_list *)item->priv;

	h = item->title ? item->h - 3: item->h - 2;
	y = item->title ? item->y + 2: item->y + 1;

	wattrset(dlg->win, item_attr);
	for (i = 0, row = 0; row < h && i < l->cnt; i++) {
		if (i < l->scroll)
			continue;
		if (row == l->choice) {
			l->selected = (char *)l->items[i];
			wattrset(dlg->win, focused ? item_selected_attr:
			    item_attr);
		}
		mvwprintw(dlg->win, y + row, item->x + 1, "%*s",
		    2 - item->w, l->items[i]);
		if (row == l->choice)
			wattrset(dlg->win, item_attr);
		row++;
	}
	for (; row < h; row++)
		mvwprintw(dlg->win, y + row, item->x + 1, "%*s",
		    2 - item->w, " ");
	dlg_draw_arrows(dlg->win, l->scroll, h, l->cnt, item->x,
	    y - 1, 2, item->x + 1, y + l->choice, dialog_attr, border_attr);
}

char *
dlg_list_get_choice(struct custom_dlg *dlg __unused, DLG_LIST *item)
{
	struct dlg_list *l;
	if (item->type != LIST)
		return (NULL);
	l = (struct dlg_list *)item->priv;
	return (l->selected);
}

void
dlg_list_handle_move(int key, int *choice, int *scroll, int count, int height)
{
	switch (key) {
	case KEY_UP:
		if (*choice > 0)
			*choice -= 1;
		else if (*scroll > 0)
			*scroll -= 1;
		break;
	case KEY_DOWN:
		if (*choice < count - 1) {
			if (*choice < height - 1)
				*choice += 1;
			else if (*scroll + height < count)
				*scroll += 1;
		}
		break;
	case KEY_PPAGE:
		if (*scroll > height) {
			*choice = 0;
			*scroll -= height - 1;
			if (*scroll < 0)
				*scroll = 0;
			break;
		}
	case KEY_HOME:
		*choice = *scroll = 0;
		break;
	case KEY_NPAGE:
		if (count > height) {
			*choice = height - 1;
			*scroll += height - 1;
			if (*scroll + height > count)
				*scroll = count - height;
			break;
		}
	case KEY_END:
		if (count < height)
			*choice = count - 1;
		else {
			*choice = height - 1;
			*scroll = count - height;
		}
		break;
	}
}

static int
dlg_list_proc(struct custom_dlg *dlg, DLG_LIST *item,  int (*key_handler)(int ))
{
	int key, h, ret = 0;
	struct dlg_list *l = (struct dlg_list *)item->priv;

	h = item->title ? item->h - 3: item->h - 2;
	do {
		dlg_list_draw(dlg, item, 1);
		wrefresh(dlg->win);
		key = wgetch(dlg->win);
		switch(key) {
		case KEY_UP:
		case KEY_DOWN:
		case KEY_PPAGE:
		case KEY_HOME:
		case KEY_NPAGE:
		case KEY_END:
			dlg_list_handle_move(key, &l->choice, &l->scroll,
			    l->cnt, h);
			break;
		case KEY_F(1):
			display_helpfile();
			break;
		case KEY_ESC:
			return (DE_ESC);
		case '\r':
		case '\n':
		case KEY_ENTER:
			return (DE_CR);
		case KEY_BTAB:
			return (DE_BTAB);
		case KEY_TAB:
			return (DE_TAB);
		default:
			if (key_handler){
				ret = key_handler(key);
				if (ret != 0)
					return (ret);
			}
			beep();
		}
	} while (1);
	/* NOTREACHED */
	return (ret);
}

DLG_CHECKBOX *
dlg_add_checkbox(struct custom_dlg *dlg, int y, int x, int w, int h,
    int checked, const char *title)
{
	DLG_CHECKBOX *item;

	item = malloc(sizeof(DLG_CHECKBOX));
	if (item == NULL)
		return (NULL);
	bzero(item, sizeof(DLG_CHECKBOX));
	item->type = CHECKBOX;
	item->x = x;
	item->y = y;
	item->h = h;
	item->w = w;
	if (title)
		item->title = strdup(title);
	item->data = (void *)(intptr_t)checked;
	TAILQ_INSERT_TAIL(&dlg->items, item, item);
	return (item);
}

static void
dlg_checkbox_draw(struct custom_dlg *dlg, DLG_CHECKBOX *item, int focused)
{
	wattrset(dlg->win, focused ? button_active_attr:
	    button_inactive_attr);
	mvwprintw(dlg->win, item->y, item->x, "[%s] ",
	    (int)(intptr_t)item->data ?  "X": " ");
	if (item->title != NULL) {
		print_autowrap(dlg->win, item->title, dlg->h - 1, dlg->w - 6,
		    item->w, item->y, item->x, FALSE, FALSE);
	}
}


int
dlg_checkbox_check(struct custom_dlg *dlg, DLG_CHECKBOX *item, int checked)
{
	assert(item != NULL);
	if (item->type != CHECKBOX)
		return (-1);
	item->data = (void *)(intptr_t)checked;
	dlg_edit_draw(dlg, item, dlg->focus == item);
	wrefresh(dlg->win);
	return (0);
}

int
dlg_checkbox_checked(struct custom_dlg *dlg __unused, DLG_CHECKBOX *item)
{
	assert(item != NULL);
	if (item->type != CHECKBOX)
		return (-1);
	return ((int)(intptr_t)item->data);
}

int
dlg_checkbox_toggle(struct custom_dlg *dlg __unused, DLG_CHECKBOX *item)
{
	assert(item != NULL);
	if (item->type != CHECKBOX)
		return (-1);
	item->data = (void *)(intptr_t)((int)(intptr_t)item->data ? 0: 1);
	return (0);
}

static int
dlg_checkbox_proc(struct custom_dlg *dlg, DLG_CHECKBOX *item,
    int (*key_handler)(int ))
{
	int key, ret = 0;

	do {
		dlg_checkbox_draw(dlg, item, 1);
		wrefresh(dlg->win);
		key = wgetch(dlg->win);
		switch(key) {
		case KEY_ESC:
			return (DE_ESC);
		case '\r':
		case '\n':
		case KEY_ENTER:
			return (DE_CR);
		case KEY_BTAB:
			return (DE_BTAB);
		case KEY_TAB:
			return (DE_TAB);
		default:
			if (key_handler){
				ret = key_handler(key);
				if (ret != 0)
					return (ret);
			}
			beep();
		}
	} while (1);
	/* NOTREACHED */
	return (ret);
}

void
dlg_autosize(struct custom_dlg *dlg, int *width, int *height)
{
	struct dlg_item *item;
	int w, h, max_w, max_h;

	max_w = 5; max_h = 3;
	TAILQ_FOREACH(item, &dlg->items, item) {
		w = item->w + item->x;
		h = item->h + item->y;
		if (w > max_w)
			max_w = w;
		if (h > max_h)
			max_h = h;
	}
	if (width)
		*width = max_w + 1;
	if (height)
		*height = max_h + 1;
}

static void
dlg_item_draw(struct custom_dlg *dlg, struct dlg_item *item, int focused)
{
	int h = item->h, y = item->y;

	switch (item->type) {
	case LABEL:
		dlg_label_draw(dlg, (DLG_LABEL *)item, focused);
		break;
	case EDIT:
	case LIST:
		if (item->title) {
			wattrset(dlg->win, dialog_attr);
			mvwprintw(dlg->win, item->y, item->x, "%s",
			    item->title);
			h -= 1; y += 1;
		}
		draw_box(dlg->win, y, item->x, h, item->w, border_attr,
		    dialog_attr);
		if (item->type == LIST)
			dlg_list_draw(dlg, (DLG_LIST *)item, focused);
		else
			dlg_edit_draw(dlg, (DLG_EDIT *)item, focused);
		break;
	case BUTTON:
		wattrset(dlg->win, focused ? button_active_attr:
		    button_inactive_attr);
		mvwprintw(dlg->win, item->y, item->x, "[%s]",
		    item->title ? item->title: " ");
		break;
	case CHECKBOX:
		dlg_checkbox_draw(dlg, (DLG_CHECKBOX *)item, focused);
		break;
	}
}

int
dlg_proc(struct custom_dlg *dlg, int (*key_handler)(int ))
{
	int ret = 0;
	struct dlg_item *item;

	if (dlg == NULL)
		return (-1);
	item = dlg_focus_get(dlg);
	if (item == NULL)
		item = dlg_focus_next(dlg);
	do {
		dlg_update(dlg);
		if (item == NULL)
			ret = wgetch(dlg->win);
		else {
			switch(item->type) {
			case EDIT:
				ret = dlg_edit_proc(dlg, (DLG_EDIT *)item,
				    key_handler);
				break;
			case LIST:
				ret = dlg_list_proc(dlg, (DLG_LIST *)item,
				    key_handler);
				break;
			case BUTTON:
				ret = dlg_button_proc(dlg, (DLG_BUTTON *)item,
				    key_handler);
				break;
			case CHECKBOX:
				ret = dlg_checkbox_proc(dlg,
				    (DLG_CHECKBOX *)item, key_handler);
				break;
			default:
				err(EX_SOFTWARE, "unknown dlg_item type");
			}
		}
		switch (ret) {
		default:
		case DE_CR:
		case DE_ESC:
			return (ret);
		case DE_TAB:
			item = dlg_focus_next(dlg);
			break;
		case DE_BTAB:
			item = dlg_focus_prev(dlg);
			break;
		}
	} while (item);
	return (ret);
}

void
dlg_update(struct custom_dlg *dlg)
{
	if (dlg == NULL)
		return;
	dlg_draw(dlg);
	wrefresh(dlg->win);
}

static void
dlg_draw(struct custom_dlg *dlg)
{
	struct dlg_item *item;
	draw_box(dlg->win, 0, 0, dlg->h, dlg->w, dialog_attr, border_attr);
	if (dlg->title) {
		wattrset(dlg->win, title_attr);
		mvwprintw(dlg->win, 0, (dlg->w - strlen(dlg->title)) / 2 - 1,
		    " %s ", dlg->title);
	}
	TAILQ_FOREACH(item, &dlg->items, item) {
		dlg_item_draw(dlg, item, item == dlg->focus);
	}
	display_helpline(dlg->win, dlg->h - 1, dlg->w);
}

int
dlg_open_dialog(struct custom_dlg *dlg, int width, int height,
    const char *title)
{
	WINDOW *win;
	int x, y;

	assert(dlg != NULL);
	if (width < 5 || height < 3) {
		dlg_autosize(dlg, width < 5 ? &width: NULL,
		    height < 3 ? &height: NULL);
	}

	x = DialogX ? DialogX : (COLS - width) / 2;
	y = DialogY ? DialogY : (LINES - height) / 2;

	if (use_shadow)
		draw_shadow(stdscr, y, x, height, width);
	win = newwin(height, width, y, x);
	if (win == NULL)
		return (-1);
	if (title) {
		dlg->title = strndup(title, width - 2);
		if (dlg->title == NULL) {
			delwin(win);
			return (-1);
		}
	}
	dlg->win = win;
	dlg->x = x;
	dlg->y = y;
	dlg->w = width;
	dlg->h = height;
	keypad(win, TRUE);
	return (0);
}

void
dlg_close_dialog(struct custom_dlg *dlg)
{
	if (dlg == NULL)
		return;
	if (dlg->win != NULL) {
		delwin(dlg->win);
		dlg->win = NULL;
	}
}

int
dlg_open_popupmenu(struct custom_dlg *dlg, int y, int x, int width, int height,
    int cnt, const char **items)
{
	WINDOW *win;
	DLG_LIST *item;

	assert(dlg != NULL);
	if (use_shadow)
		draw_shadow(stdscr, y, x, height, width);
	win = newwin(height, width, y, x);
	if (win == NULL)
		return (-1);
	dlg->win = win;
	dlg->x = x;
	dlg->y = y;
	dlg->w = width;
	dlg->h = height;
	keypad(win, TRUE);
	if (dlg_add_list(dlg, 0, 0, width, height, NULL, cnt, items) == NULL)
		return (-1);
	return (0);
}

int
dlg_popupmenu_proc(struct custom_dlg *dlg, int (*key_handler)(int ))
{
	int ret = 0;
	struct dlg_item *item;

	if (dlg == NULL)
		return (-1);

	item = dlg_focus_get(dlg);
	if (item == NULL)
		item = dlg_focus_next(dlg);
	assert(item != NULL);
	assert(item->type == LIST);

	draw_box(dlg->win, 0, 0, dlg->h, dlg->w, dialog_attr,
	    border_attr);
	ret = dlg_list_proc(dlg, (DLG_LIST *)item, key_handler);
	return (ret);
}

char *
dlg_popupmenu_get_choice(struct custom_dlg *dlg)
{
	DLG_LIST *item;
	if (dlg == NULL)
		return (NULL);
	item = TAILQ_FIRST(&dlg->items);
	assert(item != NULL);

	return (dlg_list_get_choice(dlg, item));
}

void
dlg_draw_arrows(WINDOW *win, int scroll, int height, int choice, int x, int y,
    int tag_x, int cur_x, int cur_y, chtype box, chtype border)
{
	wmove(win, y, x + tag_x + 1);
	wattrset(win, scroll ? uarrow_attr : box);
	waddch(win, scroll ? ACS_UARROW : ACS_HLINE);
	waddch(win, scroll ? '(' : ACS_HLINE);
	waddch(win, scroll ? '-' : ACS_HLINE);
	waddch(win, scroll ? ')' : ACS_HLINE);
	wmove(win, y + height + 1, x + tag_x + 1);

	wattrset(win, scroll + height < choice ? darrow_attr : border);
	waddch(win, scroll + height < choice ? ACS_DARROW : ACS_HLINE);
	waddch(win, scroll + height < choice ? '(' : ACS_HLINE);
	waddch(win, scroll + height < choice ? '+' : ACS_HLINE);
	waddch(win, scroll + height < choice ? ')' : ACS_HLINE);
	wmove(win, cur_y, cur_x);  /* Restore cursor position */
}

