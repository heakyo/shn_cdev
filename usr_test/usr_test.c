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
#include<sys/ioctl.h>
#include<fcntl.h>
#include<errno.h>

#define FPATH "/dev/shn_cdev"
#define BUF_LEN 0x8000

// ioctl cmd
#define SHNCDEV_IOC_MAGIC 	'S'

#define SHNCDEV_IOC_GB 		_IO(SHNCDEV_IOC_MAGIC, 8) // get bar len

struct shn_ioctl {
	union {
		int size;
		int bar;
	};
};

int main(int argc, char *argv[])
{
	int fd;
	char rd_buf[BUF_LEN];
	struct shn_ioctl ioctl_data;

	ioctl_data.size = 32;

	fd = open(FPATH, O_RDWR);
	if (fd == -1) {
		perror("open file error");
		return errno;
	}

#if 0
	if (ioctl(fd, SHNCDEV_IOC_GB, &ioctl_data) == -1) {
		perror("ioctl error");
		return errno;
	}
	printf("size: %d\n", ioctl_data.size);
#endif
	if(lseek(fd, 0, SEEK_SET) < 0) {
		perror("seek error");
	}

	if (read(fd, rd_buf, 8) < 0) {
		perror("read file error");
	}
	//printf("rd_buf: %x\n", *(int *)&rd_buf[4]);

	close(fd);

	return 0;
}
