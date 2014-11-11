//============================================================================
// Name        : http-server.cpp
// Author      : Xuefeng Du
// Version     :
// Copyright   : Apache
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_EVENTS 10
#define PORT 8080

void set_non_blocking(int sock_fd) {
	int opts = fcntl(sock_fd, F_GETFL);
	if (opts < 0) {
		perror("fcntl(F_GETFL)\n");
		exit(1);
	}

	opts = (opts | O_NONBLOCK);
	opts = fcntl(sock_fd, F_SETFL, opts);
	if (opts < 0) {
		perror("fcntl(F_GETFL)");
		exit(1);
	}
}

struct proc_msg_t {
	int proc_id;
	int fd;
	int epoll_fd;
	int listen_fd;
	int channel[2];
	epoll_event ev;
	uint32_t evts;
};

#include <sched.h>
void set_cpu(int cpu_num) {
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpu_num, &mask);
	int res = sched_setaffinity(getpid(), sizeof(mask), &mask);
	if (res < 0) {
		perror("sched_setaffinity");
	} else {
		printf("set cpu %d for pid %d\n", cpu_num, getpid());
	}
}

int epoll_process(int epoll_fd, int fd, int listen_fd, epoll_event& ev, uint32_t evts) {
	int addr_len, conn_sock;
	struct sockaddr_in remote;
	char buf[BUFSIZ];

	if (fd == listen_fd) {
		while ((conn_sock = accept(listen_fd, (struct sockaddr*) &remote, (size_t*) &addr_len)) > 0) {
			set_non_blocking(conn_sock);
			ev.events = EPOLLIN | EPOLLET;
			ev.data.fd = conn_sock;

			int res_listen_fd = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &ev);
			if (res_listen_fd < 0) {
				perror("epoll_ctl add");
			}
		}
		printf("Sub process %d conn_sock: %d, errno: %d\n", getpid(), conn_sock, errno);
		if (conn_sock == -1) {
			if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
				perror("accept conn_sock");
			}
		}

		return 1;
	}

	if (evts & EPOLLIN) {
		printf("Sub process %d process EPOLLIN event for fd: %d\n", getpid(), fd);
		int n = 0, nread;
		while ((nread = read(fd, buf + n, BUFSIZ - 1)) > 0) {
			n += nread;
		}
		if (nread == -1 && errno != EAGAIN) {
			perror("read");
		}

		//printf("read content: %s\n", buf);

		ev.data.fd = fd;
		ev.events = evts | EPOLLOUT | EPOLLET; //should have EPOLLET, otherwise will be in dead cycle

		int res_epoll_in = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
		if (res_epoll_in == -1) {
			perror("epoll_ctl mod");
		}
	}

	if (evts & EPOLLOUT) {
		printf("Sub process  %d process EPOLLOUT event for fd: %d\n", getpid(), fd);
		char* str = "Hello World";
		sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n%s", strlen(str), str);
		int nwrite, data_size = strlen(buf);
		int n = data_size;

		while (n > 0) {
			nwrite = write(fd, buf + data_size - n, n);
			if (nwrite < n) {
				if (nwrite == -1 && errno == EAGAIN) {
					perror("write error");
				}
				break;
			}
			n -= nwrite;
		}
		//printf("send content: %s\n", buf);

		close(fd);
		printf("Sub process %d Close fd: %d\n", getpid(), fd);
	}

	return 0;
}

int main() {
	struct sockaddr_in local;

	printf("server is starting, current pid: %d\n", getpid());
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		perror("listen_fd");
		exit(1);
	}
	printf("listen_fd: %d\n", listen_fd);

	set_non_blocking(listen_fd);
	bzero(&local, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_port = htons(PORT);

	int res = bind(listen_fd, (struct sockaddr*) &local, sizeof(local));
	if (res < 0) {
		perror("listen_fd");
		exit(1);
	}

	res = listen(listen_fd, 20);
	if (res < 0) {
		perror("listen");
		exit(1);
	}

	struct epoll_event ev, events[MAX_EVENTS];

	int nfds, epoll_fd, fd;

	printf("Come into epoll logic with pid: %d\n", getpid());

	epoll_fd = epoll_create(MAX_EVENTS);
	if (epoll_fd < 0) {
		perror("epoll_create");
		exit(1);
	}
	printf("%d epoll_fd: %d\n", getpid(), epoll_fd);

	ev.events = EPOLLIN;
	ev.data.fd = listen_fd;

	res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);
	if (res < 0) {
		perror("epoll_ctl listen_fd");
		exit(1);
	}

	puts("****************************\n");

	int pipes[2];
	res = pipe(pipes);
	if (res < 0) {
		perror("pipe");
		exit(1);
	}

	set_cpu(0);

	int pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	} else if (pid == 0) {
		set_cpu(0);	//set cpu num for sub process here

		int n_read = 0;
		struct proc_msg_t pm;
		while ((n_read = read(pipes[0], (struct proc_msg_t*) &pm, sizeof(pm))) > 0) {
			printf("Sub process %d get info from pipe with size: %d, epoll_fd: %d, fd: %d, listen_fd: %d\n", getpid(), n_read, pm.epoll_fd, pm.fd,
					pm.listen_fd);
			epoll_process(pm.epoll_fd, pm.fd, pm.listen_fd, pm.ev, pm.evts);
		}
	} else {
		struct proc_msg_t pm;
		pm.proc_id = pid;
		pm.epoll_fd = epoll_fd;
		pm.listen_fd = listen_fd;
		pm.ev = ev;
		pm.channel[0] = pipes[0];
		pm.channel[1] = pipes[1];

		for (;;) {
			nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
			if (nfds < 0) {
				perror("epoll_wait");
				exit(1);
			}

			int i;
			for (i = 0; i < nfds; ++i) {
				uint32_t evts = events[i].events;
				fd = events[i].data.fd;
				printf("Main process %d process fd: %d\n", getpid(), fd);

				pm.evts = evts;
				pm.fd = fd;

				write(pm.channel[1], (struct proc_msg_t*) &pm, sizeof(pm));
			}
		}
	}
	return 0;
}
