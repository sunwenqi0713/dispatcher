/**
 * @file thread_pool_example.cpp
 * @brief 线程池调度队列使用示例
 *
 * 演示 ThreadPoolDispatchQueue 的并发执行特性
 */

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#include "dispatcher/ThreadPoolDispatchQueue.h"

using namespace dispatch;

// 用于同步控制台输出
std::mutex g_printMutex;

void safePrint(const std::string& msg) {
  std::lock_guard<std::mutex> lock(g_printMutex);
  std::cout << msg << std::flush;
}

std::string getTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
  return ss.str();
}

// ========================================================================
// Example 1: Basic concurrent execution
// ========================================================================
void example1_basicConcurrency() {
  std::cout << "\n=== Example 1: Basic Concurrent Execution ===\n";

  // Create a thread pool with 4 workers
  auto pool = ThreadPoolDispatchQueue::create("worker-pool", 4);
  std::cout << "Created thread pool with " << pool->threadCount() << " threads\n\n";

  std::atomic<int> completed{0};
  const int taskCount = 8;

  // Submit tasks that simulate work
  for (int i = 0; i < taskCount; ++i) {
    pool->async([i, &completed]() {
      std::stringstream ss;
      ss << "[" << getTimestamp() << "] Task " << i << " started on thread " << std::this_thread::get_id() << "\n";
      safePrint(ss.str());

      // Simulate work (500ms)
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      ss.str("");
      ss << "[" << getTimestamp() << "] Task " << i << " completed\n";
      safePrint(ss.str());

      completed++;
    });
  }

  // Wait for all tasks to complete
  while (completed < taskCount) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  std::cout << "\nAll " << taskCount << " tasks completed.\n";
  std::cout << "With 4 threads and 500ms tasks, 8 tasks should complete in ~1 second.\n";
  std::cout << "(Compare to serial: 8 * 500ms = 4 seconds)\n";
}

// ========================================================================
// Example 2: CPU-bound parallel processing
// ========================================================================
void example2_parallelProcessing() {
  std::cout << "\n=== Example 2: Parallel Processing ===\n";

  auto pool = ThreadPoolDispatchQueue::create("compute-pool", 4);

  // Compute sum in parallel
  const int rangeCount = 4;
  const int numbersPerRange = 10000000;
  std::atomic<long long> partialSums[rangeCount];
  std::atomic<int> completed{0};

  for (int i = 0; i < rangeCount; ++i) {
    partialSums[i] = 0;
  }

  auto startTime = std::chrono::steady_clock::now();

  for (int range = 0; range < rangeCount; ++range) {
    pool->async([range, numbersPerRange, &partialSums, &completed]() {
      long long sum = 0;
      int start = range * numbersPerRange;
      int end = start + numbersPerRange;

      for (int i = start; i < end; ++i) {
        sum += i;
      }

      partialSums[range] = sum;
      completed++;

      std::stringstream ss;
      ss << "Range " << range << " computed: sum = " << sum << "\n";
      safePrint(ss.str());
    });
  }

  // Wait for completion
  while (completed < rangeCount) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Aggregate results
  long long totalSum = 0;
  for (int i = 0; i < rangeCount; ++i) {
    totalSum += partialSums[i];
  }

  auto endTime = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

  std::cout << "Total sum of 0 to " << (rangeCount * numbersPerRange - 1) << " = " << totalSum << "\n";
  std::cout << "Parallel computation time: " << duration << "ms\n";
}

// ========================================================================
// Example 3: Comparison with serial execution
// ========================================================================
void example3_serialVsParallel() {
  std::cout << "\n=== Example 3: Serial vs Parallel Comparison ===\n";

  const int taskCount = 8;
  const int taskDurationMs = 100;

  // Serial execution (1 thread)
  {
    auto serialQueue = ThreadPoolDispatchQueue::create("serial", 1);
    std::atomic<int> completed{0};

    auto startTime = std::chrono::steady_clock::now();

    for (int i = 0; i < taskCount; ++i) {
      serialQueue->async([&completed, taskDurationMs]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(taskDurationMs));
        completed++;
      });
    }

    while (completed < taskCount) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "Serial (1 thread): " << duration << "ms "
              << "(expected: ~" << taskCount * taskDurationMs << "ms)\n";
  }

  // Parallel execution (4 threads)
  {
    auto parallelPool = ThreadPoolDispatchQueue::create("parallel", 4);
    std::atomic<int> completed{0};

    auto startTime = std::chrono::steady_clock::now();

    for (int i = 0; i < taskCount; ++i) {
      parallelPool->async([&completed, taskDurationMs]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(taskDurationMs));
        completed++;
      });
    }

    while (completed < taskCount) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "Parallel (4 threads): " << duration << "ms "
              << "(expected: ~" << (taskCount / 4) * taskDurationMs << "ms)\n";
  }
}

// ========================================================================
// Example 4: Thread pool with delayed tasks
// ========================================================================
void example4_delayedTasks() {
  std::cout << "\n=== Example 4: Delayed Tasks in Thread Pool ===\n";

  auto pool = ThreadPoolDispatchQueue::create("delayed-pool", 4);
  std::atomic<int> completed{0};

  auto startTime = std::chrono::steady_clock::now();

  // Submit tasks with different delays
  for (int i = 0; i < 4; ++i) {
    auto delay = std::chrono::milliseconds(i * 200);
    pool->asyncAfter(
        [i, startTime, &completed]() {
          auto elapsed =
              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime)
                  .count();

          std::stringstream ss;
          ss << "Task " << i << " executed at " << elapsed << "ms (delay: " << i * 200 << "ms)\n";
          safePrint(ss.str());

          completed++;
        },
        delay);
  }

  // Wait for completion
  while (completed < 4) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  std::cout << "All delayed tasks completed.\n";
}

// ========================================================================
// Example 5: Sync operation in thread pool
// ========================================================================
void example5_syncOperation() {
  std::cout << "\n=== Example 5: Sync Operation (Barrier) ===\n";

  auto pool = ThreadPoolDispatchQueue::create("sync-pool", 4);
  std::atomic<int> counter{0};

  // Submit some async tasks
  for (int i = 0; i < 10; ++i) {
    pool->async([i, &counter]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      counter++;

      std::stringstream ss;
      ss << "Async task " << i << " completed\n";
      safePrint(ss.str());
    });
  }

  // Sync waits for all previous tasks to complete
  std::cout << "Calling sync() - will wait for all previous tasks...\n";
  pool->sync([&counter]() {
    std::cout << "==> Sync task executing. Counter = " << counter << "\n";
    std::cout << "==> No other task is running during this sync block.\n";
  });

  std::cout << "After sync(), counter = " << counter << "\n";
}

int main() {
  std::cout << "ThreadPoolDispatchQueue Examples\n";
  std::cout << "================================\n";
  std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n";

  example1_basicConcurrency();
  example2_parallelProcessing();
  example3_serialVsParallel();
  example4_delayedTasks();
  example5_syncOperation();

  std::cout << "\n=== All examples completed ===\n";
  return 0;
}
