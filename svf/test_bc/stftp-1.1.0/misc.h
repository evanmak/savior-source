/** \file
 * Miscellaneous functions.
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

/** Holds port number and host information to pass around to functions. */
struct sock_hostport { 
	struct hostent *he; /**< Contains a whole bunch of host information. */
	int port; /**< The port number. */
	char *name; /**< The literal hostname. */
};

/** Makes a linked list of the listparse stuff. */
struct parse_list {
	struct listparse *lp; /**< The current entry. */
	struct parse_list *next; /**< Pointer to the next one. */
};

void p_help();
void strip_nl(char *data);
int str_begin(char *src,char *pat);
void parse_pwd(char *pwd,char *dir);
void p_header(char *host, char *dir);
void nullify(struct parse_list *head);
void fnullify(struct parse_list *head);
struct parse_list *fscan(struct parse_list *head, char *filter);
