/**
 * @file IDispatchQueue.h
 * @brief 调度队列抽象接口
 *
 * 定义了调度队列的核心接口，所有调度队列实现都必须遵循此接口。
 */

#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include "Types.h"

namespace dispatch {

/**
 * @brief 调度队列接口
 *
 * 提供任务调度的核心功能：
 * - 同步执行：阻塞等待任务完成
 * - 异步执行：立即返回，任务在后台执行
 * - 延迟执行：指定延迟后执行任务
 * - 取消任务：通过任务ID取消尚未执行的任务
 *
 * 继承自 enable_shared_from_this 以支持安全的自引用。
 */
class IDispatchQueue : public std::enable_shared_from_this<IDispatchQueue> {
 public:
  virtual ~IDispatchQueue() = default;

  /**
   * @brief 同步执行任务
   *
   * 将任务提交到队列并阻塞当前线程，直到任务执行完成。
   * 适用于需要等待结果的场景。
   *
   * @param function 要执行的任务函数
   * @warning 不要在队列的工作线程中调用 sync，会导致死锁
   */
  virtual void sync(const DispatchFunction& function) = 0;

  /**
   * @brief 异步执行任务
   *
   * 将任务提交到队列后立即返回，任务将在队列的工作线程中执行。
   *
   * @param function 要执行的任务函数（将被移动）
   */
  virtual void async(DispatchFunction function) = 0;

  /**
   * @brief 延迟异步执行任务
   *
   * 将任务提交到队列，在指定延迟后执行。
   *
   * @param function 要执行的任务函数（将被移动）
   * @param delay 延迟时间
   * @return TaskId 任务ID，可用于取消任务
   */
  virtual TaskId asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) = 0;

  /**
   * @brief 取消任务
   *
   * 通过任务ID取消尚未执行的任务。
   * 如果任务已经开始执行，则无法取消。
   *
   * @param taskId 要取消的任务ID
   */
  virtual void cancel(TaskId taskId) = 0;
};

}  // namespace dispatch
