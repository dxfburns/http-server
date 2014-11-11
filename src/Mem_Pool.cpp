#include "stdafx.h"
#include <iostream>
using namespace std;

template<typename T, int base_size = 3>
class MemPool 
{
private:
#pragma pack(push,1)
	union ObjectChunk 
	{
		ObjectChunk* next;
		char buf[sizeof(T)];
	};

	struct MemBlock
	{
		MemBlock* next;
		ObjectChunk chunks[base_size];
	};
#pragma pack(pop)

	MemPool(const MemPool<T, base_size>&) 
	{}
	MemBlock* head;;
	MemBlock* current;
	ObjectChunk* free_chunk;
public:
	MemPool():current(NULL),free_chunk(NULL) ,max(0),capacity(base_size)
	{}

	~MemPool()
	{
		while(current)
		{
			MemBlock* t= current;
			current = current->next;
			delete t;
		}
	}

	T* New()
	{
		if(max++ < capacity)
		{
			if(!free_chunk)
			{
				MemBlock* tb = new MemBlock;
				tb->next = current;
				current = tb;

				for(unsigned i = 0; i< base_size; ++i)
				{
					current->chunks[i].next = &(current->chunks[i+1]);
				}

				current->chunks[base_size - 1].next = NULL;
				free_chunk = &(current->chunks[0]);

				head = current;
			}
		} 
		else 
		{
			Chain();
			int num = max / base_size;
			for(int i = 0; i < num; ++i) 
			{
				current = ++head;
			}
		}

		ObjectChunk* t = free_chunk;
		free_chunk = free_chunk->next;

		return reinterpret_cast<T*>(t);
	}

	void Delete(T* t)
	{
		ObjectChunk* chunk = reinterpret_cast<ObjectChunk*>(t);
		chunk->next = free_chunk;
		free_chunk = chunk;
		max--;
	}
private:
	int max, capacity;
	void Chain()
	{
		capacity += base_size;
		MemBlock* t = new MemBlock;
		current->next = t;
		t->next = NULL;

		for(unsigned i = 0; i< base_size; ++i)
		{
			t->chunks[i].next = &(t->chunks[i+1]);
		}

		t->chunks[base_size - 1].next = NULL;
		current->chunks[base_size - 1].next = &(t->chunks[0]);
		free_chunk = &(t->chunks[0]);
	}
};

void run_mem_pool()
{
	MemPool<int> pool;
	cout << sizeof(pool) << endl;
	int* p_int = pool.New();
	int* p_int1 = pool.New();
	int* p_int2 = pool.New();
	int* p_int3 = pool.New();
	pool.Delete(p_int3);
	pool.Delete(p_int);
	p_int3 = pool.New();
	cout << sizeof(pool) << endl;
}


