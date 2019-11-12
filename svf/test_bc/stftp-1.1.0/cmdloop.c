/** \file
 * The main I/O loop.
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
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "misc.h"
#include "main.h"
#include "listparse.h"
#include "ftp.h"
#include "tty.h"
#include "cmdloop.h"

/**
 * Gets the ith element of the given parse_list and returns it.
 *
 * \param head The first directory entry.
 * \param i The index of the element we want.
 * \return The directory entry we want.
 */
struct parse_list *get_element(struct parse_list *head, int i) {
	struct parse_list *cur = head;
	int j = 1;
	for(j = 1;cur->next != NULL;j++) {
		if(j == i) {
			return cur;
		}
		cur = cur->next;
	}

	return cur;
}

/**
 * The main I/O loop; this is where the program spends a majority of its
 * time. Takes in commands from the user and then does something based on
 * that. The function loops infinitely until a quit command is received
 * from the user.
 * 
 * \param sockfd The socket descriptor for the FTP server.
 * \param info The sock_hostport structure with the FTP server info.
 * \param dir What directory are we in on the FTP server?
 * \param head The beginning of the linked list of files on the server.
 * \return 0 if a quit is received.
 */
int cmd_loop(int sockfd,struct sock_hostport info,char *dir,
			 struct parse_list *head) {
	int cmd; /* Our command */
	char *output; /* Pointer to output from our friend the server */
	int top_pos = 1; /* Top of the screen */
	struct parse_list *cur = head; /* Currently selected */
	struct parse_list *fhead; /* Beginning of filtered list */
	char filter[1024] = "";
	int pos = 1; /* Absolute position in the list, (pos)th element */

	fhead = fscan(head,filter);
	cur = fhead;

	while(1) { /* Loop forever */
		if(fhead != NULL) {
			p_dir(get_element(fhead,top_pos));		
		}
		cmd = curses_getch();

		/* The great command tester */
		switch(cmd) {
			case 'q':
				fnullify(fhead);
				nullify(head);
				return 0;
				break;
			case 'l': /* Change local download directory */
				output = malloc(1024);
				e_prompt("Local dir: ",output,1);
				if(chdir(output) < 0) {
					error("Could not change directory");
				}
				else {
					status("Changed local dir successfully.");
				}			
				free(output);
				break;
			case 'L': /* Print current local directory */
				output = malloc(1024);
				getcwd(output,1024);
				status(output);
				free(output);
				break;
			case 'h':
				/* Print a help message */
				curses_end();
				p_help();
				curses_init();
				curses_display();
				break;
			case KEY_DOWN:
				error("");
				if(cur != NULL && cur->next != NULL) {
					cur = cur->next;
					pos++;
					if(inc_line()) {
						top_pos += 5; 
						set_line(get_line() - 5);
					}
				}
				break;
			case KEY_UP:
				error("");
				pos--;
				if(pos >= 1) {
					if(dec_line()) {
						if(pos > 5) {
							top_pos -= 5;
							set_line(get_line() + 5);
						}
						else {
							top_pos = 1;
							set_line(pos);
						}
					}
					cur = get_element(fhead,pos);
				}
				else {
					pos = 1;
				}
				break;
			case 'd': /* Delete a file */
				output = calloc(cur->lp->namelen+1,sizeof(char));
				char a = c_prompt("Are you sure you want to delete this file? (y/n)");
				if(a == 'y' || a == 'Y') { /* Point of no return */
					strncpy(output,cur->lp->name,cur->lp->namelen);
					ftp_write(sockfd,"DELE ");
					ftp_write(sockfd,output);
					free(output);
					output = send_cmd(sockfd,"\r\n");
					strip_nl(output);
					if(str_begin(output,"2")) { /* Success. Redisplay the screen */
						fnullify(fhead);
						nullify(head);
						head = get_list(sockfd,info);
						top_pos = 1;
						cur = head;
						fhead = fscan(head,filter);
						set_line(1);
						pos = 1;
					}
					free(output);
				}
				else {
					status("");
					free(output);
				}
				break;
			case 'f': /* Filter out stuff */
				output = malloc(1024);
				e_prompt("Filter: ",output,1);
				strcpy(filter,output);
				strcpy(output,"Now using the filter `");
				strcat(output,filter);
				strcat(output,"'.");
				error(output);
				free(output);
				fnullify(fhead);
				fhead = fscan(head,filter);
				cur = fhead;
				top_pos = 1;
				set_line(1);
				pos = 1;				
				break;
			case 'g': /* Go to a specific directory */
				output = malloc(1024);
				e_prompt("Directory: ",output,1);
				if(ftp_cwd(sockfd,info,output)) {
					output = send_cmd(sockfd,"PWD\n"); /* Where are we?! */
					if(str_begin(output,"257 ")) { /* :) */
						parse_pwd(output,dir);
						p_header(info.name,dir);
						free(output);
					}
					else { /* ?_? */
						fatalerror(output);
					}

					nullify(head); /* Frees the directory */
					head = get_list(sockfd,info);
					top_pos = 1;
					cur = head;
					set_line(1);
					pos = 1;
				}
				break;
			case 'u': /* Move up a directory level */
			case KEY_LEFT: /* A good synonym */
				output = send_cmd(sockfd,"CDUP\r\n");
				if(str_begin(output,"2")) {
					free(output);
					output = send_cmd(sockfd,"PWD\n"); /* Where are we?! */
					if(str_begin(output,"257 ")) { /* :) */
						parse_pwd(output,dir);
						p_header(info.name,dir);
						free(output);
					}
					else { /* ?_? */
						fatalerror(output);
					}
					
					fnullify(fhead);
					nullify(head);
					head = get_list(sockfd,info);
					fhead = fscan(head,filter);
					top_pos = 1;
					cur = fhead;
					set_line(1);
					pos = 1;
				}
				break;
			case 'p': /* P is for Put */
				output = local_loop();
				/* Clean up from the loop */
				set_line(pos);
				if(fhead != NULL)
					p_dir(get_element(fhead,top_pos));
				p_header(info.name,dir);
				if(output == NULL) { /* The user aborted. */
					break;
				}
				FILE *fd = fopen(output,"r");
				if(fd == NULL) {
					error("Unable to open local file for reading");
					free(output);
					break;
				}
				FILE *f_size = fopen(output,"r");
				fseek(f_size,0,SEEK_END);
				long int size = ftell(f_size);
				fclose(f_size);
				int ok = ftp_stor(sockfd,info,fd,output,size);
				if(ok) {
					/* OK, we sent it, now update the directory listing */
					fnullify(fhead);
					nullify(head);
					head = get_list(sockfd,info);
					fhead = fscan(head,filter);
					top_pos = 1;
					cur = fhead;
					set_line(1);
					pos = 1;
				}
				free(output);
				break;
			case '\n':
			case KEY_RIGHT:
				if(cur != NULL) {
					/* Is it a directory? */
					if(cur->lp->flagisdir) {
						if(ftp_cwd(sockfd,info,cur->lp->name)) {
							output = send_cmd(sockfd,"PWD\n"); /* Where are we?! */
							pwdtest: if(str_begin(output,"257 ")) { /* :) */
								parse_pwd(output,dir);
								p_header(info.name,dir);
								free(output);
							}
							else if(str_begin(output,"250")) {
								/* Using 250 as a mark; why, I don't know */
								free(output);
								output = ftp_read(sockfd);
								goto pwdtest; /* Fast solution! */
							}
							else { /* ?_? */
								fatalerror(output);
							}

							fnullify(fhead);
							nullify(head);
							head = get_list(sockfd,info);
							top_pos = 1;
							fhead = fscan(head,filter);
							cur = fhead;
							set_line(1);
							pos = 1;
						}
					}
					/* How about a regular ol' file? */
					else {
						/* Overwrite check */
						output = calloc(cur->lp->namelen+1,sizeof(char));
						strncpy(output,cur->lp->name,cur->lp->namelen);
						FILE *fp = fopen(output,"r");
						if(fp != NULL) { /* Overwrite!!! */
							fclose(fp);
							char a = c_prompt("This file already exists. Overwrite? (y/n)");
							if(a == 'y' || a == 'Y') 
								ftp_retr(sockfd,info,cur->lp);
							else
								status("");
						}
						else { /* No problem. */
							ftp_retr(sockfd,info,cur->lp);
						}
						free(output);
					}
				}
				break;
			default:
				error("!Invalid command");
				status("");
				break;
		}
	}

	return 0;
}

