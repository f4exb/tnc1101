/*
 * Part of ols-fwloader - Serial port routines
 * Inspired by pirate-loader, written by Piotr Pawluczuk
 * http://the-bus-pirate.googlecode.com/svn/trunk/bootloader-v4/pirate-loader/source/pirate-loader.c
 *
 * Copyright (C) 2010 Piotr Pawluczuk
 * Copyright (C) 2011 Michal Demin <michal.demin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#if _WIN32
#else
#include <errno.h>
#endif

#include "usbserial.h"

int usb_serial_setup(int fd, unsigned long speed)
{
#if _WIN32
	COMMTIMEOUTS timeouts;
	DCB dcb = {0};
	HANDLE hCom = (HANDLE)fd;

	dcb.DCBlength = sizeof(dcb);

	dcb.BaudRate = speed;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;

	if( !SetCommState(hCom, &dcb) ){
		return -1;
	}

	timeouts.ReadIntervalTimeout = 100;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.ReadTotalTimeoutConstant = 100;
	timeouts.WriteTotalTimeoutMultiplier = 100;
	timeouts.WriteTotalTimeoutConstant = 2550;

	if (!SetCommTimeouts(hCom, &timeouts)) {
		return -1;
	}

	return 0;
#else
	struct termios t_opt;
	unsigned long  baud;
  //  printf("Serial: Speed= %lu\n",speed);
	switch (speed) {
		case 921600:
			baud = 921600;
			break;
		case 115200:
			baud = 115200;
			break;
		case 1000000:
			baud = 1000000;
			break;
		case 1500000:
			baud = 1500000;
			break;
		default:
			printf("unknown speed setting: %lu \n",speed);
			baud=115200;
			printf("setting to default: %lu\n",baud);
			//return -1;
			break;
	}

	/* set the serial port parameters */
	fcntl(fd, F_SETFL, 0);
	tcgetattr(fd, &t_opt);
	cfsetispeed(&t_opt, baud);
	cfsetospeed(&t_opt, baud);
	t_opt.c_cflag |= (CLOCAL | CREAD);
	t_opt.c_cflag &= ~PARENB;
	t_opt.c_cflag &= ~CSTOPB;
	t_opt.c_cflag &= ~CSIZE;
	t_opt.c_cflag |= CS8;
	t_opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	t_opt.c_iflag &= ~(IXON | IXOFF | IXANY);
	t_opt.c_iflag &= ~(ICRNL | INLCR);
	t_opt.c_oflag &= ~(OCRNL | ONLCR);
	t_opt.c_oflag &= ~OPOST;
	t_opt.c_cc[VMIN] = 0;
	t_opt.c_cc[VTIME] = 10;

#if IS_DARWIN
	if (tcsetattr(fd, TCSANOW, &t_opt) < 0) {
		return -1;
	}

	return ioctl( fd, IOSSIOSPEED, &baud );
#else
	tcflush(fd, TCIOFLUSH);

	return tcsetattr(fd, TCSANOW, &t_opt);
#endif
#endif
}

//int serial_write(int fd, const char *buf, int size) {
//  return serial_write(fd, (char*)buf, size);
//}

int usb_serial_write(int fd, const char *buf, int size)
{
	int ret = 0;
#if _WIN32
	HANDLE hCom = (HANDLE)fd;
	int res = 0;
	unsigned long bwritten = 0;

	res = WriteFile(hCom, buf, size, &bwritten, NULL);
	//DWORD dw = GetLastError();
  // printf("serial.c: res = %i, bwritten= %lu, size = %i, handle= %lu, GetLastError=%lu\n\n",res,bwritten,size,hCom,dw);
	if( res == FALSE ) {
		ret = -1;
	} else {
		ret = bwritten;
	}
#else
	ret = write(fd, buf, size);
#endif

#if DEBUG
	if (ret != size) {
    if (ret == -1) {
      fprintf(stderr, "Error code: %d\n", ret);
    } else {
      fprintf(stderr, "Error sending %d bytes (wrote %d)\n", size, ret);
    }
  }
#endif

	return ret;
}

int usb_serial_read(int fd, char *buf, int size, int retry_limit)
{
  int len = 0;
  int rbytes = 0;

#if _WIN32
  HANDLE hCom = (HANDLE)fd;
  unsigned long bread = 0;

  rbytes = ReadFile(hCom, buf, size, &bread, NULL);

  if (rbytes == FALSE || rbytes == -1) {
    len = -1;
  } else {
    len = bread;
  }

#else
  int retries = 0;
  while (len < size) {
    rbytes = read(fd, buf + len, size - len);
    //fprintf(stderr, "\tread %d (%d of %d) bytes\t(%d of %d)\n", rbytes, len, size, retries, retry_limit);
    if (rbytes == -1){
      //fprintf(stderr, "rbytes -1");
      return -1;
    } else if (rbytes == 0) {
      retries++;
      if (retries >= retry_limit) {
        break;
      }
    } else {
      retries = 0;
      len += rbytes;
    }
  }
#endif

#if DEBUG
  if (len != size) {
    fprintf(stderr, "Error receiving %d bytes (read %d)\n", size, len);
  }
#endif

  return len;
}

int usb_serial_open(char *port)
{
	int fd;
#if _WIN32
	static char full_path[32] = {0};

	HANDLE hCom = NULL;

	if( port[0] != '\\' ) {
		_snprintf(full_path, sizeof(full_path) - 1, "\\\\.\\%s", port);
		port = full_path;
	}

	hCom = CreateFileA(port, GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if( !hCom || hCom == INVALID_HANDLE_VALUE ) {
		fd = -1;
	} else {
		fd = (int)hCom;
	}
#else
  fprintf(stderr, "opening serial port %s\n", port);
	// O_NDELAY is superceded by O_NONBLOCK in POSIX
	fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd == -1) {
		fprintf(stderr, "Could not open serial port: ");
    if (errno == ENOENT) {
      fprintf(stderr, "No such file or directory: %s\n", port);
    } else if (errno == EAGAIN) {
      fprintf(stderr, "Serial port %s is already memory-mapped by another process.\n", port);
    } else {
      fprintf(stderr, "errno: %d\n", errno);
    }
		return -1;
	}
#endif
	return fd;
}

int usb_serial_close(int fd)
{
#if _WIN32
	HANDLE hCom = (HANDLE)fd;
	return CloseHandle(hCom);
#else
	return close(fd);
#endif
}

