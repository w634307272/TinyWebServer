//
// Created by lee on 2021/7/25.
//

#include "ThreadPool.h"
#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>

const int NUMBER = 2;

template<typename T>
ThreadPool<T>::ThreadPool(int min, int max){
    //实例化任务队列
    taskQ = new TaskQueue<T>;
    do{
        threadIDs = new pthread_t[max];
        if(threadIDs == nullptr){
            std::cout << "malloc threadIDs fail" << std::endl;
        }
        memset(threadIDs, 0, sizeof(pthread_t) * max);
        minNum = min;
        maxNum = max;
        busyNum = 0;
        liveNum = min;
        exitNum = 0;

        shutdown = false;

        //创建线程
        pthread_create(&managerID, NULL, manager, this);
        for(int i = 0; i < min; i++){
            pthread_create(&threadIDs[i], NULL, worker, this);
        }
        return;
    }while(0);

    //释放资源
    if(threadIDs) delete[]threadIDs;
    if(taskQ) delete taskQ;
}

template<typename T>
ThreadPool<T>::~ThreadPool() {
    //关闭线程池
    shutdown = true;
    //阻塞回收管理者线程
    pthread_join(managerID, NULL);
    //唤醒阻塞的消费者线程
    for(int i = 0; i < liveNum; i++){
        notEmpty.signal();
    }
    //释放堆内存
    if(taskQ){
        delete taskQ;
    }
    if(threadIDs){
        delete[]threadIDs;
    }
}

template<typename T>
void* ThreadPool<T>::worker(void* arg){
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while(1){
        pool->mutexPool.lock();
        while(pool -> taskQ -> taskNumber() == 0 && !pool -> shutdown){
            //std::cout << "thread " << pthread_self() << "waiting..." << std::endl;
            //阻塞工作线程
            pool->notEmpty.wait(pool->mutexPool.get());

            //判断是否要销毁该线程
            if(pool -> exitNum > 0){
                pool -> exitNum--;
                if(pool -> liveNum > pool -> minNum){
                    pool -> liveNum--;
                    pool->mutexPool.unlock();
                    pool -> threadExit();
                }
            }
        }
        //判断线程池是否被关闭了
        if(pool -> shutdown){
            pool->mutexPool.unlock();
            pool -> threadExit();
        }

        //从任务队列中取出一个任务
        Task<T> task = pool -> taskQ -> takeTask();
        //解锁
        pool -> busyNum++;
        pool->mutexPool.unlock();
        //std::cout << "thread " << pthread_self() << " start working" << std::endl;
        task.function(task.arg);
        delete task.arg;
        task.arg = nullptr;

        //任务处理结束
        //std::cout << "thread" <<pthread_self() <<"end_working..."<<std::endl;
        pool->mutexPool.lock();
        pool -> busyNum--;
        pool->mutexPool.unlock();
    }
    return NULL;
}

template<typename T>
void* ThreadPool<T>::manager(void *arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while(!pool -> shutdown){
        //每隔三秒检查一次
        sleep(3);
        //取出线程池中任务的数量和当前线程的数量
        pool->mutexPool.lock();
        int busyNum = pool -> busyNum;
        int liveNum = pool -> liveNum;
        int queueSize = pool -> taskQ ->taskNumber();
        pool->mutexPool.unlock();

        //添加线程
        //任务的个数 > 存活的线程个数 &&存活的线程数 < 最大线程数
        if(queueSize > liveNum && liveNum < pool -> maxNum){
            pool->mutexPool.lock();
            int counter = 0;
            for(int i = 0; i < pool -> maxNum && counter < NUMBER && pool -> liveNum < pool -> maxNum; i++){
                if(pool -> threadIDs[i] == 0){
                    pthread_create(&pool -> threadIDs[i], NULL, worker, pool);
                    counter ++;
                    pool -> liveNum++;
                }
            }
            pool->mutexPool.unlock();
        }

        //销毁线程
        //忙的线程 * 2 < 存活的线程数 && 存活的线程 > 最小线程数
        if(busyNum * 2 < liveNum && liveNum > pool -> minNum){
            pool->mutexPool.lock();
            pool -> exitNum = NUMBER;
            pool->mutexPool.unlock();
            for(int i = 0; i < NUMBER; i++){
                pool->notEmpty.signal();
            }
        }
    }
    return NULL;
}

template<typename T>
void ThreadPool<T>::addTask(Task<T> task) {
    if(shutdown){
        return;
    }
    taskQ -> addTask(task);
    notEmpty.signal();
}

template<typename T>
int ThreadPool<T>::getBusyNum(){
    mutexPool.lock();
    int busyNum = this -> busyNum;
    mutexPool.unlock();
    return busyNum;
}

template<typename T>
int ThreadPool<T>::getAliveNum() {
    mutexPool.lock();
    int aliveNum = this -> liveNum;
    mutexPool.unlock();
    return aliveNum;
}

template<typename T>
void ThreadPool<T>::threadExit() {
    pthread_t tid = pthread_self();
    for(int i = 0; i < maxNum; i++){
        if(threadIDs[i] == tid) {
            threadIDs[i] = 0;
            std::cout << "threadExit() called," << tid << "exiting...\n" << std::endl;
            break;
        }
    }
    pthread_exit(NULL);
}