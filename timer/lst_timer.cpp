#include "lst_timer.h"
#include "../http/http_conn.h"

time_wheel::time_wheel() : cur_slot(0)
{
    for(int i = 0; i < N; i++){
        slots[i] = NULL;
    }
}
time_wheel::~time_wheel()
{
    for(int i = 0; i < N; i++){
        util_timer* tmp = slots[i];
        while(tmp){
            slots[i] = tmp -> next;
            delete tmp;
            tmp = slots[i];
        }
    }
}

void time_wheel::add_timer(util_timer *timer)
{
    int ticks = 0;
    time_t time_gap = timer->expire - time(NULL);
    if(time_gap < SI) {
        ticks = 1;
    }
    else {
        ticks = time_gap / SI;
    }
    //计算待插入的定时器在时间轮转动多少圈后被触发
    int rotation = ticks / N;
    //计算待插入的定时器应该被插入哪个槽中
    int ts = (cur_slot + (ticks % N)) % N;

    timer->rotation = rotation;
    timer->time_slot = ts;
    //如果第ts个槽尚无任何定时器则把新建的定时器插入其中，并将该定时器设置为该槽的头结点
    if(!slots[ts]){
        slots[ts] = timer;
    }
    else{
        timer->next = slots[ts];
        slots[ts]->prev = timer;
        slots[ts] = timer;
    }

}
void time_wheel::adjust_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    int ts = timer->time_slot;
    if(timer == slots[ts]) {
        slots[ts] = slots[ts]->next;
        if(slots[ts])
            slots[ts]->prev = NULL;
    }
    else{
        timer->prev->next = timer->next;
        if(timer->next)
            timer->next->prev = timer->prev;
    }
    timer -> next = nullptr;
    timer -> prev = nullptr;
    add_timer(timer);
}
void time_wheel::del_timer(util_timer *timer)
{
    if(!timer)
        return;
    int ts = timer->time_slot;
    //slots[ts]是目标定时器所在槽的头结点。如果目标定时器就是该头结点，这需要重置第ts个槽的头结点
    if(timer == slots[ts]){
        slots[ts] = slots[ts]->next;
        if(slots[ts])
            slots[ts]->prev = NULL;
        delete timer;
    }
    else{
        timer->prev->next = timer->next;
        if(timer->next)
            timer->next->prev = timer->prev;
        delete timer;
    }
}
void time_wheel::tick()
{
    util_timer* tmp = slots[cur_slot];
    while(tmp){
        //如果定时器的rotation值大于0,则它在这一轮不起作用
        if(tmp -> rotation > 0){
            tmp->rotation--;
            tmp = tmp->next;
        }
        //否则，说明定时器已经到期，于是执行定时任务，然后删除该定时器
        else{
            tmp->cb_func(tmp->user_data);
            if(tmp == slots[cur_slot]){
                slots[cur_slot] = tmp->next;
                delete tmp;
                if(slots[cur_slot]){
                    slots[cur_slot]->prev = NULL;
                }
                tmp = slots[cur_slot];
            }
            else{
                tmp->prev->next = tmp->next;
                if(tmp->next){
                    tmp->next->prev = tmp->prev;
                }
                util_timer* tmp2 = tmp->next;
                delete tmp;
                tmp = tmp2;
            }
        }
    }
    cur_slot = ++cur_slot % N;
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data *user_data) //回调函数
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
