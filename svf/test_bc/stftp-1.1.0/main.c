/** \file 
 * The main function.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>

#include "misc.h"
#include "main.h"
#include "ftp.h"
#include "tty.h"
#include "listparse.h"
#include "cmdloop.h"

/** Used in stftp -V and other such places. */
#define STFTP_VERSION "1.1.0"

/**
 * Breaks the output of LIST into (hopefully) distinct entries and returns
 * an amalgamated list of all the entires.
 *
 * \param output What LIST has given us.
 * \return A parse_list with all the files from the directory.
 */
struct parse_list *list_parse(char *output) {
	char *o_ptr = output;
	char *i_ptr = o_ptr;
	struct parse_list *list = malloc(sizeof(struct parse_list));
	struct parse_list *head = list; /* To be returned */
	list->next = NULL;
	struct listparse *lp = malloc(sizeof(struct listparse));
	int i;

	if(output == NULL) { /* Generally received when read permission is denied */
		return NULL;
	}

	int l = strlen(output)+1;
	
	for(i=0;i<l;i++) {
		if(*o_ptr == '\n') { /* New line */
			*o_ptr = '\0';
			o_ptr++;
			if(listparse(lp,i_ptr)) {
				list->lp = lp;
				lp = malloc(sizeof(struct listparse));
				list->next = malloc(sizeof(struct parse_list));
				list = list->next;
				list->next = NULL;
			}
			i_ptr = o_ptr; /* Reset our "index" variable */
		}
		else if(*o_ptr != '\r' && *o_ptr != '\0') {
			o_ptr++;
		}
		else if(*o_ptr == '\r') {
			*o_ptr = '\0';
			if(*(o_ptr+1) == '\n')
				o_ptr++;
			o_ptr++;
			if(listparse(lp,i_ptr)) {
				list->lp = lp;
				lp = malloc(sizeof(struct listparse));
				list->next = malloc(sizeof(struct parse_list));
				list = list->next;
				list->next = NULL;
			}
			i_ptr = o_ptr;
		}
		else { /* We are at the end of the output. */
			if(listparse(lp,i_ptr)) { /* We can read the last line! */
				list->lp = lp;
				list->next = NULL;
			}
			else { /* We can't so let's just ignore it. */
				if(list == head) { /* Nothing was done!! */
					free(list);
					return NULL;
				}
				free(list);
			}
			break;
		}
	}

	free(output);

	return head;
}

/** 
 * Print the usage message and die.
 * 
 * \param progname The name of the program as it was called. 
 */
void print_usage(char *progname) {
	char msg[256];

	sprintf(msg,"Usage:\t%s [-h|-V]\n"
				"\t%s [-p port] [-u username] hostname\n\n"
				"For help once the program is run, type \"h\" for a command list.", 	progname,progname);
	fatalerror(msg);

	return;
}

/** Print the version number and such and die. */
void print_version() {
	char version[] = "stftp " STFTP_VERSION "\n"
		"Copyright (C) 2006, 2007 Andre Beausoleil\n"
		"\nThis is free software; see the file `COPYING` in the source distribution"
 		"\nfor copying conditions. There is NO warranty; not even for MERCHANTABILITY" 			"\nor FITNESS FOR A PARTICULAR PURPOSE.";
	fatalerror(version);

	return;
}

/**
 * Gets the directory listing of the current directory and returns it.
 * 
 * \param sockfd FTP socket descriptor.
 * \param info Host and port information.
 * \return A parse_list with all the directory entries.
 */
struct parse_list *get_list(int sockfd,struct sock_hostport info) {
	char *output = send_cmd(sockfd,"PASV\n");
	struct parse_list *head;
	int pasvfd;

	if(str_begin(output,"227 ")) {
		pasvfd = pasv_connect(output,info); /* Connect to the PASV port */
		free(output);
		ftp_write(sockfd,"LIST\r\n");
		output = ftp_read(sockfd);
		strip_nl(output);
		if(str_begin(output,"1")) { /* We will be receiving the listing! */
			free(output);
			output = ftp_read(pasvfd);
			char *help;
			while((help = ftp_read(pasvfd)) != NULL) {
				output = realloc(output,strlen(output)+4096);
				strcat(output,help);
				free(help);
			}
			strip_nl(output);
			close(pasvfd);
			/* Parse the directory listing, make a linked list of this, 
			 * and return it. */
			head = list_parse(output); 
			if(head != NULL && head->lp == NULL) { /* The directory is in fact empty */
				free(head);
				head = NULL;
			}
			output = ftp_read(sockfd); /* This probably says 226 */
			strip_nl(output);
			status(output);
			free(output);
		}
		else { /* I really should have some handling for this */
			fatalerror(output);
		}
	}
	else {
		fatalerror(output);
	}

	return head;
}

/** 
 * The main function. Probably does more than it should at this point.
 * It mainly does initializing things, like ask for the username & pass, and
 * connecting to the server, getting the first directory, etc. Near the end
 * of the function it gives up control to cmd_loop(). Then, once that returns,
 * it quits.
 */
