/**
 * @file DispatchQueue.cpp
 * @brief 调度队列基类实现
 */

#include "dispatcher/DispatchQueue.h"

#include <future>

#include "dispatcher/ThreadedDispatchQueue.h"

namespace dispatch {

// 全局主队列
std::shared_ptr<DispatchQueue> DispatchQueue::main_ = nullptr;

DispatchQueue::DispatchQueue() = default;

DispatchQueue::~DispatchQueue() = default;

bool DispatchQueue::safeSync(const DispatchFunction& function) {
  if (isCurrent()) {
    // 已在队列线程中，直接执行避免死锁
    function();
  } else {
    // 不在队列线程中，使用 sync 提交
    sync(function);
  }
  return true;
}

void DispatchQueue::flushAndTeardown() {
  // 使用 safeSync 确保在队列线程中执行 fullTeardown
  safeSync([this]() { this->fullTeardown(); });
}

bool DispatchQueue::isRunningSync() const { return runningSync_; }

std::shared_ptr<DispatchQueue> DispatchQueue::createThreaded(const std::string& name, ThreadQoSClass qosClass) {
  return std::make_shared<ThreadedDispatchQueue>(name, qosClass);
}

void DispatchQueue::setQoSClass(ThreadQoSClass /*qosClass*/) {
  // 基类默认实现：不做任何事
  // 子类可以覆盖此方法
}

void DispatchQueue::setDisableSyncCallsInCallingThread(bool /*disableSyncCallsInCallingThread*/) {
  // 基类默认实现：不做任何事
  // 子类可以覆盖此方法
}

void DispatchQueue::setMain(const std::shared_ptr<DispatchQueue>& main) { main_ = main; }

DispatchQueue* DispatchQueue::getMain() { return main_.get(); }

std::shared_ptr<DispatchQueue> DispatchQueue::create(const std::string& name, ThreadQoSClass qosClass) {
  // 默认创建线程化的队列
  return createThreaded(name, qosClass);
}

DispatchQueue* DispatchQueue::getCurrent() { return ThreadedDispatchQueue::getCurrent(); }

}  // namespace dispatch
