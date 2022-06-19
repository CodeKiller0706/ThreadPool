#pragma once

#include <pthread.h>

// ����ṹ��
typedef struct Task {
	void (*function)(void* arg);
	void* arg;
} Task;


// �̳߳ؽṹ��
typedef struct ThreadPool
{
	//�������
	Task* taskQ;
	int queueCapcity;  //����
	int queueSize;     //��ǰ�������
	int queueFront;    //��ͷ
	int queueRear;     //��β

	pthread_t managerId;    //�������߳�
	pthread_t* threadIds;   //�������߳�ID
	int minNum;             //��С�߳�����
	int maxNum;             //����߳�����
	int busyNum;            //busy�̸߳���
	int liveNum;            //����̸߳���
	int exitNum;            //Ҫ���ٵ��̸߳���

	pthread_mutex_t mutexpool;   //�������̳߳�
	pthread_mutex_t mutexbusy;   //��busynum����  
	pthread_cond_t notFull;      //�����������
	pthread_cond_t notEmpty;     //������п���

	int shutdown;           //�Ƿ������̳߳� 0������ 1����
} ThreadPool;


// �����̳߳ز���ʼ��
ThreadPool* CreateThreadPool(int min, int max, int queueSize);

// �����̳߳�

// ���̳߳��������

// ��ȡbusy�̸߳���

//��ȡlive�̸߳���

void* worker(void* arg);

void* manager();
