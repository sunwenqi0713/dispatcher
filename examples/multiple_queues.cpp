/**
 * @file multiple_queues.cpp
 * @brief 多队列示例
 *
 * 演示如何使用多个队列处理不同类型的任务，
 * 以及队列间的协作
 */

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "dispatcher/DispatchQueue.h"

using namespace dispatch;
using namespace std::chrono_literals;

/**
 * @brief 模拟的网络服务
 */
class NetworkService {
 public:
  NetworkService()
      : m_networkQueue(DispatchQueue::create("NetworkQueue", kThreadQoSClassNormal)),
        m_callbackQueue(DispatchQueue::create("CallbackQueue", kThreadQoSClassHigh)) {}

  ~NetworkService() {
    m_networkQueue->flushAndTeardown();
    m_callbackQueue->flushAndTeardown();
  }

  // 异步获取数据
  void fetchData(const std::string& url, std::function<void(std::string)> callback) {
    m_networkQueue->async([this, url, callback]() {
      // 在网络队列中执行网络请求
      std::cout << "  [Network] Fetching: " << url << "\n";
      std::this_thread::sleep_for(200ms);  // 模拟网络延迟

      std::string result = "Data from " + url;

      // 在回调队列中执行回调（避免阻塞网络队列）
      m_callbackQueue->async([callback, result]() {
        std::cout << "  [Callback] Processing result\n";
        callback(result);
      });
    });
  }

 private:
  std::shared_ptr<DispatchQueue> m_networkQueue;
  std::shared_ptr<DispatchQueue> m_callbackQueue;
};

/**
 * @brief 多层处理流水线
 */
class ProcessingPipeline {
 public:
  ProcessingPipeline()
      : m_inputQueue(DispatchQueue::create("InputQueue", kThreadQoSClassNormal)),
        m_processQueue(DispatchQueue::create("ProcessQueue", kThreadQoSClassNormal)),
        m_outputQueue(DispatchQueue::create("OutputQueue", kThreadQoSClassNormal)) {}

  ~ProcessingPipeline() {
    m_inputQueue->flushAndTeardown();
    m_processQueue->flushAndTeardown();
    m_outputQueue->flushAndTeardown();
  }

  void submit(int data) {
    // 阶段1：输入处理
    m_inputQueue->async([this, data]() {
      std::cout << "  [Input] Received: " << data << "\n";
      int validated = data;  // 模拟验证

      // 阶段2：核心处理
      m_processQueue->async([this, validated]() {
        std::cout << "  [Process] Processing: " << validated << "\n";
        std::this_thread::sleep_for(50ms);  // 模拟处理
        int result = validated * 2;

        // 阶段3：输出
        m_outputQueue->async([result]() { std::cout << "  [Output] Result: " << result << "\n"; });
      });
    });
  }

  void waitForCompletion() {
    // 按顺序等待每个队列清空
    m_inputQueue->sync([]() {});
    m_processQueue->sync([]() {});
    m_outputQueue->sync([]() {});
  }

 private:
  std::shared_ptr<DispatchQueue> m_inputQueue;
  std::shared_ptr<DispatchQueue> m_processQueue;
  std::shared_ptr<DispatchQueue> m_outputQueue;
};

int main() {
  std::cout << "=== Multiple Queues Example ===\n\n";

  // 1. 网络服务示例
  std::cout << "1. Network Service (separate queues for network and callbacks):\n";
  {
    NetworkService service;
    std::atomic<int> completed{0};

    std::vector<std::string> urls = {"https://api.example.com/users", "https://api.example.com/posts",
                                     "https://api.example.com/comments"};

    for (const auto& url : urls) {
      service.fetchData(url, [&completed](const std::string& result) {
        std::cout << "  [Main] Got: " << result << "\n";
        completed++;
      });
    }

    // 等待所有请求完成
    while (completed < 3) {
      std::this_thread::sleep_for(50ms);
    }
    std::this_thread::sleep_for(100ms);
  }

  // 2. 处理流水线示例
  std::cout << "\n2. Processing Pipeline (input -> process -> output):\n";
  {
    ProcessingPipeline pipeline;

    for (int i = 1; i <= 3; ++i) {
      pipeline.submit(i * 10);
    }

    pipeline.waitForCompletion();
  }

  // 3. 主队列示例
  std::cout << "\n3. Main Queue Pattern:\n";
  {
    auto mainQueue = DispatchQueue::create("MainQueue", kThreadQoSClassHigh);
    auto workerQueue = DispatchQueue::create("WorkerQueue", kThreadQoSClassLow);

    // 设置主队列
    DispatchQueue::setMain(mainQueue);

    workerQueue->async([mainQueue]() {
      std::cout << "  [Worker] Starting heavy computation...\n";
      std::this_thread::sleep_for(100ms);  // 模拟耗时操作

      int result = 42;

      // 回到主队列更新UI
      DispatchQueue::getMain()->async(
          [result]() { std::cout << "  [Main] Update UI with result: " << result << "\n"; });
    });

    std::this_thread::sleep_for(200ms);
    mainQueue->flushAndTeardown();
    workerQueue->flushAndTeardown();
  }

  std::cout << "\n=== Example completed ===\n";
  return 0;
}
