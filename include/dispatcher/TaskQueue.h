/**
 * @file TaskQueue.h
 * @brief 任务队列实现
 *
 * 提供线程安全的任务队列，支持任务的入队、出队、延迟执行和取消。
 * 这是调度器的核心组件，管理待执行任务的存储和调度。
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

#include "IDispatchQueue.h"
#include "IQueueListener.h"
#include "Types.h"

namespace dispatch {

/**
 * @brief 入队任务的返回信息
 *
 * 包含任务ID和是否为队列的第一个任务的标志。
 */
struct EnqueuedTask {
  TaskId id = 0;         ///< 任务的唯一标识符
  bool isFirst = false;  ///< 是否是队列创建后的第一个任务（用于启动工作线程）
};

/**
 * @brief 任务队列
 *
 * 线程安全的任务队列实现，特性：
 * - 按执行时间排序的优先队列
 * - 支持延迟执行
 * - 支持任务取消
 * - 支持屏障同步（barrier）
 * - 可配置的并发任务数
 * - 状态监听器支持
 *
 * @note TaskQueue 本身不创建线程，需要外部调用 runNextTask() 来执行任务
 */
class TaskQueue : public IDispatchQueue {
 public:
  TaskQueue();
  TaskQueue(const TaskQueue& other) = delete;
  ~TaskQueue() override;

  /**
   * @brief 销毁队列
   *
   * 标记队列为已销毁状态，清空所有待执行的任务。
   * 正在执行的任务不受影响，会继续执行完成。
   */
  void dispose();

  /**
   * @brief 入队任务（立即执行）
   * @param function 任务函数
   * @return EnqueuedTask 包含任务ID和是否首个任务的信息
   */
  EnqueuedTask enqueue(DispatchFunction function);

  /**
   * @brief 入队任务（延迟执行）
   * @param function 任务函数
   * @param delay 延迟时间
   * @return EnqueuedTask 包含任务ID和是否首个任务的信息
   */
  EnqueuedTask enqueue(DispatchFunction function, std::chrono::steady_clock::duration delay);

  /**
   * @brief 入队任务（指定执行时间）
   * @param function 任务函数
   * @param executeTime 执行时间点
   * @return EnqueuedTask 包含任务ID和是否首个任务的信息
   */
  EnqueuedTask enqueue(DispatchFunction function, std::chrono::steady_clock::time_point executeTime);

  /**
   * @brief 屏障同步
   *
   * 等待所有之前提交的任务执行完成，然后执行屏障函数，
   * 在屏障函数执行期间不会有其他任务执行。
   *
   * @param function 屏障函数
   */
  void barrier(const DispatchFunction& function);

  // IDispatchQueue 接口实现
  void sync(const DispatchFunction& function) final;
  void async(DispatchFunction function) final;
  TaskId asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) final;
  void cancel(TaskId taskId) final;

  /**
   * @brief 运行下一个任务
   *
   * 从队列中取出下一个可执行的任务并执行。
   * 如果没有可执行的任务，会等待直到指定时间。
   *
   * @param maxTime 最大等待时间点
   * @return true 如果执行了任务
   * @return false 如果超时或队列已销毁
   */
  bool runNextTask(std::chrono::steady_clock::time_point maxTime);

  /**
   * @brief 运行下一个任务（不等待）
   * @return true 如果执行了任务
   * @return false 如果没有可执行的任务
   */
  bool runNextTask();

  /**
   * @brief 刷新队列
   *
   * 执行队列中所有任务，包括未来的延迟任务。
   *
   * @return size_t 执行的任务数量
   */
  size_t flush();

  /**
   * @brief 刷新队列（仅到当前时间）
   *
   * 执行队列中所有当前应该执行的任务。
   *
   * @return size_t 执行的任务数量
   */
  size_t flushUpToNow();

  /**
   * @brief 检查队列是否已销毁
   * @return true 队列已销毁
   */
  bool isDisposed() const;

  /**
   * @brief 设置队列监听器
   * @param listener 监听器对象
   */
  void setListener(const std::shared_ptr<IQueueListener>& listener);

  /**
   * @brief 设置最大并发任务数
   *
   * 控制同时执行的任务数量上限。默认为1（串行执行）。
   *
   * @param maxConcurrentTasks 最大并发数
   */
  void setMaxConcurrentTasks(size_t maxConcurrentTasks);

  // 仅用于测试
  std::shared_ptr<IQueueListener> getListener() const;

 private:
  /**
   * @brief 内部任务结构
   */
  struct Task {
    TaskId id;                                          ///< 任务ID
    DispatchFunction function;                          ///< 任务函数
    std::chrono::steady_clock::time_point executeTime;  ///< 执行时间
    bool isBarrier;                                     ///< 是否为屏障任务

    Task(TaskId id, DispatchFunction function, std::chrono::steady_clock::time_point executeTime, bool isBarrier);
  };

  std::atomic_bool disposed_;                 ///< 队列是否已销毁
  mutable std::mutex mutex_;                  ///< 保护队列的互斥锁
  std::condition_variable condition_;         ///< 条件变量，用于等待任务
  TaskId taskIdCounter_{0};                   ///< 任务ID计数器
  std::deque<Task> tasks_;                    ///< 任务队列（按执行时间排序）
  bool empty_ = true;                         ///< 队列是否为空
  bool first_ = true;                         ///< 是否为第一个任务
  size_t currentRunningTasks_ = 0;            ///< 当前正在执行的任务数
  size_t maxConcurrentTasks_ = 1;             ///< 最大并发任务数
  std::shared_ptr<IQueueListener> listener_;  ///< 队列监听器

  /**
   * @brief 获取下一个待执行的任务
   * @param maxTime 最大等待时间
   * @param shouldRun 输出参数，是否应该执行任务
   * @return DispatchFunction 任务函数
   */
  DispatchFunction nextTask(std::chrono::steady_clock::time_point maxTime, bool* shouldRun);

  /**
   * @brief 插入任务到队列
   * @param function 任务函数
   * @param executeTime 执行时间
   * @param isBarrier 是否为屏障任务
   * @return TaskId 任务ID
   */
  TaskId insertTask(DispatchFunction&& function, std::chrono::steady_clock::time_point executeTime, bool isBarrier);

  /**
   * @brief 移除任务（无锁版本，需在持有锁时调用）
   * @param taskId 任务ID
   * @return DispatchFunction 被移除的任务函数
   */
  DispatchFunction lockFreeRemoveTask(TaskId taskId);
};

}  // namespace dispatch
