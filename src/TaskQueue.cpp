/**
 * @file TaskQueue.cpp
 * @brief 任务队列实现
 */

#include "dispatcher/TaskQueue.h"

namespace dispatch {

TaskQueue::Task::Task(TaskId id, DispatchFunction function, std::chrono::steady_clock::time_point executeTime,
                      bool isBarrier)
    : id(id), function(std::move(function)), executeTime(executeTime), isBarrier(isBarrier) {}

TaskQueue::TaskQueue() : disposed_(false) {}

TaskQueue::~TaskQueue() { dispose(); }

void TaskQueue::dispose() {
  if (!disposed_) {
    disposed_ = true;

    // 清空任务队列
    std::deque<Task> toDelete;
    mutex_.lock();
    toDelete.swap(tasks_);
    mutex_.unlock();

    // 唤醒所有等待的线程
    condition_.notify_all();
  }
}

void TaskQueue::sync(const DispatchFunction& function) {
  // sync 通过 barrier 实现，确保同步执行
  barrier(function);
}

void TaskQueue::async(DispatchFunction function) { enqueue(std::move(function)); }

TaskId TaskQueue::asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) {
  return enqueue(std::move(function), delay).id;
}

EnqueuedTask TaskQueue::enqueue(DispatchFunction function) {
  // 立即执行 = 当前时间
  return enqueue(std::move(function), std::chrono::steady_clock::now());
}

EnqueuedTask TaskQueue::enqueue(DispatchFunction function, std::chrono::steady_clock::duration delay) {
  // 延迟执行 = 当前时间 + 延迟
  return enqueue(std::move(function), std::chrono::steady_clock::now() + delay);
}

TaskId TaskQueue::insertTask(DispatchFunction&& function, std::chrono::steady_clock::time_point executeTime,
                             bool isBarrier) {
  // 生成唯一的任务ID
  auto id = ++taskIdCounter_;

  // 创建任务对象
  Task task(id, std::move(function), executeTime, isBarrier);

  // 按执行时间排序插入（二分查找插入位置）
  // 相同执行时间的任务按ID顺序（先入先出）
  auto it = std::upper_bound(tasks_.begin(), tasks_.end(), task, [](const Task& a, const Task& b) {
    if (a.executeTime == b.executeTime) {
      return a.id < b.id;
    }
    return a.executeTime < b.executeTime;
  });

  tasks_.emplace(it, std::move(task));

  return id;
}

EnqueuedTask TaskQueue::enqueue(DispatchFunction function, std::chrono::steady_clock::time_point executeTime) {
  EnqueuedTask enqueuedTask;

  // 队列已销毁，直接返回
  if (disposed_) {
    return enqueuedTask;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);

    // 插入任务
    enqueuedTask.id = insertTask(std::move(function), executeTime, false);

    // 标记是否为第一个任务（用于启动工作线程）
    enqueuedTask.isFirst = first_;
    first_ = false;

    // 如果队列从空变为非空，通知监听器
    if (empty_) {
      empty_ = false;
      if (listener_ != nullptr) {
        listener_->onQueueNonEmpty();
      }
    }
  }

  // 唤醒等待的工作线程
  condition_.notify_all();
  return enqueuedTask;
}

void TaskQueue::cancel(TaskId taskId) {
  {
    DispatchFunction toDelete;
    std::lock_guard<std::mutex> lock(mutex_);
    // 移除任务（如果存在）
    toDelete = lockFreeRemoveTask(taskId);
    // toDelete 在作用域结束时销毁
  }

  condition_.notify_all();
}

DispatchFunction TaskQueue::lockFreeRemoveTask(TaskId taskId) {
  // 线性搜索任务（已在锁保护下）
  for (auto i = tasks_.begin(); i != tasks_.end(); ++i) {
    if (i->id == taskId) {
      auto task = std::move(*i);
      tasks_.erase(i);
      return std::move(task.function);
    }
  }

  return DispatchFunction();
}

void TaskQueue::barrier(const DispatchFunction& function) {
  auto executeTime = std::chrono::steady_clock::now();

  std::unique_lock<std::mutex> lock(mutex_);

  // 插入一个屏障任务（空函数，仅作为占位符）
  auto id = insertTask(DispatchFunction(), executeTime, true);

  while (!tasks_.empty()) {
    // 等待条件：
    // 1. 没有正在运行的任务
    // 2. 屏障任务在队列最前面
    if (currentRunningTasks_ != 0 || tasks_.front().id != id) {
      condition_.wait(lock);
      continue;
    }

    // 条件满足，执行屏障函数
    currentRunningTasks_++;
    lock.unlock();

    function();  // 执行用户提供的函数

    lock.lock();
    auto toDelete = lockFreeRemoveTask(id);  // 移除屏障占位任务
    currentRunningTasks_--;
    lock.unlock();

    condition_.notify_all();
    return;
  }
}

