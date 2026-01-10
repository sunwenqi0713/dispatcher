/**
 * @file dispatch_queue.cpp
 * @brief Dispatcher 库使用示例
 *
 * 本示例演示了调度器库的主要功能：
 * 1. 创建调度队列
 * 2. 异步/同步任务执行
 * 3. 延迟任务
 * 4. 任务取消
 * 5. 队列监听器
 * 6. 并发任务控制
 */

#include <atomic>
#include <chrono>
#include <iostream>

#include "dispatcher/DispatchQueue.h"
#include "dispatcher/ThreadedDispatchQueue.h"

using namespace dispatch;
using namespace std::chrono_literals;

/**
 * @brief 自定义队列监听器
 *
 * 实现 IQueueListener 接口，监控队列的空闲/繁忙状态。
 */
class MyQueueListener : public IQueueListener {
 public:
  void onQueueEmpty() override { std::cout << "[Listener] Queue is empty\n"; }

  void onQueueNonEmpty() override { std::cout << "[Listener] Queue has new tasks\n"; }
};

int main() {
  std::cout << "=== Dispatcher Usage Example ===\n\n";

  // ========================================================================
  // 1. 创建 DispatchQueue
  // ========================================================================
  std::cout << "1. Create DispatchQueue\n";

  // 使用工厂方法创建队列，指定名称和优先级
  auto queue = DispatchQueue::create("MyQueue", kThreadQoSClassNormal);
  std::cout << "   Queue created\n\n";

  // ========================================================================
  // 2. 设置队列监听器
  // ========================================================================
  std::cout << "2. Set queue listener\n";

  // 监听器可以用于显示加载指示器、触发清理操作等
  auto listener = std::make_shared<MyQueueListener>();
  queue->setListener(listener);
  std::cout << "\n";

  // ========================================================================
  // 3. 异步执行任务
  // ========================================================================
  std::cout << "3. Async task execution (async)\n";

  std::atomic<int> counter{0};

  // async() 立即返回，任务在后台线程执行
  for (int i = 0; i < 3; ++i) {
    queue->async([i, &counter]() {
      std::cout << "   Task " << i << " executing (thread: " << std::this_thread::get_id() << ")\n";
      std::this_thread::sleep_for(100ms);
      counter++;
    });
  }

  // 等待任务完成
  std::this_thread::sleep_for(500ms);
  std::cout << "   Completed tasks: " << counter.load() << "\n\n";

  // ========================================================================
  // 4. 同步执行任务
  // ========================================================================
  std::cout << "4. Sync task execution (sync)\n";

  int result = 0;

  // sync() 会阻塞当前线程，直到任务执行完成
  // 适用于需要等待结果的场景
  queue->sync([&result]() {
    std::cout << "   Sync task executing...\n";
    result = 42;
  });
  std::cout << "   Sync task result: " << result << "\n\n";

  // ========================================================================
  // 5. 延迟执行任务
  // ========================================================================
  std::cout << "5. Delayed task execution (asyncAfter)\n";

  auto startTime = std::chrono::steady_clock::now();

  // asyncAfter() 返回任务ID，可用于取消任务
  TaskId taskId = queue->asyncAfter(
      [startTime]() {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        std::cout << "   Delayed task executed (delayed " << ms << " ms)\n";
      },
      200ms);  // Execute after 200ms

  std::cout << "   Scheduled delayed task, ID: " << taskId << "\n";
  std::this_thread::sleep_for(300ms);
  std::cout << "\n";

  // ========================================================================
  // 6. 取消任务
  // ========================================================================
  std::cout << "6. Cancel task (cancel)\n";

  // 安排一个延迟任务
  TaskId cancelTaskId = queue->asyncAfter([]() { std::cout << "   This task should NOT execute!\n"; }, 500ms);

  std::cout << "   Scheduled task ID: " << cancelTaskId << "\n";

  // 在任务执行前取消它
  queue->cancel(cancelTaskId);
  std::cout << "   Cancelled task ID: " << cancelTaskId << "\n";

  std::this_thread::sleep_for(600ms);
  std::cout << "   (Task was successfully cancelled, did not execute)\n\n";

  // ========================================================================
  // 7. 使用 safeSync
  // ========================================================================
  std::cout << "7. Use safeSync\n";

  // safeSync 在已经位于队列线程时直接执行，避免死锁
  // 这是比 sync 更安全的选择
  queue->safeSync([]() { std::cout << "   safeSync task executed\n"; });
  std::cout << "\n";

  // ========================================================================
  // 8. 并发任务示例
  // ========================================================================
  std::cout << "8. Concurrent tasks example (TaskQueue)\n";

  // 直接使用 TaskQueue 可以更细粒度地控制并发
  auto taskQueue = std::make_shared<TaskQueue>();
  taskQueue->setMaxConcurrentTasks(3);  // 允许最多3个任务并发

  std::atomic<int> runningTasks{0};
  std::atomic<int> maxRunning{0};

  // 提交5个任务
  for (int i = 0; i < 5; ++i) {
    taskQueue->enqueue([i, &runningTasks, &maxRunning]() {
      int current = ++runningTasks;

      // 记录最大并发数
      int expected = maxRunning.load();
      while (current > expected && !maxRunning.compare_exchange_weak(expected, current)) {
      }

      std::cout << "   Concurrent task " << i << " started (current concurrency: " << current << ")\n";
      std::this_thread::sleep_for(100ms);
      --runningTasks;
    });
  }

  // 创建多个工作线程来执行任务
  std::thread worker1([&taskQueue]() {
    while (!taskQueue->isDisposed()) {
      if (!taskQueue->runNextTask(std::chrono::steady_clock::now() + 50ms)) {
        break;
      }
    }
  });

  std::thread worker2([&taskQueue]() {
    while (!taskQueue->isDisposed()) {
      if (!taskQueue->runNextTask(std::chrono::steady_clock::now() + 50ms)) {
        break;
      }
    }
  });

  std::thread worker3([&taskQueue]() {
    while (!taskQueue->isDisposed()) {
      if (!taskQueue->runNextTask(std::chrono::steady_clock::now() + 50ms)) {
        break;
      }
    }
  });

  // 等待任务完成
  std::this_thread::sleep_for(400ms);

  // 销毁队列并等待线程结束
  taskQueue->dispose();
  worker1.join();
  worker2.join();
  worker3.join();

  std::cout << "   Max concurrent tasks: " << maxRunning.load() << "\n\n";

  // ========================================================================
  // 9. 清理资源
  // ========================================================================
  std::cout << "9. Cleanup\n";

  // flushAndTeardown 会等待所有任务完成，然后销毁队列
  queue->flushAndTeardown();
  std::cout << "   Queue destroyed\n\n";

  std::cout << "=== Example completed ===\n";
  return 0;
}
