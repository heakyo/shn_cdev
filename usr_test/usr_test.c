/*
 * =====================================================================================
 *
 *       Filename:  usr_test.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  05/14/2017 03:18:18 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<errno.h>

#define FPATH "/dev/shn_cdev"
#define BUF_LEN 32

int main(int argc, char *argv[])
{
	int fd;
	char *rd_buf[BUF_LEN];

	fd = open(FPATH, O_RDWR);
	if (fd == -1) {
		perror("open file error");
		return errno;
	}

	if (read(fd, rd_buf, 6) < 0) {
		perror("read file error");
	}

	close(fd);

	return 0;
}
