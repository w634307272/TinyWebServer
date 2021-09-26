//
// Created by lee on 2021/7/25.
//

#include "TaskQueue.h"
template<typename T>
TaskQueue<T>::TaskQueue() {

}

template<typename T>
TaskQueue<T>::~TaskQueue() {

}

template<typename T>
void TaskQueue<T>::addTask(Task<T> task) {
    m_mutex.lock();
    m_taskQ.push(task);
    m_mutex.unlock();
}

template<typename T>
void TaskQueue<T>::addTask(callback f, void* arg) {
    m_mutex.lock();
    m_taskQ.push(Task<T>(f, arg));
    m_mutex.unlock();
}

template<typename T>
Task<T> TaskQueue<T>::takeTask() {
    Task<T> t;
    m_mutex.lock();
    if(!m_taskQ.empty()){
        t = m_taskQ.front();
        m_taskQ.pop();
    }
    m_mutex.unlock();
    return t;
}

