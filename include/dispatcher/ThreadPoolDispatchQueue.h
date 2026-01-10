/**
 * @file ThreadPoolDispatchQueue.h
 * @brief 线程池调度队列
 *
 * 使用线程池实现的调度队列，支持真正的并发任务执行。
 * 通过多个工作线程同时从 TaskQueue 中获取和执行任务。
 */

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "DispatchQueue.h"
#include "TaskQueue.h"

namespace dispatch {

/**
 * @brief 线程池调度队列
 *
 * 与 ThreadedDispatchQueue 的区别：
 * - ThreadedDispatchQueue: 单线程，串行执行任务
 * - ThreadPoolDispatchQueue: 多线程，并发执行任务
 *
 * 使用场景：
 * - CPU密集型任务的并行处理
 * - I/O密集型任务的并发处理
 * - 需要限制并发数的任务调度
 *
 * @note 任务之间没有顺序保证，如需顺序执行请使用 ThreadedDispatchQueue
 */
class ThreadPoolDispatchQueue : public DispatchQueue {
 public:
  /**
   * @brief 创建线程池调度队列
   * @param name 队列名称
   * @param threadCount 工作线程数量
   * @return std::shared_ptr<ThreadPoolDispatchQueue> 队列实例
   */
  static std::shared_ptr<ThreadPoolDispatchQueue> create(const std::string& name, size_t threadCount);

  /**
   * @brief 创建线程池调度队列（使用硬件并发数）
   * @param name 队列名称
   * @return std::shared_ptr<ThreadPoolDispatchQueue> 队列实例
   */
  static std::shared_ptr<ThreadPoolDispatchQueue> create(const std::string& name);

  ~ThreadPoolDispatchQueue() override;

  // IDispatchQueue 接口实现
  void sync(const DispatchFunction& function) override;
  void async(DispatchFunction function) override;
  TaskId asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) override;
  void cancel(TaskId taskId) override;

  // DispatchQueue 接口实现
  bool isCurrent() const override;
  void fullTeardown() override;
  void setListener(const std::shared_ptr<IQueueListener>& listener) override;
  std::shared_ptr<IQueueListener> getListener() const override;

  /**
   * @brief 获取工作线程数量
   * @return size_t 线程数量
   */
  size_t threadCount() const { return thread_count_; }

  /**
   * @brief 获取队列名称
   * @return const std::string& 队列名称
   */
  const std::string& name() const { return name_; }

 private:
  ThreadPoolDispatchQueue(const std::string& name, size_t threadCount);

  /**
   * @brief 启动所有工作线程
   */
  void start();

  /**
   * @brief 工作线程主循环
   * @param threadIndex 线程索引（用于调试）
   */
  void workerMain(size_t threadIndex);

  std::string name_;                                   ///< 队列名称
  size_t thread_count_;                                ///< 工作线程数量
  TaskQueue task_queue_;                               ///< 任务队列
  std::vector<std::unique_ptr<std::thread>> workers_;  ///< 工作线程列表
  std::atomic<bool> running_{false};                   ///< 运行状态标志
};

}  // namespace dispatch
