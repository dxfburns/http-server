/*
 * epoll_server.h
 *
 *  Created on: Nov 28, 2014
 *      Author: root
 */
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <pthread.h>

#define MAX_SOCKFD_COUNT 20
#define SUB_PROCESS_COUNT 2

using namespace std;

typedef struct fork_request {
	int epoll_fd;
	int events_count;
	struct epoll_event* events;
	epoll_event* p_ev;
}fork_request;

class sub_process {
public:
	int proc_id;
	int count;
	int channel[2];
public:
	sub_process() : count(0) {}
	~sub_process();
	void run();
	void process(fork_request*);
};

class epoll_server {
public:
	epoll_server();
	~epoll_server();

	bool init_server(const char*, int);
	void listen_server();
	static void* listen_thread(void*);
	void run();
private:
	int m_epoll_fd;
	int m_sock;
	pthread_t m_listen_thread_id;
	sub_process sub_procs[SUB_PROCESS_COUNT];
};




