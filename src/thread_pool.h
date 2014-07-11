/*
 * thread_pool.h
 *
 *  Created on: Jul 6, 2014
 *      Author: root
 */

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

template<typename T>
class thread_pool {
public:
	thread_pool(int thread_number = 8, int max_requests = 10000);
	~thread_pool();
	bool append(T*);
private:
	static void* worker(void* arg);
	void run();
private:
	bool m_stop;
	int m_thread_number;
	size_t m_max_requests;
	pthread_t* m_threads;
	std::list<T*> m_work_queue;
	locker m_queue_locker;
	sem m_queue_stat;
};

#include <iostream>
//#include "thread_pool.h"

using namespace std;

template<typename T>
thread_pool<T>::thread_pool(int thread_number, int max_requests) :
		m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL) {
	if ((thread_number <= 0) || max_requests <= 0) {
		throw exception();
	}

	m_threads = new pthread_t[thread_number];
	if (!m_threads) {
		throw exception();
	}

	for (int i = 0; i < thread_number; ++i) {
		cout << "create the " << i << "th thread" << endl;
		int ret = pthread_create(m_threads + i, NULL, worker, this);
		if (ret != 0) {
			delete[] m_threads;
			throw exception();
		}
		ret = pthread_detach(m_threads[i]);
		if (ret != 0) {
			delete[] m_threads;
			throw exception();
		}
	}
}

template<typename T>
thread_pool<T>::~thread_pool() {
	delete[] m_threads;
	m_stop = true;
}

template<typename T>
bool thread_pool<T>::append(T* request) {
	m_queue_locker.lock();
	if (m_work_queue.size() > m_max_requests) {
		m_queue_locker.unlock();
		return false;
	}

	m_work_queue.push_back(request);
	m_queue_locker.unlock();
	m_queue_stat.post();

	return true;
}

template<typename T>
void* thread_pool<T>::worker(void* arg) {
	thread_pool* pool = (thread_pool*) arg;
	pool->run();

	return 0;
}

template<typename T>
void thread_pool<T>::run() {
	while (!m_stop) {
		m_queue_stat.wait();
		m_queue_locker.lock();
		if (m_work_queue.empty()) {
			m_queue_locker.unlock();
			continue;
		}

		T* request = m_work_queue.front();
		m_work_queue.pop_front();
		m_queue_locker.unlock();
		if (!request) {
			continue;
		}
		request->process();
	}
}


#endif /* THREAD_POOL_H_ */
