#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include<vector>
#include<memory>
#include<queue>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<future>
#include<functional>
#include<stdexcept>
#include<type_traits>

using namespace std;

// 线程池
class ThreadPool {
    public:
    ThreadPool(size_t);
    template<class F, class... Args> // 返回一个基于F的函数对象，其参数被绑定到Args上
    auto enqueue(F&& f, Args&&... args) -> future<typename result_of<F(Args...)>::type>;

    int get_workers_num(){
        return workers.size();
    }

    queue<function<void()> > get_tasks(){
        return tasks;
    }

    bool get_stop(){
        return stop;
    }

    ~ThreadPool();

    private:
    // 可用线程数
    vector<thread> workers;
    // 待执行的任务
    queue<function<void()> > tasks;

    // 同步互斥
    // 锁
    mutex queue_mutex;
    // 条件变量
    condition_variable condition;
    bool stop;
};

inline ThreadPool::ThreadPool(size_t threads) : stop(false) {
    for(size_t i = 0; i<threads; i++){
        workers.emplace_back(
            [this]
            {
                for(;;){
                    function<void()> task;
                    {
                        unique_lock<mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this]{return this->stop || !this->tasks.empty();});
                        if(this->stop && this->tasks.empty())
                            return;
                        task = move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            }
        );
    }
}


template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> future<typename result_of<F(Args...)>::type>{
    using return_type = typename result_of<F(Args...)>::type;

    auto task = make_shared<packaged_task<return_type> >(
        bind(forward<F>(f), forward<Args>(args)...)
    );

    future<return_type> res = task->get_future();
    {
        unique_lock<mutex> lock(queue_mutex);

        if(stop){
            throw runtime_error("Enqueue on stopped ThreadPool");
        }

        tasks.emplace([task](){(*task)();});
    }
    condition.notify_one();
    return res;
}





inline ThreadPool::~ThreadPool(){
    {
        unique_lock<mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(thread &worker : workers){
        worker.join();
    }
}

#endif