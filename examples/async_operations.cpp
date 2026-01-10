/**
 * @file async_operations.cpp
 * @brief 异步操作模式示例
 *
 * 演示各种异步编程模式
 */

#include <chrono>
#include <future>
#include <iostream>
#include <optional>

#include "dispatcher/DispatchQueue.h"

using namespace dispatch;
using namespace std::chrono_literals;

/**
 * @brief 异步任务封装
 */
template <typename T>
class AsyncTask {
 public:
  AsyncTask(std::shared_ptr<DispatchQueue> queue) : m_queue(queue) {}

  // 执行异步任务并返回 future
  std::future<T> execute(std::function<T()> task) {
    auto promise = std::make_shared<std::promise<T>>();
    auto future = promise->get_future();

    m_queue->async([promise, task]() {
      try {
        T result = task();
        promise->set_value(result);
      } catch (...) {
        promise->set_exception(std::current_exception());
      }
    });

    return future;
  }

 private:
  std::shared_ptr<DispatchQueue> m_queue;
};

/**
 * @brief 链式异步操作
 */
class AsyncChain {
 public:
  AsyncChain(std::shared_ptr<DispatchQueue> queue) : m_queue(queue) {}

  // 开始链式操作
  AsyncChain& then(std::function<void()> task) {
    m_queue->async(task);
    return *this;
  }

  // 延迟后执行
  AsyncChain& delay(std::chrono::milliseconds ms) {
    m_queue->asyncAfter([]() {}, ms);
    return *this;
  }

  // 等待所有操作完成
  void wait() {
    m_queue->sync([]() {});
  }

 private:
  std::shared_ptr<DispatchQueue> m_queue;
};

/**
 * @brief 批量操作
 */
class BatchProcessor {
 public:
  BatchProcessor(std::shared_ptr<DispatchQueue> queue) : m_queue(queue) {}

  // 批量提交任务，等待全部完成
  void processBatch(const std::vector<std::function<void()>>& tasks) {
    std::atomic<size_t> completed{0};
    size_t total = tasks.size();

    for (const auto& task : tasks) {
      m_queue->async([task, &completed]() {
        task();
        completed++;
      });
    }

    // 等待所有任务完成
    while (completed < total) {
      std::this_thread::sleep_for(10ms);
    }
  }

  // 并行处理数组
  template <typename T, typename Func>
  std::vector<T> map(const std::vector<T>& input, Func func) {
    std::vector<T> results(input.size());
    std::atomic<size_t> completed{0};

    for (size_t i = 0; i < input.size(); ++i) {
      m_queue->async([&input, &results, &completed, i, func]() {
        results[i] = func(input[i]);
        completed++;
      });
    }

    while (completed < input.size()) {
      std::this_thread::sleep_for(1ms);
    }

    return results;
  }

 private:
  std::shared_ptr<DispatchQueue> m_queue;
};

int main() {
  std::cout << "=== Async Operations Example ===\n\n";

  auto queue = DispatchQueue::create("AsyncQueue", kThreadQoSClassNormal);

  // 1. Future 模式
  std::cout << "1. Future Pattern:\n";
  {
    AsyncTask<int> task(queue);

    auto future = task.execute([]() {
      std::cout << "  Computing...\n";
      std::this_thread::sleep_for(100ms);
      return 42;
    });

    std::cout << "  Waiting for result...\n";
    int result = future.get();
    std::cout << "  Result: " << result << "\n";
  }

  // 2. 链式调用
  std::cout << "\n2. Chain Pattern:\n";
  {
    AsyncChain chain(queue);

    chain.then([]() { std::cout << "  Step 1: Initialize\n"; })
        .then([]() { std::cout << "  Step 2: Load data\n"; })
        .then([]() { std::cout << "  Step 3: Process\n"; })
        .then([]() { std::cout << "  Step 4: Save\n"; })
        .wait();

    std::cout << "  Chain completed\n";
  }

  // 3. 批量处理
  std::cout << "\n3. Batch Processing:\n";
  {
    BatchProcessor processor(queue);

    std::vector<std::function<void()>> tasks;
    for (int i = 0; i < 5; ++i) {
      tasks.push_back([i]() {
        std::cout << "  Processing item " << i << "\n";
        std::this_thread::sleep_for(20ms);
      });
    }

    std::cout << "  Starting batch...\n";
    processor.processBatch(tasks);
    std::cout << "  Batch completed\n";
  }

  // 4. Map 操作
  std::cout << "\n4. Parallel Map:\n";
  {
    BatchProcessor processor(queue);

    std::vector<int> input = {1, 2, 3, 4, 5};
    std::cout << "  Input: ";
    for (int v : input) std::cout << v << " ";
    std::cout << "\n";

    auto results = processor.map<int>(input, [](int x) { return x * x; });

    std::cout << "  Output (squared): ";
    for (int v : results) std::cout << v << " ";
    std::cout << "\n";
  }

  // 5. 错误处理
  std::cout << "\n5. Error Handling:\n";
  {
    AsyncTask<int> task(queue);

    auto future = task.execute([]() -> int {
      std::cout << "  Executing task that might fail...\n";
      throw std::runtime_error("Something went wrong!");
      return 0;
    });

    try {
      int result = future.get();
      std::cout << "  Result: " << result << "\n";
    } catch (const std::exception& e) {
      std::cout << "  Caught exception: " << e.what() << "\n";
    }
  }

  // 6. 超时获取
  std::cout << "\n6. Timeout Pattern:\n";
  {
    AsyncTask<int> task(queue);

    auto future = task.execute([]() {
      std::this_thread::sleep_for(500ms);  // 慢任务
      return 100;
    });

    // 尝试等待100ms
    auto status = future.wait_for(100ms);
    if (status == std::future_status::timeout) {
      std::cout << "  Task timed out (still running)\n";
    } else {
      std::cout << "  Task completed: " << future.get() << "\n";
    }

    // 等待完成
    std::cout << "  Waiting for completion... Result: " << future.get() << "\n";
  }

  queue->flushAndTeardown();
  std::cout << "\n=== Example completed ===\n";
  return 0;
}