int main(int argc, char *argv[]) {
	int sockfd = -1; /*	Our main socket */
	struct sock_hostport sock_info; /* Stores host & port */
	char *output;
	sock_info.he = NULL;
	sock_info.port = -1;
	char *username = "anonymous"; /* Default is anon login */
	char *password;
	char dir[128]; /* Our current directory */
	struct parse_list *head;

	int n = 0; /* For random stuff */
	char msg[256]; /* More random stuff */

	curses_init(); /* Begin Curses */

	if(argc == 1) { /* No arguments at all! This is no good. */
		print_usage(argv[0]);
	}

	while((n = getopt(argc,argv,"hp:u:V")) != -1) {
		switch(n) {
			case 'h': /* Print help */
				print_usage(argv[0]);
				break;
			case 'p':
				if((sock_info.port = testport(optarg)) < 0) {
					sprintf(msg,"!!Invalid port %s",optarg);
					fatalerror(msg);
				}
				break;
			case 'u':
				username = optarg;
				break;
			case 'V': /* Version information */
				print_version();
				break;
			default:
				print_usage(argv[0]);
				break;
		}
	}

	/* Now all options are done, so we can start getting the hostname and
	 * stuff. */
	if(optind == argc) { /* No more args!! */
		print_usage(argv[0]);
	}
	else { /* Ok, there are more args */
		if((sock_info.he = testhost(argv[optind])) != NULL) {
			/* Well, ok then. */
		}
		else { /* Hostname is no good */
			sprintf(msg,"!!Invalid hostname %s",argv[optind]);
			fatalerror(msg);
		}
	}
	/* The other arguments are useless, we silently discard them */
	
	sock_info.name = argv[optind];

	/* Ask for password if necessary */
	if(strcmp(username,"anonymous") != 0) {
		output = malloc(1024);
		sprintf(msg,"%s@%s password: ",username,sock_info.name);
		prompt(msg,output);
		password = malloc(strlen(output)+1);
		strcpy(password,output);
		free(output);
	}
	else {
		password = malloc(strlen("anonymous@")+1);
		strcpy(password,"anonymous@");
	}

	/* Reading of the command line is complete. */
	if(sock_info.port <= 0) { /* We have a blank port! */
		sock_info.port = 21; /* Change to default port */
		status("(Using default port 21)");
	}

	/* At this point we can safely complete the Curses init. */
	curses_display();
	error(""); /* For refresh */

	p_header("","");

	sockfd = ftp_connect(sock_info);
	p_header(sock_info.name,"");
	output = ftp_read(sockfd);
	strip_nl(output);
	while(1) { /* Loop until we get the welcome message in full */
		if(str_begin(output,"220-")) { /* The welcome message is multi-line */
			if(strstr(output,"220 ") != 0) { /* However, we were sent the last line */
				status(strstr(output,"220 "));
				break; /* Zoinks! Let's get out of here! */
			}
			else { /* We weren't sent the last line */
				free(output);
				output = ftp_read(sockfd);
				strip_nl(output);
			}
		}
		else if(!str_begin(output,"220")) { /* We didn't even get a welcome! */
			fatalerror(output);
		}
		else { /* We need not even worry */
			break;
		}
	}
	free(output);

	/* Log in */
	login: status("Sending username...");
	sprintf(msg,"USER %s\n",username);
	output = send_cmd(sockfd,msg);
	if(!str_begin(output,"230")) { /* 230 means we don't need a password */
		status("Sending password...");
		if(str_begin(output,"331")) { /* We do, in fact, need a password. */
			free(output);
			sprintf(msg,"PASS %s\n",password);
			output = send_cmd(sockfd,msg);
			pwdtest: if(str_begin(output,"230 ")) { /* Space implies no msg */
				free(output); /* Well, OK */
			}
			else if(str_begin(output,"230")) { /* All right, fine */
				if(strstr(output,"230 ") != 0) {
					status(strstr(output,"230 "));
					free(output);
				}
				else { /* Read from the socket again, we've yet to be done */
					free(output);
					output = ftp_read(sockfd);
					strip_nl(output);
					status(output);
					goto pwdtest; /* Too lazy to code properly */
				}
			}
			else if(str_begin(output,"530")) {
				curses_getch(); /* Let this sink in. */
				free(output);
				output = malloc(1024);
				sprintf(msg,"%s@%s password: ",username,sock_info.name);
				prompt(msg,output);
				password = malloc(strlen(output));
				strcpy(password,output);
				free(output);
				goto login; /* SPAGHETTI CODE WOO */
			}
			else { /* We COULD still have a 230 hiding in there somewhere. */
				if(strstr(output,"230 ") != 0) {
					status(strstr(output,"230 "));
				}
				else { /* I seriously don't know what's going on. */
					fatalerror(output);
				}
			}
		}
		else { /* Either the username is invalid or something else occured */
			fatalerror(output); /* I don't want to see what it is */
		}
	}
	else { /* Cool, we didn't need a password */
		while(strstr(output,"230 ") == 0) {
			free(output);
			output = ftp_read(sockfd);
			strip_nl(output);
		}
		status(strstr(output,"230 "));
		free(output);
	}

	free(password);

	/* Username and password sent, now it's time to get the directory tree. */
	output = send_cmd(sockfd,"PWD\n"); /* Where are we?! */
	if(str_begin(output,"257 ")) { /* :) */
		parse_pwd(output,dir);
		p_header(sock_info.name,dir);
		free(output);
	}
	else { /* ?_? */
		fatalerror(output);
	}

	/* Change type to binary because it's just better that way. */
	output = send_cmd(sockfd,"TYPE I\n");
	if(!str_begin(output,"200 ")) {
		fatalerror(output);
	}
	free(output);

	/* Now we go into passive mode and actually ask for the dir listing. */
	head = get_list(sockfd,sock_info);

	if(head != NULL) {
		set_line(1);
		p_dir(head); /* Print the directory listing */
	}

	/* Now we begin the main loop */
	cmd_loop(sockfd,sock_info,dir,head);

	/* If we get here the program is closing */
	output = send_cmd(sockfd,"QUIT\n"); /* Tell the FTP server we are leaving */
	close(sockfd); /* Close the socket connection */
	curses_end();
	printf("%s\n",output);
	free(output);

	return 0;
} 

