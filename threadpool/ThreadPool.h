//
// Created by lee on 2021/7/25.
//
#pragma once
#include "TaskQueue.h"
#include "TaskQueue.cpp"

template<typename T>
class ThreadPool {
public:
    ThreadPool(int min, int max);
    ~ThreadPool();
    //线程池添加任务
    void addTask(Task<T> task);
    //获取线程池中工作的线程的个数
    int getBusyNum();
    //获取线程池中活着的线程的个数
    int getAliveNum();
private:
    TaskQueue<T>* taskQ;
    pthread_t managerID;
    pthread_t* threadIDs;
    int minNum;
    int maxNum;
    int busyNum;
    int liveNum;
    int exitNum;
    locker mutexPool;
    cond notEmpty;

    bool shutdown;
    //工作的线程（消费者线程）任务函数
    static void* worker(void* arg);
    //管理者线程任务函数
    static void* manager(void* arg);
    //单个线程退出
    void threadExit();
};

