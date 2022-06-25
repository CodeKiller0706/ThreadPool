#include "threadpool.h"
#include <stdlib.h>


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

		// �������
		pool->taskQ = (Task*)malloc(sizeof(Task*) * queueSize);
		pool->queueCapcity = queueSize;
		pool->queueFront = 0;
		pool->queueRear = 0;
		pool->queueSize = 0;

		pool->shutdown = 0;

		// �����߳�
		pthread_create(&pool->managerId, NULL, manager, pool); // �������߳�
		for (int i = 0; i < min; i++)
		{
			pthread_create(&pool->threadIds, NULL, worker, pool); // �����߳�
		}
		return pool;
	} while (0);

	// �Դ���ʧ�����ݻ���
	if (pool && pool->threadIds)
		free(pool->threadIds);
	if (pool && pool->taskQ)
		free(pool->taskQ);
	if (pool)
		free(pool);

	return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
	if (pool == NULL)
		return -1;

	// �ر��̳߳�
	pool->shutdown = 1;

	// �������չ������߳�
	pthread_join(pool->managerId, NULL);

	// �����������������߳�
	for (int i = 0; i < pool->liveNum; i++)
	{
		pthread_cond_signal(&pool->notEmpty);
	}

	// �ͷŶ��ڴ�
	if (pool->taskQ)
		free(pool->taskQ);
	if (pool->threadIds)
		free(pool->threadIds);

	pthread_mutex_destroy(&pool->mutexpool);
	pthread_mutex_destroy(&pool->mutexbusy);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);

	free(pool);
	pool = NULL;

	return 0;
}

void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	if (pool == NULL)
		return NULL;

	while (1)
	{
		pthread_mutex_lock(&pool->mutexpool);

		// ��ǰ��������Ƿ�Ϊ��
		while (pool->queueSize == 0 && !pool->shutdown)
		{
			// ���������߳�
			pthread_cond_wait(&pool->notEmpty, &pool->mutexpool);

			// �ж��Ƿ������߳�
			if (pool->exitNum > 0)
			{
				pool->exitNum--;
				pool->liveNum--;
				pthread_mutex_unlock(&pool->mutexpool);
				//threadExit(pool);
				return;
			}
		}

		// �ж��̳߳��Ƿ񱻹ر�
		if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexpool);
			//threadExit(pool);
			return;
		}

		// �����������ȡ��һ������
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;

		// �ƶ�ͷ
		pool->queueFront = (pool->queueFront + 1) % (pool->queueCapcity);
		pool->queueSize--;

		pthread_cond_signal(&pool->notFull); // �����߻���������
		pthread_mutex_unlock(&pool->mutexpool);

		printf("pthread %ld start working...\n", pthread_self());
		pthread_mutex_lock(&pool->mutexbusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexbusy);

		task.function(task.arg); // �����߳�
		free(task.arg);
		task.arg = NULL;

		printf("pthread %ld end working...\n", pthread_self());
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
		// ÿ��3����һ��
		sleep(3);

		// ȡ���̳߳�������������͵�ǰ�̵߳�����
		pthread_mutex_lock(&pool->mutexpool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexpool);

		pthread_mutex_lock(&pool->mutexbusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexbusy);

		//����߳�  ����ĸ���>�����߳��� && �����߳���<����߳���
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

		// �����߳�
		// æ���߳�*2 < �����߳� && �����߳� < ��С�߳���
		if (busyNum * 2 < liveNum && liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexpool);
			pool->exitNum = addThreadNum;
			pthread_mutex_unlock(&pool->mutexpool);

			// �ù������߳���ɱ
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
		// �����������߳�
		pthread_cond_wait(&pool->notFull, &pool->mutexpool);
	}
	if (pool->shutdown)
		return;

	// �������
	pool->taskQ[pool->queueRear].function = func;
	pool->taskQ[pool->queueRear].arg = arg;
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapcity;
	pool->queueSize++;

	pthread_cond_signal(&pool->notEmpty); // �����߻���������

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
