############################################################
# dispatcher examples
############################################################

# Dispatch queue example
add_executable(dispatch_queue dispatch_queue.cpp)
target_link_libraries(dispatch_queue PRIVATE dispatcher::dispatcher)

# Multiple queues example
add_executable(multiple_queues multiple_queues.cpp)
target_link_libraries(multiple_queues PRIVATE dispatcher::dispatcher)

# Async operations example
add_executable(async_operations async_operations.cpp)
target_link_libraries(async_operations PRIVATE dispatcher::dispatcher)

# Producer consumer example
add_executable(producer_consumer producer_consumer.cpp)
target_link_libraries(producer_consumer PRIVATE dispatcher::dispatcher)

# Thread safe data example
add_executable(thread_safe_data thread_safe_data.cpp)
target_link_libraries(thread_safe_data PRIVATE dispatcher::dispatcher)

# Timer example
add_executable(timer_example timer_example.cpp)
target_link_libraries(timer_example PRIVATE dispatcher::dispatcher)

# Thread pool example
add_executable(thread_pool_example thread_pool_example.cpp)
target_link_libraries(thread_pool_example PRIVATE dispatcher::dispatcher)