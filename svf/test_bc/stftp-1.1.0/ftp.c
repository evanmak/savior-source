/** \file 
 * The FTP backend.
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "misc.h"
#include "main.h"
#include "ftp.h"
#include "listparse.h"
#include "tty.h"

/**
 * Tests whether the hostname is valid. Will return a hostent structure if it
 * works out. If not, NULL. This function tries to figure out
 * if something is IP address-like - if so, it'll use inet_aton and
 * gethostbyaddr. If not, it'll use gethostbyname. Obviously if these things
 * fail we'll just return NULL. 
 *
 * \param host The hostname given. 
 * \return A hostent structure with the appropriate fields filled in.
 */
struct hostent *testhost(char *host) {
	int i;
	int result = 1; /* Will be 1 or 0 in the end */
	struct in_addr temp; /* May be used if we have IP */
	struct hostent *he; /* What we will return in the end */

	if(strlen(host) > 127) { /* Too big */
		return NULL;
	}
	if(strlen(host) > 15) { /* Short circuits! IP address are <= 15 chars */
		result = 0;
	}

	if(result) {
		/* Now we actually have to do work */
		for(i = 0;i<16;i++) {
			if(!isdigit(host[i]) && host[i] != '.') { /* Could not be an IP */
				if(host[i] == '\0') { /* Oh, OK */
					break;
				}
				result = 0; /* Nope, it wasn't */
				break;
			}
		}
	}

	if(result) { /* IP address */
		if(inet_aton(host,&temp)) {
			he = gethostbyaddr(&temp,sizeof(temp),AF_INET);
		}
		else {
			return NULL; /* Might as well short-circuit */
		}
	}
	else {
		he = gethostbyname(host);
	}

	return he; /* If we have NULL, we rely on other people to worry about it */
}

/** 
 * Simliar test as testhost, but for the port number. Much easier. At most 5
 * digits.
 *
 * \param port The port number as a string.
 * \return The port number as an int if it is valid, or 0 if not.
 */
int testport(char *port) {
	int i;
	int portnum;
	if(strlen(port) > 5) {
		return -1;
	}
	for(i=0;i<6;i++) {
		if(!isdigit(port[i])) {
			if(port[i] == '\0') { /* Done! */
				portnum = atoi(port);
				if(portnum > 65535) {
					return -1;
				}
				else {
					return portnum;
				}
			}
			return -1;
		}
	}
	
	portnum = atoi(port);
	return ((portnum > 65535) ? -1 : portnum);
}

/**
 * Connects to a host and port given in the struct.
 * 
 * \param hostport Where the host and port are to be found.
 * \return The file descriptor of the socket used to connect. 
 */
