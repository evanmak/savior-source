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
struct hostent *testhost(char *host);
int testport(char *port);
int ftp_connect(struct sock_hostport hostport);
char *ftp_read(int sockfd);
char *ftp_nread(int sockfd, int *size);
void ftp_write(int sockfd,char *msg);
int pasv_connect(char *output,struct sock_hostport pasv_info);
int ftp_cwd(int sockfd, struct sock_hostport info, char *newdir);
void ftp_retr(int sockfd, struct sock_hostport info, struct listparse *lp);
int ftp_stor(int sockfd, struct sock_hostport info, FILE *fd, char *path,
				long int gsize);
char *send_cmd(int sockfd,char *msg);
void v_send_cmd(int sockfd,char *msg);
