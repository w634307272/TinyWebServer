//
// Created by lee on 2021/7/25.
//
#pragma once
#include <queue>
#include <pthread.h>
#include "../lock/locker.h"

using callback = void(*)(void* arg);

//任务结构体
template<typename T>
struct Task{
    Task<T>(){
        function = nullptr;
        arg = nullptr;
    }
    Task<T>(callback f, void* arg){

        function = f;
        this -> arg = (T*)arg;
    }
    callback function;
    T* arg;
};
template<typename T>
class TaskQueue {
public:
    TaskQueue();
    ~TaskQueue();
    //添加任务
    void addTask(Task<T> task);
    void addTask(callback function, void* arg);
    //取出一个任务
    Task<T> takeTask();
    //获取当前任务的个数
    inline int taskNumber(){
        return m_taskQ.size();
    }

private:
    std::queue<Task<T>> m_taskQ;
    locker m_mutex;
};


