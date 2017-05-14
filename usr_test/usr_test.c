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

int main(int argc, char *argv[])
{
	int fd;

	fd = open(FPATH, O_RDWR);
	if (fd == -1) {
		perror("open file error");
		return errno;
	}

	close(fd);

	return 0;
}
