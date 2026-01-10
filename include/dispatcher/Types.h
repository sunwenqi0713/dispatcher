/**
 * @file Types.h
 * @brief 调度器库的基础类型定义
 *
 * 定义了整个调度器库使用的通用类型，包括任务ID和任务函数类型。
 */

#pragma once

#include <cstdint>
#include <functional>

namespace dispatch {

/**
 * @brief 任务ID类型
 *
 * 用于唯一标识每个被提交到队列的任务。
 * 可以通过此ID取消尚未执行的任务。
 */
using TaskId = int64_t;

/**
 * @brief 调度函数类型
 *
 * 无参数、无返回值的可调用对象，代表一个待执行的任务。
 * 可以是 lambda、函数指针、std::bind 结果等。
 */
using DispatchFunction = std::function<void()>;

}  // namespace dispatch
