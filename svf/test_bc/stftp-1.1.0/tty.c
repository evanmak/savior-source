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

#include <curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "misc.h"
#include "main.h"
#include "listparse.h"
#include "tty.h"

/** Finds the minimum of two numbers. */
#define MIN(a,b) ((a) < (b)) ? (a) : (b)

char l_header[81]; /**< The header. */
char l_error[81]; /**< The error line. */
char l_status[81]; /**< The status line. */
int hi_line = 0; /**< Which line shall we highlight? */

/** Simple curses initialization. */
void curses_init() {
	initscr();
	keypad(stdscr,TRUE);
	cbreak();
	noecho();

	return;
}

/** Display the window. */
void curses_display() {
	int i;

	erase();
	attron(A_REVERSE);
	for(i = 0;i<COLS;i++) /* Fill the top line */
		mvaddstr(0,i," ");

	if(l_header != NULL)
		mvaddstr(0,0,l_header);
	
	for(i = 0;i<COLS;i++) /* Fill the error line */
		mvaddstr(LINES-2,i," ");

	if(l_error != NULL)
		mvaddstr(LINES-2,0,l_error); /* Print any error we may have */
	
	attroff(A_REVERSE);
	move(LINES-1,0);

	if(l_status != NULL) /* Print any status msg we may have */
		mvaddstr(LINES-1,0,l_status); /* The status line */

	return;
}

/** Wrapper function for outside usage. */
int curses_getch() {
	return getch();
}

/**
 * Prints the directory listing starting from the head.
 *
 * \param head The beginning of the directory listing.
 */
void p_dir(struct parse_list *head) {
	struct parse_list *cur = head;
	int i = 1;
	int j = 0;
	void *size = malloc(80);
	
	loop: while(cur->next != NULL && i < LINES-2) {
		if(i == hi_line)
			attron(A_REVERSE);
		for(j = 0;j<COLS;j++) /* Fill the line */
			mvaddstr(i,j," ");
		if(cur->lp->flagisdir) {
			mvaddstr(i,0,"<DIR>");
		}
		else {
			sprintf(size,"%13ld",cur->lp->size);
			mvaddstr(i,0,size);
		}
		mvaddnstr(i,14,cur->lp->name,MIN(cur->lp->namelen,COLS-11));
		if(i == hi_line)
			attroff(A_REVERSE);
		i++;
		cur = cur->next;
	}
	if(cur->next == NULL && i < LINES - 2) {
		if(i+1 <= hi_line) { 
			hi_line = i-1;	
			cur = head;
			i = 1;
			goto loop;
		}
		else if(i == hi_line) {
			attron(A_REVERSE);
		}
		for(j = 0;j<COLS;j++) /* Fill the line */
			mvaddstr(i,j," ");
		if(cur->lp->flagisdir) {
			mvaddstr(i,0,"<DIR>");
		}
		else {
			sprintf(size,"%13ld",cur->lp->size);
			mvaddstr(i,0,size);
		}
		mvaddnstr(i,14,cur->lp->name,MIN(cur->lp->namelen,COLS-11));
		if(i == hi_line)
			attroff(A_REVERSE);
	}

	free(size);

	move(LINES-1,COLS-1);

	refresh();
}

/** Destroy the curses window. */
void curses_end() {
	clear();
	refresh();
	endwin();

	return;
}

/**
 * A "fatal" error; exit the program and print msg.
 *
 * \param msg The msg to print.
 */
void fatalerror(char *msg) {
	curses_end();
	printf("%s\n",msg);
	exit(1);
	
	return;
}

/**
 * Prompts the user with a message, and waits for newline. Then it returns the
 * input without the trailing newline. 
 * \param msg The prompt message.
 * \param input Used to store the user's input.
 * \param becho Is the input echoed or not? 
 */
void e_prompt(char *msg,char *input,int becho) { 
	int j;

	for(j = 0;j<COLS;j++)
		mvaddstr(LINES-1,j," ");
	move(LINES-1,strlen(msg));
	mvaddstr(LINES-1,0,msg);
	if(becho)	
		echo();
	getstr(input);
	noecho();
}

/** 
 * Prompts the user with a message, but only waits for one character.
 * \param msg The message to prompt.
 * \return The character the user gave us.
 */
char c_prompt(char *msg) {
	char c;
	int j;

	for(j = 0;j<COLS;j++)
		mvaddstr(LINES-1,j," ");
	if(strcmp(msg,"") == 0) { /* Blank prompt, whatever */
		move(LINES-1,0);
	}
	else { /* Not so blank */
		mvaddstr(LINES-1,0,msg);
	}
	c = getch();
	return c;
}

/* Highlighted line functions */

/** 
 * Sets the highlighted line to set. 
 *
 * \param set Which line? 
 */
void set_line(int set) {
	if(set >= 0 && set < LINES)
		hi_line = set;
}

/**
 * \return The hi_line so outside functions can know it.
 */
int get_line() {
	return hi_line;
}

/**
 * Increments the line counter.
 *
 * /return 0 normally, 1 if out of bounds. */
int inc_line() {
	hi_line++;
	/* Do fancy checking for if we are > LINES-2 */
	if(hi_line >= LINES-2) {
	//	hi_line = LINES-3;
		return 1;
	}

	return 0;
}

/** See inc_line(). */
int dec_line() {	
	hi_line--;
	if(hi_line < 1) {
		return 1;
	}
	
	return 0;
}

/**
 * A non-fatal error; simply output msg to the error line and update the
 * display.
 *
 * \param msg Our new error message.
 */
void error(char *msg) {
	strncpy(l_error,msg,80*sizeof(char));
	curses_display();
	refresh();
	return;
}

/**
 * Output a message to the status line and update display.
 *
 * \param msg Our new status message.
 */
void status(char *msg) {
	strncpy(l_status,msg,80*sizeof(char));
	curses_display();
	refresh();

	return;
}

/**
 * Output a message to the header line and update display.
 *
 * \param msg Our new header message.
 */
void header(char *msg) {
	strncpy(l_header,msg,80*sizeof(char));
	curses_display();
	refresh();

	return;
}
