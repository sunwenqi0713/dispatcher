<h1 align="center">Dispatch Queue</h1>

<p align="center">
  <b>一个现代、高效的 C++ 任务调度库</b>
</p>

<p align="center">
  提供灵活的异步/同步任务执行、延迟任务、任务取消、线程池等功能。
</p>

---

## ✨ 特性

- 🚀 **异步/同步执行** - 支持阻塞和非阻塞两种任务执行模式
- ⏱️ **延迟任务** - 支持指定延迟时间后执行任务
- ❌ **任务取消** - 通过任务 ID 取消尚未执行的任务
- 🔄 **线程池** - 内置线程池实现，支持真正的并发执行
- 🔒 **线程安全** - 所有接口都是线程安全的
- 📊 **队列监听** - 支持监听队列状态变化（空闲/繁忙）
- 🎯 **屏障同步** - 支持 barrier 操作确保任务顺序
- ⚙️ **QoS 支持** - 支持线程优先级配置
- 📦 **现代 CMake** - 完善的 CMake 构建系统，易于集成

## 📋 要求

- C++17 或更高版本
- CMake 3.16+
- 支持的编译器:
  - GCC 7+
  - Clang 5+
  - MSVC 2017+

## 🔧 构建

### 基本构建

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `dispatcher_BUILD_SHARED` | OFF | 构建共享库 |
| `dispatcher_BUILD_EXAMPLES` | ON | 构建示例程序 |

```bash
# 构建共享库并禁用示例
cmake -Ddispatcher_BUILD_SHARED=ON -Ddispatcher_BUILD_EXAMPLES=OFF ..
```

### 安装

```bash
cmake --install . --prefix /usr/local
```

## 🚀 快速开始

### 集成到项目

**方式 1: CMake FetchContent**

```cmake
include(FetchContent)
FetchContent_Declare(
    dispatcher
    GIT_REPOSITORY https://github.com/your-repo/dispatcher.git
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(dispatcher)

target_link_libraries(your_target PRIVATE dispatcher::dispatcher)
```

**方式 2: 子目录**

```cmake
add_subdirectory(dispatcher)
target_link_libraries(your_target PRIVATE dispatcher::dispatcher)
```

**方式 3: find_package**

```cmake
find_package(dispatcher REQUIRED)
target_link_libraries(your_target PRIVATE dispatcher::dispatcher)
```

### 基本用法

```cpp
#include <dispatcher/DispatchQueue.h>
#include <iostream>

using namespace dispatch;

int main() {
    // 创建调度队列
    auto queue = DispatchQueue::create("MyQueue", kThreadQoSClassNormal);

    // 异步执行任务
    queue->async([]() {
        std::cout << "Hello from background thread!" << std::endl;
    });

    // 同步执行任务（阻塞等待完成）
    int result = 0;
    queue->sync([&result]() {
        result = 42;
    });
    std::cout << "Result: " << result << std::endl;

    // 延迟执行任务
    TaskId taskId = queue->asyncAfter(
        []() { std::cout << "Delayed task!" << std::endl; },
        std::chrono::seconds(1)
    );

    // 取消任务
    queue->cancel(taskId);

    // 清理资源
    queue->flushAndTeardown();
    return 0;
}
```

## 📖 API 参考

### 核心类

#### `DispatchQueue`

任务调度队列的主要接口类。

```cpp
// 创建队列
static std::shared_ptr<DispatchQueue> create(const std::string& name, ThreadQoSClass qosClass);
static std::shared_ptr<DispatchQueue> createThreaded(const std::string& name, ThreadQoSClass qosClass);

// 异步执行（立即返回）
void async(DispatchFunction function);

// 同步执行（阻塞等待）
void sync(const DispatchFunction& function);

// 安全同步（避免死锁）
bool safeSync(const DispatchFunction& function);

// 延迟执行
TaskId asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay);

// 取消任务
void cancel(TaskId taskId);

// 检查是否在队列线程中
bool isCurrent() const;

// 关闭队列
void flushAndTeardown();
```

#### `ThreadPoolDispatchQueue`

线程池调度队列，支持真正的并发执行。

```cpp
// 创建线程池（指定线程数）
static std::shared_ptr<ThreadPoolDispatchQueue> create(const std::string& name, size_t threadCount);

// 创建线程池（使用硬件并发数）
static std::shared_ptr<ThreadPoolDispatchQueue> create(const std::string& name);

// 获取线程数量
size_t threadCount() const;
```

#### `TaskQueue`

底层任务队列，提供更精细的控制。

```cpp
// 入队任务
EnqueuedTask enqueue(DispatchFunction function);
EnqueuedTask enqueue(DispatchFunction function, std::chrono::steady_clock::duration delay);

// 屏障同步
void barrier(const DispatchFunction& function);

// 执行下一个任务
bool runNextTask(std::chrono::steady_clock::time_point maxTime);
bool runNextTask();

// 刷新队列
size_t flush();
size_t flushUpToNow();

// 配置并发
void setMaxConcurrentTasks(size_t maxConcurrentTasks);
```

### 类型定义

