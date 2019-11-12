/** \file
 * Parses the output of LIST.
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

/**
 * This structure holds information for each file in the directory. Generally
 * there is an array of a lot of these.
 */
struct listparse {
	char *name;	/**< Probably not 0-terminated. */
	int namelen; /**< Since name isn't 0-terminated, gives the length. */
	long size; /**< The size of the file in question. */
	int flagisdir; /**< 1 if a directory, 0 if not. */
};

int listparse(struct listparse *lst, char *line);
