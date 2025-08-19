# Trading-Order-book
Engineered a high-performance, thread-safe C++17 Limit Order Book using a producer-consumer model. Minimized latency with a lock-free read path (std::atomic, memory semantics) while ensuring data integrity with std::mutex. Managed threads robustly with std::condition_variable and guaranteed memory safety via RAII (std::unique_ptr).
