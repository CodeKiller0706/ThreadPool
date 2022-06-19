#pragma once

#include <pthread.h>

// 任务结构体
typedef struct Task {
	void (*function)(void* arg);
	void* arg;
} Task;


// 线程池结构体
typedef struct ThreadPool
{
	//任务队列
	Task* taskQ;
	int queueCapcity;  //容量
	int queueSize;     //当前任务个数
	int queueFront;    //队头
	int queueRear;     //队尾

	pthread_t managerId;    //管理者线程
	pthread_t* threadIds;   //工作的线程ID
	int minNum;             //最小线程数量
	int maxNum;             //最大线程数量
	int busyNum;            //busy线程个数
	int liveNum;            //存活线程个数
	int exitNum;            //要销毁的线程个数

	pthread_mutex_t mutexpool;   //锁整个线程池
	pthread_mutex_t mutexbusy;   //锁busynum变量  
	pthread_cond_t notFull;      //任务队列满了
	pthread_cond_t notEmpty;     //任务队列空了

	int shutdown;           //是否销毁线程池 0不销毁 1销毁
} ThreadPool;


// 创建线程池并初始化
ThreadPool* CreateThreadPool(int min, int max, int queueSize);

// 销毁线程池

// 给线程池添加任务

// 获取busy线程个数

//获取live线程个数

void* worker(void* arg);

void* manager();
