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

#ifndef _CUSTOMDLG_H_
#define _CUSTOMDLG_H_
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <dialog.h>

enum dlg_item_type {
	LABEL, EDIT, BUTTON, LIST, CHECKBOX
};

TAILQ_HEAD(dlg_items, dlg_item);
struct custom_dlg {
	WINDOW			*win;
	struct dlg_items	items;
	struct dlg_item		*focus;
	char			*title;
	int			x, y, w, h;
};

struct dlg_item {
	enum dlg_item_type	type;
	TAILQ_ENTRY(dlg_item)	item;
	char			*title;
	int			x, y, w, h;
	void			*data;
	void			*priv;
};

struct dlg_edit {
	char			*value;
	int			len, scroll;
};

struct dlg_list {
	const char		**items;
	char			*selected;
	int			cnt, scroll, choice;
};

#define DLG_LABEL	struct dlg_item
#define DLG_EDIT	struct dlg_item
#define DLG_LIST	struct dlg_item
#define DLG_BUTTON	struct dlg_item
#define DLG_CHECKBOX	struct dlg_item

#define KEY_TAB		9
#define KEY_ESC		27

#define DLG_EVENT_BASE	1000
#define DE_CR		(DLG_EVENT_BASE + 1)
#define DE_ESC		(DLG_EVENT_BASE + 2)
#define DE_TAB		(DLG_EVENT_BASE + 3)
#define DE_BTAB		(DLG_EVENT_BASE + 4)

#define DLG_METHOD(name, ...)	\
    dlg_##name(struct custom_dlg *dlg, ## __VA_ARGS__)

int	dlg_open_dialog(struct custom_dlg *dlg, int width, int height,
    const char *title);
void	dlg_close_dialog(struct custom_dlg *dlg);

int	dlg_open_popupmenu(struct custom_dlg *dlg, int y, int x, int width,
    int height, int cnt, const char **items);
int	dlg_popupmenu_proc(struct custom_dlg *dlg, int (*key_handler)(int ));
char	*dlg_popupmenu_get_choice(struct custom_dlg *dlg);

DLG_LABEL *dlg_add_label(struct custom_dlg *dlg, int y, int x, int w, int h,
    const char *title);
int dlg_item_set_title(struct custom_dlg *dlg, DLG_LABEL *item,
    const char *title);

DLG_EDIT *dlg_add_edit(struct custom_dlg *dlg, int y, int x, int w,
    const char *title, int maxlen, const char *value);
char *dlg_edit_get_value(struct custom_dlg *dlg __unused, DLG_EDIT *item);
int dlg_edit_set_value(struct custom_dlg *dlg, DLG_EDIT *item,
    const char *value);

DLG_LIST *dlg_add_list(struct custom_dlg *dlg, int y, int x, int w, int h,
    const char *title, int cnt, const char **items);
char *dlg_list_get_choice(struct custom_dlg *dlg __unused, DLG_LIST *item);
void dlg_list_handle_move(int key, int *choice, int *scroll, int count,
    int height);

DLG_BUTTON *dlg_add_button(struct custom_dlg *dlg, int y, int x,
    const char *title);

DLG_CHECKBOX *dlg_add_checkbox(struct custom_dlg *dlg, int y, int x, int w,
    int h, int checked, const char *title);
int	dlg_checkbox_check(struct custom_dlg *dlg, DLG_CHECKBOX *item,
    int checked);
int	dlg_checkbox_checked(struct custom_dlg *dlg __unused, DLG_CHECKBOX *item);
int	dlg_checkbox_toggle(struct custom_dlg *dlg __unused, DLG_CHECKBOX *item);

void dlg_init(struct custom_dlg *dlg);
void dlg_free(struct custom_dlg *dlg);
void dlg_update(struct custom_dlg *dlg);
/* key_handler is a function which helps to handle custom keys.
 * It should return key code if you want to handle it, otherwise
 * it should return zero.
 */
int dlg_proc(struct custom_dlg *dlg, int (*key_handler)(int ));
struct dlg_item *dlg_focus_get(struct custom_dlg *dlg);
struct dlg_item *dlg_focus_next(struct custom_dlg *dlg);
struct dlg_item *dlg_focus_prev(struct custom_dlg *dlg);
void dlg_autosize(struct custom_dlg *dlg, int *width, int *height);

#endif /* _CUSTOMDLG_H_ */
