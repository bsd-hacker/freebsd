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
 *
 */

#ifndef _SADE_H_
#define _SADE_H_

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <dialog.h>

/* Bitfields for menu options */
#define DMENU_NORMAL_TYPE	0x1     /* Normal dialog menu           */
#define DMENU_RADIO_TYPE	0x2     /* Radio dialog menu            */
#define DMENU_CHECKLIST_TYPE	0x4     /* Multiple choice menu         */

#define VTY_STATUS_LINE		24
#define TTY_STATUS_LINE		23
#define HSCROLL_LEFT	0
#define HSCROLL_RIGHT	1

typedef struct _dmenu {
    int type;				/* What sort of menu we are	*/
    char *title;			/* Our title			*/
    char *prompt;			/* Our prompt			*/
    char *helpline;			/* Line of help at bottom	*/
    char *helpfile;			/* Help file for "F1"		*/
    dialogMenuItem *items;		/* Array of menu items		*/
} DMenu;

struct de_device;

int	dmenu_open(DMenu *menu, int *ch, int *sc, int buttons);
int	dmenu_open_main(int *ch, int *sc);
int	dmenu_open_devices(int *ch, int *sc, int (*checked)(struct _dmenu_item *),
    int (*fire)(struct _dmenu_item *));
void	dmenu_open_errormsg(char *msg);
int	dmenu_open_yesno(char *msg);
int	dmenu_open_noyes(char *msg);
int	parted_open(struct de_device *pdev, int level);
int	fsed_open(void);
WINDOW *savescr(void);
void	restorescr(WINDOW *win);
char	*hscroll_str(char *buf, int w, const char *str, int sc, int flag);
char	*fmtsize(uint64_t size);

typedef struct history *history_t;
history_t history_init(void);
void	history_free(history_t hist);
int	history_add_entry(history_t hist, void *data);
int	history_play(history_t hist, int (*play)(void *));
int	history_rollback(history_t hist, int (*rollback)(void *));
int	history_isempty(history_t hist);

#endif /* _SADE_H_ */
