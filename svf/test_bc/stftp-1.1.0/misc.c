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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "misc.h"
#include "main.h"
#include "listparse.h"
#include "tty.h"

/** Used with p_help(). */
char helpmsg[] = 
	"stftp help\n"
	"\n"
	"Note that the left and right arrow keys are synonyms for 'u' and '\\n'\n"
	"when traversing directories.\n"
	"\n"
	"ubiquitous commands\n"
	"h\tthis help message\n"
	"u\tgo up a directory (cd ..)\n"
	"f\tfilter by beginning pattern\n"
	"\n"
	"remote view commands\n"
	"g\tgoto (a directory)\n"
	"l\tchange current local directory\n"
	"L\tprint current local directory\n"
	"q\tquit\n"
	"d\tdelete the highlighted file\n"
	"\\n\tdirectory: change server to selected directory\n"
	"\tfile: download selected file to current local directory\n"
	"p\tupload a file to the current remote directory, switches to local view\n"
	"\n"
	"local view commands\n"
	"q\treturn to remote view\n"
	"l/g\tchange local directory\n"
	"\\n\tdirectory: change to selected directory\n"
	"\tfile: upload selected file to current remote directory\n"
	;

/** Prints a helpful message using the "less" pager. */
void p_help() {
	FILE *pd = popen("less -","w");
	fwrite(helpmsg,sizeof(char),strlen(helpmsg),pd);
	pclose(pd);
}

/**
 * Strips out CR and LF from the end of something.
 *
 * \param data What will be stripped.
 */
void strip_nl(char *data) {
	if(data != NULL) {
		while(data[strlen(data)-1] == '\n' || data[strlen(data)-1] == '\r') {	
			data[strlen(data)-1] = '\0';
		} 
	}
}

/**
 * Tests to see whether the string src begins with the string pat.
 *
 * \param src What to search in.
 * \param pat The pattern to match.
 * \return 1 for success, else 0.
 */
int str_begin(char *src,char *pat) {
	char *ppat = pat;
	char *psrc = src;
	while(*ppat != '\0') {
		if(*psrc++ != *ppat++) {
			return 0;
		}
	}

	return 1;
}

/**
 * Parses the output of PWD to find out what directory we're in.
 *
 * \param pwd Output of PWD.
 * \param dir The directory name will be put in this address.
 */
void parse_pwd(char *pwd,char *dir) {
	char *dirind = dir;
	char *ind;

	ind = strchr(pwd,'"');
	ind++;
	while(*ind != '"') {
		*dirind++ = *ind++;
	}
	*dirind = '\0';

	return;
}

/**
 * Easier way to print the header up top.
 * 
 * \param host Current hostname.
 * \param dir Current directory.
 */
void p_header(char *host, char *dir) {
	char *msg = malloc(80);
	sprintf(msg,"stftp %s %s",host,dir);
	header(msg);
	free(msg);
	
	return;
}


/**
 * Frees head, the lp member inside of head, and the name member inside of 
 * that. 
 *
 * \param head The container of all this soon-to-be-free stuff.
 */
void nullify(struct parse_list *head) {
	struct parse_list *cur;

	if(head != NULL) {
		if(head->next != NULL) {
			cur = head;
			while(cur->next != NULL) {
				head = head->next;
				free(cur->lp->name);
				free(cur->lp);
				free(cur);
				cur = head;
			}
		}
		
		if(head->lp != NULL) {
			free(head->lp->name);
			free(head->lp);
		}

		free(head);
	}
}

/**
 * Simliar to nullify, except doesn't free the lp member. For filtered
 * lists.
 *
 * \param head The container of all this soon-to-be-free stuff.
 */
void fnullify(struct parse_list *head) {
	struct parse_list *cur;

	if(head != NULL) {
		if(head->next != NULL) {
			cur = head;
			while(cur->next != NULL) {
				head = head->next;
				free(cur);
				cur = head;
			}
		}
		
		free(head);
	}
}


/**
 * Used with the filtering. This builds a linked list of all the filenames
 * that match the given filter.
 *
 * \param head The head of the entire directory listing.
 * \param filter The given name filter.
 * \return Pointer to the head of the filtered list.
 */
struct parse_list *fscan(struct parse_list *head, char *filter) {
	struct parse_list *fhead; /* We're returning this */
	struct parse_list *ftemp; /* Placeholder */
	struct parse_list *cur; /* Another one */

	if(head != NULL) { /* If this is true there's no point */
		fhead = malloc(sizeof(struct parse_list));
		cur = fhead; /* A marker */
		ftemp = head;
		/* Find the head of the filtered list */
		while(ftemp != NULL && !str_begin(ftemp->lp->name,filter)) {
			ftemp = ftemp->next;
		}
		if(ftemp == NULL) { /* Nothing was found */
			free(fhead);
			fhead = NULL;
			cur = NULL;
		}
		else { /* This is a 'priming read' */
			fhead->lp = ftemp->lp;
                        ftemp = ftemp->next;
			while(ftemp != NULL && !str_begin(ftemp->lp->name,filter)) {
				ftemp = ftemp->next;
			}
			while(ftemp != NULL) { /* Go until we run out */
				/* The name etc. will be the same */
				fhead->next = malloc(sizeof(struct parse_list));
				fhead = fhead->next;
				fhead->lp = ftemp->lp;
				ftemp = ftemp->next;
				/* Find the next one */
				while(ftemp != NULL && !str_begin(ftemp->lp->name,filter)) {
					ftemp = ftemp->next;
				}
			}
			/* Now we have run out. */
			fhead->next = NULL;
			fhead = cur; /* Back at the beginning */
		}
	}
	else {
		return NULL;
	}

	return fhead;
}