DispatchFunction TaskQueue::nextTask(std::chrono::steady_clock::time_point maxTime, bool* shouldRun) {
  std::unique_lock<std::mutex> lock(mutex_);
  bool hasTask = false;

  while (!disposed_) {
    // 情况1：队列为空
    if (tasks_.empty()) {
      // 通知监听器队列已空
      if (!empty_) {
        empty_ = true;
        if (listener_ != nullptr) {
          listener_->onQueueEmpty();
        }
      }

      // 等待新任务或超时
      auto result = condition_.wait_until(lock, maxTime);
      if (result == std::cv_status::timeout) {
        break;  // 超时退出
      } else {
        continue;  // 有新任务，重新检查
      }
    }

    // 情况2：已达到最大并发数
    if (currentRunningTasks_ >= maxConcurrentTasks_) {
      auto result = condition_.wait_until(lock, maxTime);
      if (result == std::cv_status::timeout) {
        break;
      } else {
        continue;
      }
    }

    // 情况3：检查队列头部的任务
    const auto& nextTask = tasks_.front();

    // 如果是屏障任务，需要等待其他任务完成
    if (nextTask.isBarrier) {
      auto result = condition_.wait_until(lock, maxTime);
      if (result == std::cv_status::timeout) {
        break;
      } else {
        continue;
      }
    }

    // 如果任务的执行时间还未到
    if (nextTask.executeTime > std::chrono::steady_clock::now()) {
      auto maxTimeToWait = std::min(maxTime, nextTask.executeTime);

      auto result = condition_.wait_until(lock, maxTimeToWait);

      // 如果是因为 maxTime 超时，退出循环
      if (maxTimeToWait == maxTime && result == std::cv_status::timeout) {
        break;
      } else {
        continue;  // 继续检查
      }
    }

    // 任务可以执行
    hasTask = true;
    break;
  }

  // 准备返回任务
  DispatchFunction nextTaskFunction;
  if (disposed_ || !hasTask) {
    *shouldRun = false;
  } else {
    // 取出任务
    nextTaskFunction = std::move(tasks_.front().function);
    currentRunningTasks_++;
    tasks_.pop_front();
  }
  return nextTaskFunction;
}

size_t TaskQueue::flush() {
  // 执行所有任务（包括未来的延迟任务）
  size_t ranTasks = 0;
  while (runNextTask()) {
    ranTasks++;
  }
  return ranTasks;
}

size_t TaskQueue::flushUpToNow() {
  // 只执行当前应该执行的任务
  auto maxTime = std::chrono::steady_clock::now();
  size_t ranTasks = 0;
  while (runNextTask(maxTime)) {
    ranTasks++;
  }
  return ranTasks;
}

bool TaskQueue::runNextTask() { return runNextTask(std::chrono::steady_clock::now()); }

bool TaskQueue::runNextTask(std::chrono::steady_clock::time_point maxTime) {
  auto shouldRun = true;
  auto task = nextTask(maxTime, &shouldRun);

  if (shouldRun) {
    // 执行任务
    task();

    // 在通知条件变量之前销毁任务
    // 确保任务持有的资源被释放
    task = DispatchFunction();

    {
      std::lock_guard<std::mutex> lock(mutex_);
      currentRunningTasks_--;
    }

    condition_.notify_all();
  }
  return shouldRun;
}

bool TaskQueue::isDisposed() const { return disposed_; }

void TaskQueue::setMaxConcurrentTasks(size_t maxConcurrentTasks) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (maxConcurrentTasks_ == maxConcurrentTasks) {
      return;
    }
    maxConcurrentTasks_ = maxConcurrentTasks;
  }
  // 并发数变化可能允许更多任务执行
  condition_.notify_all();
}

void TaskQueue::setListener(const std::shared_ptr<IQueueListener>& listener) {
  std::lock_guard<std::mutex> lock(mutex_);
  listener_ = listener;
}

std::shared_ptr<IQueueListener> TaskQueue::getListener() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return listener_;
}

}  // namespace dispatch
