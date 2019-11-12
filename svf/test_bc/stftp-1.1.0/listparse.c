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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "listparse.h"

/**
 * This behemoth parses the output of LIST. At present it works with UNIX
 * format, and DOS/Windows format. There are probably other formats out there,
 * but I haven't seen them and thus they probably aren't in wide use. However,
 * if enough people complain I'll add it in.
 * 
 * \param lst Where we will put the file information.
 * \param line One line of output from LIST.
 * \return 1 if the entry was successfully parsed, 0 otherwise.
 */
int listparse(struct listparse *lst, char *line) {
	int len = strlen(line);
	int i,j,mon;
	i = j = mon = 0;

	lst->name = malloc(strlen(line));
	lst->namelen = 0;
	lst->size = 0;
	lst->flagisdir = 0;
	
	if(len < 2) /* Nothing is this short */
		return 0;

	switch(*line) {
		case '-':
		case 'd':
		case 'l':
			/* UNIX like. Samples:
			 * drwxrwxr-x  264 0        1003         8192 Sep 18 15:34 gnu
			 * drwxrwxr-x   14 ftp      ftp          4096 Oct 27  2005 mozilla
			 * -rw-r--r--    1 ftp      ftp       1868178 Sep 17  2003 ls-lR
			 */
			if(*line == 'd')
				lst->flagisdir = 1;

			for(i = 1;i < len;i++) {
				if(line[i] == ' ' && line[i+1] != ' ') { /* Stop just before something */
					if(j == 0 || j == 1 || j == 2) { /* inum, owner, group */
						j++;
					}
					else if(j == 3) { /* size */
						lst->size = strtol(line+i+1,NULL,10);
						j++;
					}
					else if(j == 4) { 
						/* Two cases here. Either it's a month name, or a big
							year-month-day thing. */
						if(isalpha(*(line+i+1))) { /* A month */
							mon = 1;
						}
						/* The other case is the default. */
						j++;
					}							
					else if(j == 5) { /* day or time, depending */
						j++;
					}
					else if(j == 6 && mon == 1) { /* It's the time or year. */
						j++;
					}
					else { /* The name, finally */
						strcpy(lst->name,line + i + 1);
						lst->namelen = len - i - 1;
						break; /* Sayonara */
					}
				}
			}

			/* Cut down the name in case of a link */
			if(*line == 'l')
				for(j = i+1;j + 3 < len;j++)
					if(*(line+j) == ' ')
						if(*(line+j+1) == '-')
							if(*(line+j+2) == '>')
								if(*(line+j+3) == ' ') {
									lst->namelen = j-i-1;
									break;
								}

			return 1;
	}

	if(*line >= '0' && *line <= '9') {
		/* DOS style. Samples:
		 * 10-12-06  02:45PM             14201016 setup_mw.exe
		 * 09-15-06  02:13PM       <DIR>          public
		 */
		for(i = 1; i < len; i++) {
			if(line[i] == ' ' && line[i+1] != ' ') {
				if(j == 0) /* Time */
					j++;
				else if(j == 1) { /* Size OR directory */
					if(line[i+1] == '<') {
						lst->flagisdir = 1;
					}
					else {
						lst->size = strtol(line+i+1,NULL,10);
					}
					j++;
				}
				else { /* Name */
					strcpy(lst->name,line + i + 1);
					lst->namelen = len - i - 1;
				}
			}
		}
		
		return 1;
	}

	/* Everything else is unknown to me */
	free(lst->name);

	return 0;
}
						
					
