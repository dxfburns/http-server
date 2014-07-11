/*
 * host.cpp
 *
 *  Created on: Jul 10, 2014
 *      Author: root
 */

//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <stdio.h>
//#include <unistd.h>
//#include <errno.h>
//#include <string.h>
//#include <fcntl.h>
//#include <stdlib.h>
//#include <sys/epoll.h>
#include "locker.h"
#include "http_conn.h"
#include "thread_pool.h"

#include <iostream>
using namespace std;

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern void add_fd(int, int, bool);
extern void remove_fd(int, int);

void add_sig(int sig, void (handler)(int), bool restart = true) {
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	if (restart) {
		sa.sa_flags |= SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
}

void show_error(int conn_fd, const char* info) {
	cout << info << endl;
	send(conn_fd, info, strlen(info), 0);
	close(conn_fd);
}

void run_http_server() {
	const char* ip = "192.168.254.130";
	int port(6688);

	add_sig(SIGPIPE, SIG_IGN);

//	thread_pool<http_conn>* pool = NULL;
//	try {
//		pool = new thread_pool<http_conn>(1,1000);
//	} catch (exception& e) {
//		cout << e.what() << endl;
//		return;
//	}

	http_conn* users = new http_conn[MAX_FD];
	int user_count(0);

	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	cout << "listen_fd : " << listen_fd << endl;
	struct linger tmp = { 1, 0 };
	setsockopt(listen_fd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

	int ret(0);
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &addr.sin_addr);
	//addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	ret = bind(listen_fd, (struct sockaddr*) &addr, sizeof(addr));
	ret = listen(listen_fd, 5);

	epoll_event events[MAX_EVENT_NUMBER];
	int epoll_fd = epoll_create(5);
	cout << "epoll_fd : " << epoll_fd << endl;
	add_fd(epoll_fd, listen_fd, false);
	http_conn::m_epoll_fd = epoll_fd;

	while (true) {
		int number = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
		if (number < 0 && errno != EINTR) {
			cout << "epoll failure" << endl;
			break;
		}
		for (int i = 0; i < number; ++i) {
			int sock_fd = events[i].data.fd;
			cout << "sock_fd : " << sock_fd << endl;
			if (sock_fd == listen_fd) {
				struct sockaddr_in client_address;
				socklen_t client_addr_len = sizeof(client_address);
				int conn_fd = accept(listen_fd, (struct sockaddr*) &client_address, &client_addr_len);
				cout << "conn_fd : " << conn_fd << endl;
				if (conn_fd < 0) {
					cout << "errno is: " << errno << endl;
					continue;
				}
				if (http_conn::m_user_count >= MAX_FD) {
					show_error(conn_fd, "Internal server is busy.");
					continue;
				}

				users[conn_fd].init(conn_fd, client_address); //users[conn_fd].init(conn_fd, client_address);
			} else if (events[i].events & (EPOLLHUP | EPOLLERR)) {
				users[sock_fd].close_conn();
			} else if (events[i].events & EPOLLIN) {
				bool ret_read = users[sock_fd].read();
				if (ret_read) {
					//pool->append(users + sock_fd);
					users[sock_fd].process(); //single thread processing way
				} else {
					users[sock_fd].close_conn();
				}
			} else if (events[i].events | EPOLLOUT) {
				bool ret_write = users[sock_fd].write();
				if (!ret_write) {
					users[sock_fd].close_conn();
				}
			}
		}
	}

	close(epoll_fd);
	close(listen_fd);
	delete[] users;
	//delete pool;
}

