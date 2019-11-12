/** \file
 * Terminal handling (via Curses).
 */

/* Copyright 2006, 2007 Andre Beausoleil.
 *
 * This file is part of stftp, a "simple" terminal FTP client.
 * The author can be reached at <abeausoleil@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/** Macro for ease of use. Like e_prompt() but always echoes. */
#define prompt(a,b) e_prompt((a),(b),0);

void curses_init();
void curses_display();
int curses_getch();
void curses_end();
void fatalerror(char *msg);
void e_prompt(char *msg, char *input,int becho);
void p_dir(struct parse_list *head);
char c_prompt(char *msg);
void error(char *msg);
void status(char *msg);
void header(char *msg);
void set_line(int set);
int get_line();
int inc_line();
int dec_line();
