/**
 * @file DispatchQueue.h
 * @brief 调度队列基类
 *
 * 提供调度队列的基础实现和工厂方法。
 * 这是用户主要使用的接口类。
 */

#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "IDispatchQueue.h"
#include "IQueueListener.h"
#include "Types.h"

namespace dispatch {

/**
 * @brief 线程服务质量等级
 *
 * 用于指定队列工作线程的优先级。
 * 注意：实际效果取决于操作系统支持。
 */
enum ThreadQoSClass : uint8_t {
  kThreadQoSClassLowest = 0,  ///< 最低优先级，适用于后台任务
  kThreadQoSClassLow = 1,     ///< 低优先级，适用于不紧急的任务
  kThreadQoSClassNormal = 2,  ///< 普通优先级，默认选择
  kThreadQoSClassHigh = 3,    ///< 高优先级，适用于用户交互相关任务
  kThreadQoSClassMax = 4      ///< 最高优先级，适用于实时任务
};

/**
 * @brief 调度队列基类
 *
 * 提供任务调度功能的抽象基类，特性：
 * - 异步/同步任务执行
 * - 延迟任务调度
 * - 任务取消
 * - 全局主队列支持
 *
 * 使用示例：
 * @code
 * // 创建队列
 * auto queue = DispatchQueue::create("MyQueue", kThreadQoSClassNormal);
 *
 * // 异步执行
 * queue->async([]() {
 *     std::cout << "在后台线程执行" << std::endl;
 * });
 *
 * // 同步执行
 * queue->sync([]() {
 *     std::cout << "阻塞等待完成" << std::endl;
 * });
 *
 * // 延迟执行
 * queue->asyncAfter([]() {
 *     std::cout << "1秒后执行" << std::endl;
 * }, std::chrono::seconds(1));
 *
 * // 清理
 * queue->flushAndTeardown();
 * @endcode
 */
class DispatchQueue : public IDispatchQueue {
 public:
  /// 空任务ID常量
  static constexpr TaskId kNullTaskId = 0;

  explicit DispatchQueue();
  DispatchQueue(const DispatchQueue& other) = delete;
  ~DispatchQueue() override;

  /**
   * @brief 安全的同步执行
   *
   * 如果当前已在队列线程中，直接执行函数；
   * 否则使用 sync() 提交任务。
   * 这可以避免在队列线程中调用 sync() 导致的死锁。
   *
   * @param function 要执行的函数
   * @return true 执行成功
   */
  bool safeSync(const DispatchFunction& function);

  /**
   * @brief 检查当前线程是否是队列的工作线程
   * @return true 当前在队列线程中
   */
  virtual bool isCurrent() const = 0;

  /**
   * @brief 检查是否正在执行同步任务
   * @return true 正在执行 sync 任务
   */
  bool isRunningSync() const;

  /**
   * @brief 刷新并销毁队列
   *
   * 等待所有任务执行完成，然后销毁队列。
   * 这是安全关闭队列的推荐方式。
   */
  void flushAndTeardown();

  /**
   * @brief 完全销毁队列（子类实现）
   */
  virtual void fullTeardown() = 0;

  /**
   * @brief 设置队列监听器
   * @param listener 监听器对象
   */
  virtual void setListener(const std::shared_ptr<IQueueListener>& listener) = 0;

  /**
   * @brief 设置线程服务质量等级
   * @param qosClass 服务质量等级
   */
  virtual void setQoSClass(ThreadQoSClass qosClass);

  /**
   * @brief 创建调度队列（工厂方法）
   *
   * @param name 队列名称（用于调试）
   * @param qosClass 线程优先级
   * @return std::shared_ptr<DispatchQueue> 队列实例
   */
  static std::shared_ptr<DispatchQueue> create(const std::string& name, ThreadQoSClass qosClass);

  /**
   * @brief 创建线程化的调度队列
   *
   * 创建一个始终由单独线程支持的队列。
   *
   * @param name 队列名称
   * @param qosClass 线程优先级
   * @return std::shared_ptr<DispatchQueue> 队列实例
   */
  static std::shared_ptr<DispatchQueue> createThreaded(const std::string& name, ThreadQoSClass qosClass);

  /**
   * @brief 获取当前线程所属的调度队列
   * @return DispatchQueue* 当前队列，如果不在任何队列中则返回 nullptr
   */
  static DispatchQueue* getCurrent();

  /**
   * @brief 获取主队列
   * @return DispatchQueue* 主队列，如果未设置则返回 nullptr
   */
  static DispatchQueue* getMain();

  /**
   * @brief 设置主队列
   *
   * 设置全局主队列，可在任何地方通过 getMain() 获取。
   *
   * @param main 主队列实例
   */
  static void setMain(const std::shared_ptr<DispatchQueue>& main);

  // 仅用于测试
  virtual std::shared_ptr<IQueueListener> getListener() const = 0;

  /**
   * @brief 禁用在调用线程中执行同步任务
   *
   * 启用后，sync() 将始终在工作线程中执行，而不是在调用线程中。
   *
   * @param disableSyncCallsInCallingThread 是否禁用
   */
  virtual void setDisableSyncCallsInCallingThread(bool disableSyncCallsInCallingThread);

 protected:
  bool runningSync_ = false;  ///< 是否正在执行同步任务

 private:
  static std::shared_ptr<DispatchQueue> main_;  ///< 全局主队列
};

}  // namespace dispatch
