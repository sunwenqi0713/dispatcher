/**
 * @file producer_consumer.cpp
 * @brief 生产者-消费者模式示例
 *
 * 演示如何使用 DispatchQueue 实现生产者-消费者模式
 */

#include <atomic>
#include <chrono>
#include <iostream>
#include <random>

#include "dispatcher/DispatchQueue.h"

using namespace dispatch;
using namespace std::chrono_literals;

class DataProcessor {
 public:
  DataProcessor() : m_queue(DispatchQueue::create("ProcessorQueue", kThreadQoSClassNormal)), m_processedCount(0) {}

  ~DataProcessor() { m_queue->flushAndTeardown(); }

  // 生产者调用：提交数据进行处理
  void submitData(int data) {
    m_queue->async([this, data]() { processData(data); });
  }

  // 获取已处理数量
  int getProcessedCount() const { return m_processedCount.load(); }

  // 等待所有任务完成
  void waitForCompletion() {
    m_queue->sync([]() {
      // 空任务，只是为了等待之前的任务完成
    });
  }

 private:
  void processData(int data) {
    // 模拟数据处理
    std::this_thread::sleep_for(10ms);
    std::cout << "  Processed data: " << data << " (total: " << ++m_processedCount << ")\n";
  }

  std::shared_ptr<DispatchQueue> m_queue;
  std::atomic<int> m_processedCount;
};

int main() {
  std::cout << "=== Producer-Consumer Example ===\n\n";

  DataProcessor processor;

  // 创建多个生产者线程
  std::cout << "Starting producers...\n";
  std::vector<std::thread> producers;

  for (int producerId = 0; producerId < 3; ++producerId) {
    producers.emplace_back([&processor, producerId]() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> dis(1, 100);

      for (int i = 0; i < 5; ++i) {
        int data = producerId * 100 + dis(gen);
        std::cout << "Producer " << producerId << " submitting: " << data << "\n";
        processor.submitData(data);
        std::this_thread::sleep_for(5ms);
      }
    });
  }

  // 等待生产者完成
  for (auto& t : producers) {
    t.join();
  }

  std::cout << "\nAll producers finished, waiting for processing...\n";

  // 等待所有数据处理完成
  processor.waitForCompletion();

  std::cout << "\nTotal processed: " << processor.getProcessedCount() << "\n";
  std::cout << "\n=== Example completed ===\n";
  return 0;
}
