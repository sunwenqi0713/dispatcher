/**
 * @file timer_example.cpp
 * @brief 定时器示例
 *
 * 演示如何使用 asyncAfter 实现定时器和周期性任务
 */

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>

#include "dispatcher/DispatchQueue.h"

using namespace dispatch;
using namespace std::chrono_literals;

/**
 * @brief 简单定时器类
 */
class Timer {
 public:
  Timer(std::shared_ptr<DispatchQueue> queue)
      : m_queue(queue), m_taskId(DispatchQueue::kNullTaskId), m_running(false) {}

  // 单次定时器
  void scheduleOnce(std::chrono::milliseconds delay, std::function<void()> callback) {
    cancel();
    m_running = true;
    m_taskId = m_queue->asyncAfter(
        [this, callback]() {
          if (m_running) {
            callback();
            m_running = false;
          }
        },
        delay);
  }

  // 周期性定时器
  void scheduleRepeating(std::chrono::milliseconds interval, std::function<void()> callback) {
    m_running = true;
    scheduleNext(interval, callback);
  }

  void cancel() {
    m_running = false;
    if (m_taskId != DispatchQueue::kNullTaskId) {
      m_queue->cancel(m_taskId);
      m_taskId = DispatchQueue::kNullTaskId;
    }
  }

  bool isRunning() const { return m_running; }

 private:
  void scheduleNext(std::chrono::milliseconds interval, std::function<void()> callback) {
    if (!m_running) return;

    m_taskId = m_queue->asyncAfter(
        [this, interval, callback]() {
          if (m_running) {
            callback();
            scheduleNext(interval, callback);  // 重新调度
          }
        },
        interval);
  }

  std::shared_ptr<DispatchQueue> m_queue;
  TaskId m_taskId;
  std::atomic<bool> m_running;
};

/**
 * @brief 超时处理器
 */
class TimeoutHandler {
 public:
  TimeoutHandler(std::shared_ptr<DispatchQueue> queue) : m_queue(queue), m_taskId(DispatchQueue::kNullTaskId) {}

  // 设置超时
  void setTimeout(std::chrono::milliseconds timeout, std::function<void()> onTimeout) {
    m_taskId = m_queue->asyncAfter(onTimeout, timeout);
    std::cout << "  Timeout set for " << timeout.count() << "ms\n";
  }

  // 操作完成，取消超时
  void complete() {
    if (m_taskId != DispatchQueue::kNullTaskId) {
      m_queue->cancel(m_taskId);
      m_taskId = DispatchQueue::kNullTaskId;
      std::cout << "  Timeout cancelled (operation completed)\n";
    }
  }

 private:
  std::shared_ptr<DispatchQueue> m_queue;
  TaskId m_taskId;
};

int main() {
  std::cout << "=== Timer Example ===\n\n";

  auto queue = DispatchQueue::create("TimerQueue", kThreadQoSClassNormal);

  // 1. 单次定时器
  std::cout << "1. One-shot timer (300ms):\n";
  {
    Timer timer(queue);
    auto start = std::chrono::steady_clock::now();

    timer.scheduleOnce(300ms, [start]() {
      auto elapsed = std::chrono::steady_clock::now() - start;
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
      std::cout << "  Timer fired after " << ms << "ms\n";
    });

    std::this_thread::sleep_for(500ms);
  }

  // 2. 周期性定时器
  std::cout << "\n2. Repeating timer (100ms interval, 5 times):\n";
  {
    Timer timer(queue);
    std::atomic<int> count{0};
    auto start = std::chrono::steady_clock::now();

    timer.scheduleRepeating(100ms, [&count, start]() {
      auto elapsed = std::chrono::steady_clock::now() - start;
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
      std::cout << "  Tick " << ++count << " at " << ms << "ms\n";
    });

    // 运行5次后取消
    std::this_thread::sleep_for(550ms);
    timer.cancel();
    std::cout << "  Timer cancelled after " << count.load() << " ticks\n";
  }

  // 3. 超时处理
  std::cout << "\n3. Timeout handling:\n";
  {
    TimeoutHandler handler(queue);

    // 场景A：操作在超时前完成
    std::cout << "  Scenario A: Operation completes before timeout\n";
    handler.setTimeout(500ms, []() { std::cout << "  TIMEOUT! Operation took too long.\n"; });

    // 模拟快速完成的操作
    std::this_thread::sleep_for(100ms);
    handler.complete();

    std::this_thread::sleep_for(500ms);  // 等待确认超时未触发
  }

  {
    TimeoutHandler handler(queue);

    // 场景B：操作超时
    std::cout << "\n  Scenario B: Operation times out\n";
    handler.setTimeout(200ms, []() { std::cout << "  TIMEOUT! Operation took too long.\n"; });

    // 模拟慢操作（不调用 complete）
    std::this_thread::sleep_for(300ms);
  }

  // 4. 防抖（Debounce）
  std::cout << "\n4. Debounce example:\n";
  {
    TaskId debounceId = DispatchQueue::kNullTaskId;
    int callCount = 0;

    auto debounce = [&](std::function<void()> fn, std::chrono::milliseconds delay) {
      if (debounceId != DispatchQueue::kNullTaskId) {
        queue->cancel(debounceId);
      }
      debounceId = queue->asyncAfter(fn, delay);
    };

    // 快速连续调用
    for (int i = 0; i < 5; ++i) {
      std::cout << "  Trigger " << i + 1 << "\n";
      debounce([&callCount]() { std::cout << "  Debounced function executed (call #" << ++callCount << ")\n"; }, 100ms);
      std::this_thread::sleep_for(50ms);  // 间隔小于防抖延迟
    }

    std::this_thread::sleep_for(200ms);
    std::cout << "  Total executions: " << callCount << " (should be 1)\n";
  }

  queue->flushAndTeardown();
  std::cout << "\n=== Example completed ===\n";
  return 0;
}
