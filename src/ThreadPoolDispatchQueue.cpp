/**
 * @file ThreadPoolDispatchQueue.cpp
 * @brief 线程池调度队列实现
 */

#include "dispatcher/ThreadPoolDispatchQueue.h"

#include <cassert>

namespace dispatch {

// 线程局部变量，用于标识当前线程所属的队列
static thread_local ThreadPoolDispatchQueue* current_ = nullptr;

ThreadPoolDispatchQueue::ThreadPoolDispatchQueue(const std::string& name, size_t threadCount)
    : name_(name), thread_count_(threadCount) {
  assert(threadCount > 0 && "Thread count must be greater than 0");

  // 设置 TaskQueue 的最大并发数与线程数一致
  task_queue_.setMaxConcurrentTasks(threadCount);
}

std::shared_ptr<ThreadPoolDispatchQueue> ThreadPoolDispatchQueue::create(const std::string& name, size_t threadCount) {
  // 使用 new 因为构造函数是私有的
  auto queue = std::shared_ptr<ThreadPoolDispatchQueue>(new ThreadPoolDispatchQueue(name, threadCount));
  queue->start();
  return queue;
}

std::shared_ptr<ThreadPoolDispatchQueue> ThreadPoolDispatchQueue::create(const std::string& name) {
  // 使用硬件并发数，至少1个线程
  size_t threadCount = std::thread::hardware_concurrency();
  if (threadCount == 0) {
    threadCount = 4;  // 如果无法检测，默认4个线程
  }
  return create(name, threadCount);
}

ThreadPoolDispatchQueue::~ThreadPoolDispatchQueue() {
  // 标记停止
  running_ = false;

  // 销毁任务队列，唤醒所有等待的线程
  task_queue_.dispose();

  // 等待所有工作线程结束
  for (auto& worker : workers_) {
    if (worker && worker->joinable()) {
      worker->join();
    }
  }
}

void ThreadPoolDispatchQueue::start() {
  running_ = true;
  workers_.reserve(thread_count_);

  for (size_t i = 0; i < thread_count_; ++i) {
    workers_.push_back(std::make_unique<std::thread>(&ThreadPoolDispatchQueue::workerMain, this, i));
  }
}

void ThreadPoolDispatchQueue::workerMain(size_t threadIndex) {
  // 设置线程局部变量
  current_ = this;

  // 工作循环
  while (running_) {
    // 等待并执行下一个任务
    // 使用较长的等待时间，避免频繁轮询
    auto maxWaitTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    task_queue_.runNextTask(maxWaitTime);
  }

  // 清理线程局部变量
  current_ = nullptr;
}

void ThreadPoolDispatchQueue::sync(const DispatchFunction& function) {
  // 如果已经在当前队列的线程上，直接执行避免死锁
  if (isCurrent()) {
    function();
    return;
  }

  // 使用 barrier 实现同步执行
  task_queue_.barrier(function);
}

void ThreadPoolDispatchQueue::async(DispatchFunction function) { task_queue_.enqueue(std::move(function)); }

TaskId ThreadPoolDispatchQueue::asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) {
  return task_queue_.enqueue(std::move(function), delay).id;
}

void ThreadPoolDispatchQueue::cancel(TaskId taskId) { task_queue_.cancel(taskId); }

bool ThreadPoolDispatchQueue::isCurrent() const { return current_ == this; }

void ThreadPoolDispatchQueue::fullTeardown() {
  // 标记停止
  running_ = false;

  // 销毁任务队列，唤醒所有等待的线程
  task_queue_.dispose();

  // 等待所有工作线程结束
  for (auto& worker : workers_) {
    if (worker && worker->joinable()) {
      worker->join();
    }
  }
  workers_.clear();
}

void ThreadPoolDispatchQueue::setListener(const std::shared_ptr<IQueueListener>& listener) {
  task_queue_.setListener(listener);
}

std::shared_ptr<IQueueListener> ThreadPoolDispatchQueue::getListener() const { return task_queue_.getListener(); }

}  // namespace dispatch
