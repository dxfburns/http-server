/*
 * epoll_server.cpp
 *
 *  Created on: Nov 28, 2014
 *      Author: root
 */
#include "../include/epoll_server.h"
#include <sys/resource.h>
#include <sys/shm.h>
#include <unistd.h>

epoll_server::epoll_server() {
	rlimit fdLimit;
	int res = getrlimit(RLIMIT_NOFILE, &fdLimit);
	if (res < 0) {
		perror("getrlimit error");
	} else {
		printf("Hard limit: %d. Soft limit: %d\n", fdLimit.rlim_max, fdLimit.rlim_cur);
	}

	if (fdLimit.rlim_cur <= 1024) {
		fdLimit.rlim_max = 5000;
		fdLimit.rlim_cur = 5000;
		res = setrlimit(RLIMIT_NOFILE, &fdLimit);
		res = getrlimit(RLIMIT_NOFILE, &fdLimit);
		printf("Hard limit: %d. Soft limit: %d\n", fdLimit.rlim_max, fdLimit.rlim_cur);
	}

	key_t key = ftok("/usr/local/tmp/test", 0);
	int shm_id = shmget(key, sizeof(sub_process[SUB_PROCESS_COUNT]), 0666 | IPC_CREAT);
	void* shared_memory = (void*) 0;
	shared_memory = shmat(shm_id, (void*) 0, 0);
	sub_process* a_procs = (sub_process*) shared_memory;

	pid_t pid;
	int i = 0;
	for (; i < 3; ++i) {
		pid = fork();
		if (pid == 0 || pid == -1) {
			break;
		}
	}
	if(pid == 0) {
		printf("Create new process, pid: %d\n", getpid());
	}
//	if (pid == 0) {
//		printf("Create new process, index: %d, pid: %d\n", i, getpid());
//		this->sub_procs[i] = a_procs[i];
//		int res = pipe(this->sub_procs[i].channel);
//		if (res < 0) {
//			perror("pipe error");
//			exit(1);
//		}
//		printf("channel[0] = %d, channel[1] = %d\n", sub_procs[i].channel[0],sub_procs[i].channel[1]);
//		sub_procs[i].run();
//		//printf("epoll_server construct finished.\n");
//	} else if (pid == -1) {
//		exit(1);
//	}
}

epoll_server::~epoll_server() {
	close(m_sock);
}

void set_non_block(int fd) {
	int opts = O_NONBLOCK;
	if (fcntl(fd, F_SETFL, opts) < 0) {
		printf("设置非阻塞模式失败!\n");
	}
}

bool epoll_server::init_server(const char* ip, int port) {
	m_epoll_fd = epoll_create(MAX_SOCKFD_COUNT);

	set_non_block(m_epoll_fd);

	m_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_sock < 0) {
		printf("socket error!\n");
		return false;
	}

	sockaddr_in listen_addr;
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons(port);
	listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//listen_addr.sin_addr.s_addr = inet_addr(ip);

	int reuse_addr_on = 1;
	setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_on, sizeof(reuse_addr_on));

	int res;
	res = bind(m_sock, (sockaddr*) &listen_addr, sizeof(listen_addr));
	if (res < 0) {
		printf("bind error\n");
		return false;
	}
	res = listen(m_sock, 20);
	if (res < 0) {
		printf("listen error\n");
		return false;
	} else {
		printf("服务端监听中...\n");
	}

	res = pthread_create(&m_listen_thread_id, 0, listen_thread, this);
	if (res != 0) {
		printf("Server 监听线程创建失败!!!\n");
		return false;
	}

	return true;
}

int total_count = 0;
bool failed = false;
void* epoll_server::listen_thread(void* pvoid) {
	epoll_server* server = (epoll_server*) pvoid;
	sockaddr_in remote_addr;
	int len = sizeof(remote_addr);

	while (true) {
		memset((sockaddr*) &remote_addr, 0, len);
		int client_socket = accept(server->m_sock, (sockaddr*) &remote_addr, (socklen_t*) &len);
		if (client_socket < 0) {
			printf("Server Accept失败!, client_socket: %d\n", client_socket);
			//continue;
			printf("Error with total count : %d\n", total_count);
//			failed = true;
//			exit(0);
		} else {
			total_count++;
			printf("Accept a new socket : %d\n", client_socket);
			set_non_block(client_socket);
			struct epoll_event ev;
			ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
			ev.data.fd = client_socket;
			epoll_ctl(server->m_epoll_fd, EPOLL_CTL_ADD, client_socket, &ev);
			printf("Accept OK\n");
		}
	}

	return NULL;
}

