/**
 * @file ThreadedDispatchQueue.h
 * @brief 线程化调度队列
 *
 * 提供由独立工作线程支持的调度队列实现。
 * 这是 DispatchQueue 的主要实现类。
 */

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "DispatchQueue.h"
#include "TaskQueue.h"

namespace dispatch {

/**
 * @brief 线程化调度队列
 *
 * 特性：
 * - 拥有独立的工作线程
 * - 线程按需创建（第一个任务入队时启动）
 * - 支持同步/异步/延迟任务
 * - 支持屏障同步
 * - 线程安全
 *
 * 工作原理：
 * 1. 第一个任务入队时创建工作线程
 * 2. 工作线程循环从 TaskQueue 获取并执行任务
 * 3. 队列销毁时等待线程结束
 */
class ThreadedDispatchQueue : public DispatchQueue {
 public:
  /**
   * @brief 构造函数
   * @param name 队列名称（用于调试和线程命名）
   * @param qosClass 线程服务质量等级
   */
  ThreadedDispatchQueue(const std::string& name, ThreadQoSClass qosClass);

  ~ThreadedDispatchQueue() override;

  /**
   * @brief 同步执行任务
   *
   * 阻塞当前线程直到任务执行完成。
   * 使用屏障机制确保任务按顺序执行。
   *
   * @param function 要执行的任务
   * @warning 不要在队列的工作线程中调用，会导致死锁
   */
  void sync(const DispatchFunction& function) final;

  /**
   * @brief 异步执行任务
   * @param function 要执行的任务
   */
  void async(DispatchFunction function) override;

  /**
   * @brief 延迟异步执行任务
   * @param function 要执行的任务
   * @param delay 延迟时间
   * @return TaskId 任务ID，可用于取消
   */
  TaskId asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) override;

  /**
   * @brief 取消任务
   * @param taskId 要取消的任务ID
   */
  void cancel(TaskId taskId) override;

  /**
   * @brief 检查当前线程是否是此队列的工作线程
   * @return true 当前在工作线程中
   */
  bool isCurrent() const override;

  /**
   * @brief 完全销毁队列
   */
  void fullTeardown() override;

  /**
   * @brief 检查队列是否已销毁
   * @return true 队列已销毁
   */
  bool isDisposed() const;

  /**
   * @brief 检查工作线程是否正在运行
   * @return true 线程正在运行
   */
  bool hasThreadRunning() const;

  /**
   * @brief 设置队列监听器
   * @param listener 监听器对象
   */
  void setListener(const std::shared_ptr<IQueueListener>& listener) override;

  /**
   * @brief 设置线程服务质量等级
   * @param qosClass 服务质量等级
   */
  void setQoSClass(ThreadQoSClass qosClass) final;

  /**
   * @brief 获取当前线程所属的 ThreadedDispatchQueue
   * @return ThreadedDispatchQueue* 当前队列，如果不在任何队列中则返回 nullptr
   */
  static ThreadedDispatchQueue* getCurrent();

  // 仅用于测试
  std::shared_ptr<IQueueListener> getListener() const override;

  /**
   * @brief 禁用在调用线程中执行同步任务
   * @param disableSyncCallsInCallingThread 是否禁用
   */
  void setDisableSyncCallsInCallingThread(bool disableSyncCallsInCallingThread) override;

 private:
  mutable std::mutex mutex_;                      ///< 保护成员变量的互斥锁
  std::unique_ptr<std::thread> thread_;           ///< 工作线程
  std::shared_ptr<TaskQueue> taskQueue_;          ///< 内部任务队列
  std::string name_;                              ///< 队列名称
  ThreadQoSClass qosClass_;                       ///< 线程服务质量等级
  bool disableSyncCallsInCallingThread_ = false;  ///< 是否禁用在调用线程中执行同步任务

  /**
   * @brief 工作线程处理函数
   * @param dispatchQueue 所属的调度队列
   * @param taskQueue 任务队列
   */
  static void handler(ThreadedDispatchQueue* dispatchQueue, const std::shared_ptr<TaskQueue>& taskQueue);

  /**
   * @brief 销毁队列（内部方法）
   */
  void teardown();

  /**
   * @brief 启动工作线程
   */
  void startThread();

  /**
   * @brief 停止工作线程
   */
  void teardownThread();
};

}  // namespace dispatch
