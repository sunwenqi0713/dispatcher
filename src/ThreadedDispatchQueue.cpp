/**
 * @file ThreadedDispatchQueue.cpp
 * @brief 线程化调度队列实现
 */

#include "dispatcher/ThreadedDispatchQueue.h"

#include <cassert>
#include <condition_variable>
#include <future>
#include <mutex>

namespace dispatch {

// 线程本地存储：当前线程所属的调度队列
static thread_local ThreadedDispatchQueue* current_ = nullptr;

ThreadedDispatchQueue::ThreadedDispatchQueue(const std::string& name, ThreadQoSClass qosClass)
    : taskQueue_(std::make_shared<TaskQueue>()),
      name_(name),
      qosClass_(qosClass),
      disableSyncCallsInCallingThread_(false) {}

ThreadedDispatchQueue::~ThreadedDispatchQueue() { teardown(); }

void ThreadedDispatchQueue::sync(const DispatchFunction& function) {
  if (disableSyncCallsInCallingThread_) {
    // 禁用调用线程执行：使用 promise/future 实现同步
    std::promise<void> promise;
    auto future = promise.get_future();

    async([&function, &promise, this]() {
      runningSync_ = true;
      function();
      promise.set_value();
      runningSync_ = false;
    });

    // 阻塞等待任务完成
    future.get();
    return;
  }

  // 默认行为：使用屏障在任意线程执行
  taskQueue_->barrier([&]() {
    auto* previousCurrent = current_;
    current_ = this;  // 设置当前队列上下文
    runningSync_ = true;
    function();
    current_ = previousCurrent;  // 恢复之前的上下文
    runningSync_ = false;
  });
}

void ThreadedDispatchQueue::async(DispatchFunction function) {
  auto task = taskQueue_->enqueue(std::move(function));

  // 如果是第一个任务，启动工作线程
  if (task.isFirst) {
    startThread();
  }
}

TaskId ThreadedDispatchQueue::asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) {
  auto task = taskQueue_->enqueue(std::move(function), delay);

  // 如果是第一个任务，启动工作线程
  if (task.isFirst) {
    startThread();
  }

  return task.id;
}

void ThreadedDispatchQueue::cancel(TaskId taskId) { taskQueue_->cancel(taskId); }

std::shared_ptr<IQueueListener> ThreadedDispatchQueue::getListener() const { return taskQueue_->getListener(); }

bool ThreadedDispatchQueue::isCurrent() const { return this == getCurrent(); }

bool ThreadedDispatchQueue::isDisposed() const { return taskQueue_->isDisposed(); }

bool ThreadedDispatchQueue::hasThreadRunning() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return thread_ != nullptr;
}

void ThreadedDispatchQueue::setListener(const std::shared_ptr<IQueueListener>& listener) {
  taskQueue_->setListener(listener);
}

void ThreadedDispatchQueue::setQoSClass(ThreadQoSClass qosClass) {
  std::lock_guard<std::mutex> lock(mutex_);
  qosClass_ = qosClass;
  // 注意：std::thread 不支持运行时更改线程优先级
  // 如需此功能，需要使用平台特定的 API
  // 例如 Linux 的 pthread_setschedparam 或 Windows 的 SetThreadPriority
}

void ThreadedDispatchQueue::setDisableSyncCallsInCallingThread(bool disableSyncCallsInCallingThread) {
  disableSyncCallsInCallingThread_ = disableSyncCallsInCallingThread;
}

ThreadedDispatchQueue* ThreadedDispatchQueue::getCurrent() { return current_; }

void ThreadedDispatchQueue::teardown() {
  // 销毁任务队列（停止接受新任务，唤醒等待的线程）
  taskQueue_->dispose();
  // 等待并销毁工作线程
  teardownThread();
}

void ThreadedDispatchQueue::fullTeardown() { teardown(); }

void ThreadedDispatchQueue::startThread() {
  std::lock_guard<std::mutex> lock(mutex_);

  // 确保线程尚未启动
  assert(thread_ == nullptr);

  // 创建工作线程
  thread_ = std::make_unique<std::thread>(
      [self = this, taskQueue = taskQueue_]() { ThreadedDispatchQueue::handler(self, taskQueue); });
}

void ThreadedDispatchQueue::teardownThread() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (thread_ != nullptr) {
    if (this != ThreadedDispatchQueue::getCurrent()) {
      // 不在工作线程中，可以安全 join
      if (thread_->joinable()) {
        thread_->join();
      }
    } else {
      // 在工作线程中调用，需要 detach 避免死锁
      if (thread_->joinable()) {
        thread_->detach();
      }
    }

    thread_ = nullptr;
  }
}

void ThreadedDispatchQueue::handler(ThreadedDispatchQueue* dispatchQueue, const std::shared_ptr<TaskQueue>& taskQueue) {
  // 设置线程本地存储，标记当前线程属于此队列
  current_ = dispatchQueue;

  // 主循环：不断从队列取任务执行
  while (!taskQueue->isDisposed()) {
    // 等待最多 100000 秒（实际上是无限等待，直到有任务或队列销毁）
    taskQueue->runNextTask(std::chrono::steady_clock::now() + std::chrono::seconds(100000));
  }
}

}  // namespace dispatch