/** 
 * This function is for when you are selecting a local file. It displays
 * the files just as it does for the remote view. 
 * \return The name of the file to send.
 */
char *local_loop() {
	int cmd; /* The command */
	char dir[1024]; /* Whatever directory we may be in */
	getcwd(dir,1024);
	p_header("*LOCAL*",dir);
	char filter[1024] = ""; /* Current filter */
	char *output; /* Output from functions or something */
	int top_pos = 1; /* Top of the screen */
	struct parse_list *cur; /* Current selected */
	struct parse_list *head; /* Beginning of the linked list of files */
	struct parse_list *fhead; /* Beginning of filtered list */
	int pos = 1; /* Abs. position in list */

	FILE *pls;
	output = malloc(4096);

	pls = popen("ls -l","r");

	if(pls == NULL) {
		error("Could not open directory listing");
		free(output);
		return NULL;
	}

	fgets(output,4096,pls);
	char *help = malloc(4096);
	while(fgets(help,4096,pls) != NULL) {
		output = realloc(output,strlen(output)+4096);
		strcat(output,help);
	}
	free(help);
	pclose(pls);

	strip_nl(output);

	head = list_parse(output);
	if(head != NULL && head->lp == NULL) { /* Empty directory */
		free(head);
		head = NULL;
	}

	cur = head;


	fhead = fscan(head,filter);
	cur = fhead;

	set_line(pos);
	status("Choose a file to send.");

	while(1) { /* Loop forever */
		if(fhead != NULL) {
			p_dir(get_element(fhead,top_pos));
		}
		cmd = curses_getch();

		switch(cmd) {
			case 'q':
				fnullify(fhead);
				nullify(head);
				error("");
				status("Aborted put command.");
				return NULL;
				break;
			case 'l': /* Change local directory */
			case 'g': /* Making this a synonym */
			case 'u': /* Very, very similar */
			case KEY_LEFT: /* Cripes this is a lot of synonyms */
				if(cmd != 'u' && cmd != KEY_LEFT) {
					output = malloc(1024);
					e_prompt("Local dir: ",output,1);
					if(chdir(output) < 0) {
						error("Could not change directory");
					}
					else {
						status("Changed local dir successfully.");
					}
					free(output);
				}
				else {
					chdir("..");
				}
				dirtest: getcwd(dir,1024);
				p_header("*LOCAL*",dir);
				output = malloc(4096);
				pls = popen("ls -l","r");
				if(pls == NULL) {
					error("Could not open directory listing");
					free(output);
					return NULL;
				}
				fgets(output,4096,pls);
				help = malloc(4096);
				while(fgets(help,4096,pls) != NULL) {
					output = realloc(output,strlen(output)+4096);
					strcat(output,help);
				}
				free(help);
				pclose(pls);

				strip_nl(output);
				
				fnullify(fhead);
				nullify(head);
				strcpy(filter,"");

				head = list_parse(output);
				if(head != NULL && head->lp == NULL) {
					free(head);
					head = NULL;
				}

				cur = head;
				top_pos = 1;
				set_line(1);
				pos = 1;
				fhead = fscan(head,filter);
				cur = fhead;
				break;
			case 'f': /* Filter out stuff */
				output = malloc(1024);
				e_prompt("Filter: ",output,1);
				strcpy(filter,output);
				strcpy(output,"Now using the filter `");
				strcat(output,filter);
				strcat(output,"'.");
				error(output);
				free(output);
				fnullify(fhead);
				fhead = fscan(head,filter);
				cur = fhead;
				top_pos = 1;
				set_line(1);
				pos = 1;				
				break;
			case 'h': /* "Inherited" from cmd_loop */
				curses_end();
				p_help();
				curses_init();
				curses_display();
				break;
			case KEY_DOWN:
				error("");
				if(cur != NULL && cur->next != NULL) {
					cur = cur->next;
					pos++;
					if(inc_line()) {
						top_pos += 5;
						set_line(get_line() - 5);
					}
				}
				break;
			case KEY_UP:
				error("");
				pos--;
				if(pos >= 1) {
					if(dec_line()) {
						if(pos > 5) {
							top_pos -= 5;
							set_line(get_line() + 5);
						}
						else {
							top_pos = 1;
							set_line(pos);
						}
					}
					cur = get_element(fhead,pos);
				}
				else {
					pos = 1;
				}
				break;
			case '\n': /* Selecting the file */
			case KEY_RIGHT:
				if(cur != NULL) {
					if(cur->lp->flagisdir) {
						getcwd(dir,1024);
						help = malloc(1024+cur->lp->namelen+1);
						strcpy(help,dir);
						strcat(help,"/");
						strncat(help,cur->lp->name,cur->lp->namelen);
						chdir(help);
						free(help);
						goto dirtest;
					}
					else {
						/* Someday put an overwrite check? */
						char *name = calloc(cur->lp->namelen+1,sizeof(char));
						strncpy(name,cur->lp->name,cur->lp->namelen);
						fnullify(fhead);
						nullify(head);
						return name;
					}
				}
				break;
			default:
				error("!Invalid command");
				status("");
				break;	
		}
	}

	return NULL;
}
