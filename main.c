#include <stdio.h>
#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

void Func(void* num)
{
	printf("thread %ld is working, number = %d\n", pthread_self(), num);
	sleep(1);
}

int main()
{
	ThreadPool* pool = CreateThreadPool(3, 10, 100);
	for (int i = 0; i < 100; i++)
	{
		int* num = (int*)malloc(sizeof(int));
		*num = i + 100;
		threadPoolAdd(pool, Func, num);
	}

	sleep(10);
	threadPoolDestroy(pool);
	return 0;
}
