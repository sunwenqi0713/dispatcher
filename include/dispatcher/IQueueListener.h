/**
 * @file IQueueListener.h
 * @brief 队列状态监听器接口
 *
 * 定义了队列状态变化的回调接口，可用于监控队列的空闲/繁忙状态。
 */

#pragma once

#include <memory>

namespace dispatch {

/**
 * @brief 队列监听器接口
 *
 * 实现此接口可以监听队列的状态变化，例如：
 * - 当队列从空变为非空时（有新任务加入）
 * - 当队列从非空变为空时（所有任务执行完毕）
 *
 * 典型用途：
 * - 显示/隐藏加载指示器
 * - 触发空闲时的清理操作
 * - 性能监控和日志记录
 */
class IQueueListener {
 public:
  virtual ~IQueueListener() = default;

  /**
   * @brief 当队列变空时调用
   *
   * 所有任务执行完毕，队列进入空闲状态时触发。
   * 注意：此回调在队列的工作线程中执行。
   */
  virtual void onQueueEmpty() = 0;

  /**
   * @brief 当队列变为非空时调用
   *
   * 有新任务加入到空队列时触发。
   * 注意：此回调在提交任务的线程中执行。
   */
  virtual void onQueueNonEmpty() = 0;
};

}  // namespace dispatch
