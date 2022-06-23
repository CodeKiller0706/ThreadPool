#include "threadpool.h"
#include <malloc.h>

const int addThreadNum = 2;

ThreadPool* CreateThreadPool(int min, int max, int queueSize)
{
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	do
	{
		if (pool == NULL)
		{
			printf("malloc threadpool fail...\n");
			break;
		}

		pool->threadIds = (pthread_t)malloc(sizeof(pthread_t) * max);
		if (pool->threadIds == NULL)
		{
			printf("malloc threadIds fail...\n");
			break;
		}
		memset(pool->threadIds, 0, sizeof(pthread_t) * max);

		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min;
		pool->exitNum = 0;

		if (pthread_mutex_init(&pool->mutexpool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexbusy, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0)
		{
			printf("mutex or cond fail...\n");

			return 0;
		}

		// 任务队列
		pool->taskQ = (Task*)malloc(sizeof(Task*) * queueSize);
		pool->queueCapcity = queueSize;
		pool->queueFront = 0;
		pool->queueRear = 0;
		pool->queueSize = 0;

		pool->shutdown = 0;

		// 创建线程
		pthread_create(&pool->managerId, NULL, manager, pool); // 管理者线程
		for (int i = 0; i < min; i++)
		{
			pthread_create(&pool->threadIds, NULL, worker, NULL); // 工作线程
		}
		return pool;
	} while (0);

	// 对创建失败数据回收
	if (pool && pool->threadIds)
		free(pool->threadIds);
	if (pool && pool->taskQ)
		free(pool->taskQ);
	if (pool)
		free(pool);

	return NULL;
}

void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	if (pool == NULL)
		return NULL;

	while (1)
	{
		pthread_mutex_lock(&pool->mutexpool);

		// 当前任务队列是否为空
		while (pool->queueSize == 0 && !pool->shutdown)
		{
			// 阻塞工作线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexpool);

			// 判断是否销毁线程
			if (pool->exitNum > 0)
			{
				pool->exitNum--;
				pool->liveNum--;
				pthread_mutex_unlock(&pool->mutexpool);
				threadExit(pool);
			}
		}

		// 判断线程池是否被关闭
		if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexpool);
			threadExit(pool);
		}

		// 从任务队列中取出一个任务
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;

		// 移动头
		pool->queueFront = (pool->queueFront + 1) % (pool->queueCapcity);
		pool->queueSize--;

		pthread_cond_signal(&pool->notFull); // 消费者唤醒生产者
		pthread_mutex_unlock(&pool->mutexpool);

		pthread_mutex_lock(&pool->mutexbusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexbusy);

		task.function(task.arg); // 调用线程
		free(task.arg);
		task.arg = NULL;

		pthread_mutex_lock(&pool->mutexbusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexbusy);
	}
}

void* manager(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (!pool->shutdown)
	{
		// 每隔3秒检测一次
		sleep(3);

		// 取出线程池中任务的数量和当前线程的数量
		pthread_mutex_lock(&pool->mutexpool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexpool);

		pthread_mutex_lock(&pool->mutexbusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexbusy);

		//添加线程  任务的个数>存活的线程数 && 存活的线程数<最大线程数
		if (queueSize > liveNum && queueSize < pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexpool);
			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < addThreadNum && pool->liveNum < pool->maxNum; i++)
			{
				if (pool->threadIds[i] == 0)
				{
					pthread_create(&pool->threadIds[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexpool);
		}

		// 销毁线程
		// 忙的线程*2 < 存活的线程 && 存活的线程 < 最小线程数
		if (busyNum * 2 < liveNum && liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexpool);
			pool->exitNum = addThreadNum;
			pthread_mutex_unlock(&pool->mutexpool);

			// 让工作的线程自杀
			for (int i = 0; i < addThreadNum; i++)
			{
				pthread_cond_signal(&pool->notEmpty);

			}
		}
	}
}

void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; i++)
	{
		if(pool->threadIds[i] == tid)
			pool->threadIds[i] = 0;
	}
	pthread_exit(NULL);
}

void threadPoolAdd(ThreadPool* pool, void(func)(void*), void* arg)
{
	pthread_mutex_lock(&pool->mutexpool);
	while (pool->queueSize == pool->queueCapcity && !pool->shutdown)
	{
		// 阻塞生产者线程
		pthread_cond_wait(&pool->notFull, &pool->mutexpool);
	}
	if (pool->shutdown)
		return;

	// 添加任务
	pool->taskQ[pool->queueRear].function = func;
	pool->taskQ[pool->queueRear].arg = arg;
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapcity;
	pool->queueSize++;

	pthread_cond_signal(&pool->notEmpty); // 生产者唤醒消费者

	pthread_mutex_unlock(&pool->mutexpool);
}

int threadPoolBusyNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexbusy);
	int ret = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexbusy);
	return ret;
}

int threadPoolAliveNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexpool);
	int ret = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexpool);
	return ret;
}
