/**
 * @file thread_safe_data.cpp
 * @brief 线程安全数据访问示例
 *
 * 演示如何使用 DispatchQueue 实现线程安全的数据访问，
 * 无需手动加锁，所有数据操作都在队列中串行执行。
 */

#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "dispatcher/DispatchQueue.h"

using namespace dispatch;

/**
 * @brief 线程安全的键值存储
 *
 * 所有数据操作都通过队列串行化，无需手动加锁
 */
class ThreadSafeCache {
 public:
  ThreadSafeCache() : m_queue(DispatchQueue::create("CacheQueue", kThreadQoSClassNormal)) {}

  ~ThreadSafeCache() { m_queue->flushAndTeardown(); }

  // 异步设置值（不等待完成）
  void set(const std::string& key, int value) {
    m_queue->async([this, key, value]() {
      m_data[key] = value;
      std::cout << "[Cache] Set: " << key << " = " << value << "\n";
    });
  }

  // 同步获取值（等待结果）
  int get(const std::string& key) {
    int result = 0;
    m_queue->sync([this, &key, &result]() {
      auto it = m_data.find(key);
      if (it != m_data.end()) {
        result = it->second;
      }
    });
    return result;
  }

  // 同步检查键是否存在
  bool contains(const std::string& key) {
    bool result = false;
    m_queue->sync([this, &key, &result]() { result = m_data.find(key) != m_data.end(); });
    return result;
  }

  // 异步删除（带回调）
  void remove(const std::string& key, std::function<void(bool)> callback) {
    m_queue->async([this, key, callback]() {
      bool removed = m_data.erase(key) > 0;
      std::cout << "[Cache] Remove: " << key << " -> " << (removed ? "success" : "not found") << "\n";
      if (callback) {
        callback(removed);
      }
    });
  }

  // 同步获取大小
  size_t size() {
    size_t result = 0;
    m_queue->sync([this, &result]() { result = m_data.size(); });
    return result;
  }

 private:
  std::shared_ptr<DispatchQueue> m_queue;
  std::map<std::string, int> m_data;  // 不需要加锁！
};

int main() {
  std::cout << "=== Thread-Safe Data Access Example ===\n\n";

  ThreadSafeCache cache;

  // 从多个线程并发访问缓存
  std::vector<std::thread> threads;

  // 写入线程
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([&cache, i]() {
      std::string key = "key" + std::to_string(i);
      cache.set(key, i * 10);
    });
  }

  // 等待写入完成
  for (auto& t : threads) {
    t.join();
  }
  threads.clear();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // 读取线程
  std::cout << "\nReading values:\n";
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([&cache, i]() {
      std::string key = "key" + std::to_string(i);
      int value = cache.get(key);
      std::cout << "  " << key << " = " << value << "\n";
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  std::cout << "\nCache size: " << cache.size() << "\n";
  std::cout << "Contains 'key2': " << (cache.contains("key2") ? "yes" : "no") << "\n";

  std::cout << "\n=== Example completed ===\n";
  return 0;
}
