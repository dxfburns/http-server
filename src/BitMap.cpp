#include "stdafx.h"
#include <iostream>
using namespace std;

char* g_bitmap = NULL;
int g_size = 0;
int g_base = 0;

int bitmap_init(int size, int start) {
	g_size = size/8 + 1;
	g_base = start;
	g_bitmap = new char[g_size];

	if(g_bitmap == NULL) {
		return 0;
	}

	memset(g_bitmap,0,g_size);

	return 1;
}

int bitmap_set(int index) {
	int quo = (index - g_base) / 8;
	int remainder = (index - g_base) % 8;
	unsigned char x = 1<<remainder;

	if(quo > g_size) {
		return 0;
	}

	g_bitmap[quo] |= x;

	return 1;
}

int bitmap_get(int i) {
	int quo = i / 8;
	int remainder = i % 8;
	unsigned char x = 1<<remainder;

	if(quo > g_size) {
		return -1;
	}

	unsigned res = g_bitmap[quo] & x;

	return res > 0 ? 1 : 0;
}

int bitmap_data(int index) {
	return index + g_base;
}

int bitmap_free() {
	delete [] g_bitmap;
	return 0;
}

int bitmap_run() {
	int a[] = {5,8,7,6,3,1,10};
	bitmap_init(10,0);
	for(int i = 0; i< sizeof(a)/sizeof(a[0]); ++i) {
		bitmap_set(a[i]);
	}
	for(int i = 0; i<=10; ++i) {
		int res = bitmap_get(i);
		if(res>0) {
			int p_res = bitmap_data(i);
			cout << p_res << " ";
		}
	}
	cout << endl;
	bitmap_free();

	return 0;
}