void epoll_server::run() {
	struct epoll_event events[MAX_SOCKFD_COUNT];

	key_t key = ftok("/usr/local/tmp/test", 0);
	int shm_id = shmget(key, sizeof(sub_process[SUB_PROCESS_COUNT]), 0666 | IPC_CREAT);
	void* shared_memory = (void*) 0;

	while (true) {
		int nfds = epoll_wait(m_epoll_fd, events, MAX_SOCKFD_COUNT, -1);
		if (nfds > 0) {
			fork_request request;
			request.epoll_fd = m_epoll_fd;
			request.events_count = nfds;
			request.events = events;

			shared_memory = shmat(shm_id, (void*) 0, 0);
			sub_process* procs = (sub_process*) shared_memory;
			int fd = procs[0].channel[1];
			printf("pipe[1] = %d\n",fd);
			//write(fd, (fork_request*)&request, sizeof(fork_request));
		}
	}
}

sub_process::~sub_process() {
	close(this->channel[0]);
}

void sub_process::run() {
	printf("Sub process %d is running\n", getpid());

	int n;
	fork_request* request;
	while ((n = read(this->channel[0], request, sizeof(fork_request))) > 0) {
		process(request);
	}
}

void sub_process::process(fork_request* request) {
	int m_epoll_fd = request->epoll_fd;
	int nfds = request->events_count;
	struct epoll_event events[MAX_SOCKFD_COUNT]; // = request->events;

	this->count = nfds;

	printf("\n########################\n");
	printf("epoll will process %d fds\n", nfds);
	for (int i = 0; i < nfds; ++i) {
		int client_socket = events[i].data.fd;
		char buffer[1024];
		memset(buffer, 0, 1024);

		printf("Process client_socket : %d\n", client_socket);

//				if (client_socket == m_sock) {
//					printf("$$$$$$$$$$$$$$$$$$$$$ client_socket == m_sock, fd : %d\n", client_socket);
//				}

		if (events[i].events & EPOLLIN) {
			int rev_size = recv(events[i].data.fd, buffer, 1024, 0);
			if (rev_size <= 0) {
				cout << "recv error: recv size: " << rev_size << endl;
				struct epoll_event event_del;
				event_del.data.fd = events[i].data.fd;
				event_del.events = 0;
				epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, event_del.data.fd, &event_del);
				close(events[i].data.fd);
				total_count--;
				printf("Closed by rev_size <= 0 with client fd %d, total count : %d\n", events[i].data.fd, total_count);
			} else {
				printf("Terminal Received Msg Content:%s\n", buffer);
				struct epoll_event ev;
				ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
				ev.data.fd = client_socket;     //记录socket句柄
				epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, client_socket, &ev);
			}
		} else if (events[i].events & EPOLLOUT) {
			char sendbuff[1024];
			sprintf(sendbuff, "Hello, client fd: %d", client_socket);
			int sendsize = send(client_socket, sendbuff, strlen(sendbuff) + 1, MSG_NOSIGNAL);
			if (sendsize <= 0) {
				struct epoll_event event_del;
				event_del.data.fd = events[i].data.fd;
				event_del.events = 0;
				epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, event_del.data.fd, &event_del);
				close(events[i].data.fd);
				total_count--;
				printf("Closed by sendsize <= 0 with client fd %d, total count : %d\n", events[i].data.fd, total_count);
			} else {
				printf("Server reply msg ok! buffer: %s\n", sendbuff);
				struct epoll_event ev;
				ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
				ev.data.fd = client_socket;     //记录socket句柄
				epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, client_socket, &ev);
			}
		} else {
			cout << "EPOLL ERROR\n" << endl;
			epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, &events[i]);
			close(events[i].data.fd);
			total_count--;
			printf("Closed by EPOLL ERROR with client fd %d, total count : %d\n", events[i].data.fd, total_count);
		}
	}

	this->count = 0;
}

int main() {
	epoll_server server;
//	server.init_server("127.0.0.1", 8080);
//	server.run();

	return 0;
}