```cpp
// 任务 ID 类型
using TaskId = int64_t;

// 任务函数类型
using DispatchFunction = std::function<void()>;

// 线程优先级
enum ThreadQoSClass : uint8_t {
    kThreadQoSClassLowest = 0,   // 最低优先级
    kThreadQoSClassLow = 1,      // 低优先级
    kThreadQoSClassNormal = 2,   // 普通优先级（默认）
    kThreadQoSClassHigh = 3,     // 高优先级
    kThreadQoSClassMax = 4       // 最高优先级
};
```

### 队列监听器

实现 `IQueueListener` 接口监听队列状态：

```cpp
class MyListener : public IQueueListener {
public:
    void onQueueEmpty() override {
        // 队列变空时调用
    }
    
    void onQueueNonEmpty() override {
        // 队列有新任务时调用
    }
};

auto listener = std::make_shared<MyListener>();
queue->setListener(listener);
```

## 📚 示例

### 线程池并发执行

```cpp
#include <dispatcher/ThreadPoolDispatchQueue.h>
#include <atomic>
#include <iostream>

using namespace dispatch;

int main() {
    // 创建 4 线程的线程池
    auto pool = ThreadPoolDispatchQueue::create("worker-pool", 4);
    
    std::atomic<int> completed{0};
    const int taskCount = 100;
    
    // 提交大量任务并发执行
    for (int i = 0; i < taskCount; ++i) {
        pool->async([i, &completed]() {
            // 执行任务...
            completed++;
        });
    }
    
    // 等待所有任务完成
    while (completed < taskCount) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "All " << taskCount << " tasks completed!" << std::endl;
    return 0;
}
```

### 定时器实现

```cpp
#include <dispatcher/DispatchQueue.h>

using namespace dispatch;

class Timer {
public:
    Timer(std::shared_ptr<DispatchQueue> queue) : m_queue(queue) {}
    
    void scheduleOnce(std::chrono::milliseconds delay, std::function<void()> callback) {
        m_taskId = m_queue->asyncAfter(callback, delay);
    }
    
    void cancel() {
        if (m_taskId != DispatchQueue::kNullTaskId) {
            m_queue->cancel(m_taskId);
            m_taskId = DispatchQueue::kNullTaskId;
        }
    }
    
private:
    std::shared_ptr<DispatchQueue> m_queue;
    TaskId m_taskId = DispatchQueue::kNullTaskId;
};
```

### 防抖（Debounce）实现

```cpp
TaskId debounceId = DispatchQueue::kNullTaskId;

auto debounce = [&](std::function<void()> fn, std::chrono::milliseconds delay) {
    if (debounceId != DispatchQueue::kNullTaskId) {
        queue->cancel(debounceId);
    }
    debounceId = queue->asyncAfter(fn, delay);
};

// 快速连续调用，只有最后一次会执行
for (int i = 0; i < 5; ++i) {
    debounce([]() { std::cout << "Debounced!" << std::endl; }, 
             std::chrono::milliseconds(100));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
```

### 全局主队列

```cpp
// 设置主队列
auto mainQueue = DispatchQueue::create("Main", kThreadQoSClassHigh);
DispatchQueue::setMain(mainQueue);

// 在任何地方使用主队列
DispatchQueue::getMain()->async([]() {
    // 在主队列中执行
});
```

## ⚠️ 注意事项

### 避免死锁

不要在队列的工作线程中调用 `sync()`，这会导致死锁：

```cpp
// ❌ 错误：可能导致死锁
queue->async([&queue]() {
    queue->sync([]() { /* ... */ });  // 死锁！
});

// ✅ 正确：使用 safeSync
queue->async([&queue]() {
    queue->safeSync([]() { /* ... */ });  // 安全
});
```

### ThreadedDispatchQueue vs ThreadPoolDispatchQueue

| 特性 | ThreadedDispatchQueue | ThreadPoolDispatchQueue |
|------|----------------------|------------------------|
| 执行模式 | 串行（单线程） | 并发（多线程） |
| 任务顺序 | 严格保证 FIFO | 不保证顺序 |
| 适用场景 | 需要顺序执行的任务 | CPU/IO 密集型并行任务 |
| 资源消耗 | 低（单线程） | 较高（多线程） |

## 📁 项目结构

```
dispatcher/
├── include/dispatcher/      # 头文件
│   ├── Types.h              # 基础类型定义
│   ├── IDispatchQueue.h     # 队列接口
│   ├── IQueueListener.h     # 监听器接口
│   ├── DispatchQueue.h      # 主队列类
│   ├── TaskQueue.h          # 任务队列
│   ├── ThreadedDispatchQueue.h     # 线程化队列
│   └── ThreadPoolDispatchQueue.h   # 线程池队列
├── src/                     # 源文件
├── examples/                # 示例程序
│   ├── dispatch_queue.cpp   # 基本用法示例
│   ├── thread_pool_example.cpp  # 线程池示例
│   ├── timer_example.cpp    # 定时器示例
│   └── ...
├── cmake/                   # CMake 配置
└── CMakeLists.txt           # 构建配置
```

## 📄 License

MIT License