int ftp_connect(struct sock_hostport hostport) {
	int sockfd = socket(PF_INET, SOCK_STREAM,0);
	struct sockaddr_in sock_addr;
	struct in_addr sock_inaddr;
	if(sockfd < 0) {
		fatalerror("!!Socket creation failed");
		return -1;
	}
	struct in_addr *temp = (struct in_addr *) *hostport.he->h_addr_list;
	sock_inaddr = *temp;
	memset(&sock_addr,0,sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(hostport.port);
    sock_addr.sin_addr = sock_inaddr;
    if(connect(sockfd,(struct sockaddr *)&sock_addr,sizeof(sock_addr)) < 0) {
        fatalerror("!!Socket connection failed");
        return -1;
    }

	return sockfd;
}

/**
 * Reads 4 kilobytes from the socket given by sockfd. Puts this data into a 
 * char which is malloc'd. Thus, the pointer should be freed after use. 
 *
 * \param sockfd FTP socket descriptor.
 * \return Whatever was read, or NULL if nothing was read. 
 */
char *ftp_read(int sockfd) {
	int n;
	char *data;
	data = calloc(4096,sizeof(char)); /* 4K is really the most anyone should send */
	
	n = read(sockfd,data,4095);
	if(n < 0) {
		free(data);
		fatalerror("!!Socket read failed");
	}
	else if(n == 0) { /* EOF */
		free(data);
		return NULL;
	}

	return data;
}

/**
 * Like ftp_read, but also gives the amount of data read. 
 *
 * \param sockfd FTP socket descriptor.
 * \param size A pointer wherein will be placed the number of bytes read.
 * \return The data read, or NULL if none was read.
 */
char *ftp_nread(int sockfd,int *size) {
	char *data;
	data = calloc(4096,sizeof(char)); /* 4K is really the most anyone should send */
	
	*size = read(sockfd,data,4095);
	if(*size < 0) {
		free(data);
		fatalerror("!!Socket read failed");
	}
	else if(*size == 0) { /* EOF */
		free(data);
		return NULL;
	}

	return data;
}

/**
 * Send some data to the FTP server.
 *
 * \param sockfd FTP socket descriptor.
 * \param msg The data to send.
 */
void ftp_write(int sockfd,char *msg) {
	int n;
	
	n = write(sockfd,msg,strlen(msg));
	if(n < 0) {
		fatalerror("!!Socket write failed");
	}

	return;
}
	
/**
 * Like ftp_write, but with length parameter.
 *
 * \param sockfd FTP socket descriptor.
 * \param msg The data to send.
 * \param length The length of the data.
 */
void ftp_nwrite(int sockfd,char *msg,int length) {
	int n;
	
	n = write(sockfd,msg,length);
	if(n < 0) {
		fatalerror("!!Socket write failed");
	}

	return;
}

/** 
 * Given the output from PASV and the hostent structure, connects to the PASV
 * port and returns the file descriptor for that socket.
 * 
 * \param output The output received from PASV. The format is non-standard,
 * but is assured to begin with 227 and have 6 numbers separated by commas.
 * \param pasv_info Hostname and port information for the PASV address.
 * \return A new file descriptor for the PASV socket.
 */
int pasv_connect(char *output,struct sock_hostport pasv_info) {
	char *o_ptr = output;
	char p1[4]; /* For the first port part */
	char p2[4]; /* The other port part */
	int pasvfd = -1; /* What we return */
	int i; /* The index var as usual */

	o_ptr = o_ptr + 4; /* Skip past "227 " */
	while(!isdigit(*o_ptr)) {
		o_ptr++;
	}
	/* Now we are at the beginning of the IP address... */
	for(i=0;i<4;i++) {
		while(*o_ptr++ != ',');
	}

	/* ..and now we are the beginning of the ports. */
	char *p_ptr = p1;
	while(*o_ptr != ',') {
		*p_ptr++ = *o_ptr++;
	}
	o_ptr++;

	p_ptr = p2;
	while(isdigit(*o_ptr)) {
		*p_ptr++ = *o_ptr++;
	}

	/* port number = p1*256 + p2 */
	pasv_info.port = strtol(p1,(char **)NULL,10) * 256;
	pasv_info.port += strtol(p2,(char **)NULL,10);

	pasvfd = ftp_connect(pasv_info);

	return pasvfd;
}

/**
 * Change working directory on the server. 
 *
 * \param sockfd FTP socket descriptor.
 * \param info Hostname and port information.
 * \param newdir The directory we'd like to change to. 
 * \return 1 for success, else 0.
 */
int ftp_cwd(int sockfd, struct sock_hostport info, char *newdir) {
	char msg[strlen(newdir)+7];
	strcpy(msg,"CWD ");
	strcat(msg,newdir);
	strcat(msg,"\r\n");

	char *output = send_cmd(sockfd,msg);
	if(str_begin(output,"2")) {
		free(output);
		return 1;
	}
	else {
		free(output);
		return 0;
	}
}

/**
 * Retrieve a file.
 *
 * \param sockfd FTP socket descriptor.
 * \param info Hostname and port info.
 * \param lp The structure holding file information.
 */
void ftp_retr(int sockfd, struct sock_hostport info, struct listparse *lp) {
	char *output;
	int pasvfd;
	char *name = malloc((lp->namelen)+9);
	FILE *fd;	/* The local file */
	long int size = 0; /* Size of file; kept as a running total */
	int csize = 0; /* Instantaneous size */
	time_t b_time, time1, time2; 
	/* For keeping track of how long we've been downloading */
	char *stat_line;
	
	strncpy(name,lp->name,lp->namelen);
	name[lp->namelen] = '\0';

	fd = fopen(name,"w");
	if(fd == NULL) {
		error("Unable to open local file for writing");
		free(name);
		return;
	}

	output = send_cmd(sockfd,"PASV\r\n");
	if(str_begin(output,"227")) {
		pasvfd = pasv_connect(output,info);
		free(output);
		ftp_write(sockfd,"RETR ");
		ftp_write(sockfd,name);
		output = send_cmd(sockfd,"\r\n");
		strip_nl(output);
		if(str_begin(output,"1")) { /* ok go */
			free(output);
			b_time = time(NULL);
			time1 = b_time;
			output = ftp_nread(pasvfd,&csize);
			stat_line = malloc(160);
			while(csize != 0) {
				fwrite(output,1,csize,fd);
				size += csize;
				free(output);
				time2 = time(NULL);
				if(time2 - time1 >= 1) { /* Update the progress each second */
					sprintf(stat_line,"Downloading %s: %li/%li (%.1f%%)",name,size,
						lp->size,(double)size*100/lp->size);
					status(stat_line);
					time1 = time(NULL);
				}
				output = ftp_nread(pasvfd,&csize);
			}
			close(pasvfd);

			output = ftp_read(sockfd);
			strip_nl(output);
			if(str_begin(output,"22")) { /* Most likely good */
				if(size != lp->size) {
					error("WARNING: File may be truncated!");
				}
			}

			if(time2 == b_time) { /* To avoid DIV 0 errors */
				time2++;
			}

			status(output);
			sprintf(stat_line,"Download complete: %li bytes in %li seconds (%.1fKb/s)",
				size,(long)time2-b_time,(double)size/(time2-b_time)/1024);
			status(stat_line);
			free(stat_line);
		}
		else {
			close(pasvfd); /* Keeping neat and tidy */
		}
	}

	free(output);
	free(name);
	fclose(fd);
}

/**
 * Send a file to the server.
 *
 * \param sockfd FTP socket descriptor.
 * \param info Hostname and port info.
 * \param fd The descriptor for the file we want to send (on local machine).
 *			Note that the file should already be opened.
 * \param path Where we want to put the file on the remote machine.
 * \param gsize The given size of what we're sending, so we can check against
 * 				it at the end.
 * \return 1 if we were able to upload it (but not necessarily all the way),
 *			0 if the server refused it or encountered some other problem.
 */
int ftp_stor(int sockfd, struct sock_hostport info, FILE *fd, char *path,
				long int gsize) {
	/* Begin with setting a PASV. Then send the STOR request. That should
	 * consist of "STOR /path/to/remote/location". If we get 150, then go 
	 * over to the PASV connection and feed it data until done. Then come
	 * back to the original connection and read. If we got a 226, :) If
	 * not, crap. Then it's probably a good idea to refresh the remote
	 * server screen. THE END. */
	
	char *output;
	int pasvfd;
	long int size = 0; /* Running total of file size */
	int csize = 0; /* Instantaneous size */
	time_t b_time, time1, time2;
	char *stat_line;
	int ok = 0;

	output = send_cmd(sockfd,"PASV\r\n");
	if(str_begin(output,"227")) {
		pasvfd = pasv_connect(output,info);
		free(output);
		ftp_write(sockfd,"STOR ");
		ftp_write(sockfd,path);
		output = send_cmd(sockfd,"\r\n");
		strip_nl(output);
		if(str_begin(output,"1")) { /* all right to proceed */
			ok = 1;
			free(output);
			b_time = time(NULL);
			time1 = b_time;
			output = calloc(4096,sizeof(char));
			stat_line = malloc(160);
			csize = fread(output,sizeof(char),4095,fd);
			size += csize;
			while(csize != 0) {
				ftp_nwrite(pasvfd,output,csize);
				free(output);
				time2 = time(NULL);
				if(time2 - time1 >= 1) {
					sprintf(stat_line,"Uploading %s: %li/%li (%.1f%%)",path,size,
						gsize,(double)size*100/gsize);
					status(stat_line);
					time1 = time(NULL);
				}
				output = calloc(4096,sizeof(char));
				csize = fread(output,sizeof(char),4095,fd);
				size += csize;
			}
			close(pasvfd);

			free(output);

			output = ftp_read(sockfd);
			strip_nl(output);
			if(str_begin(output,"22")) {
				if(size != gsize) {
					error("WARNING: File may be truncated!");
				}
			}

			if(time2 == b_time) {
				time2++;
			}

			status(output);
			sprintf(stat_line,"Upload complete: %li bytes in %li seconds (%.1fKb/s)",
				size,(long)time2-b_time,(double)size/(time2-b_time)/1024);
			status(stat_line);
			free(stat_line);
		}
		else {
			close(pasvfd);
		}
	}

	free(output);
	fclose(fd);
	
	return ok;
}

/**
 * Saves time by abstraction. 
 * Writes to socket, reads from socket, and puts msg in status. 
 * Also returns the output in case we want to do something with it. 
 *
 * \param sockfd FTP socket descriptor.
 * \param msg What we want to appear in the status bar.
 * \return Data read from socket.
 */
char *send_cmd(int sockfd,char *msg) {
	char *output;

	ftp_write(sockfd,msg);
	output = ftp_read(sockfd);
	strip_nl(output);
	status(output);

	return output;
}

/**
 * Like send_cmd, but doesn't return anything and frees up the memory for us.
 *
 * \param sockfd FTP socket descriptor.
 * \param msg What we want to appear in the status bar.
 */
void v_send_cmd(int sockfd,char *msg) {
	free(send_cmd(sockfd,msg));

	return;
}
