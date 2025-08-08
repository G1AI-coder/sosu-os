SOSU OS Kernel v3.0

> An operating system and programming language built from scratch by a 11-year-old programmer.

SOSU OS is a fully custom OS kernel written in pure C. It includes:
- A built-in programming language with C#/C++/Rust-inspired syntax
- Virtual machine with typed stack and memory management
- Garbage collector (GC)
- Just-In-Time (JIT) compiler
- Performance profiler
- System calls (files, network, threads, crypto)
- Modular architecture

This is not an emulator. This is a complete system from the ground up — made for learning, experimenting, and pushing limits.

---

🧩 Features

- **Data types**: `int8` to `int64`, `float32/64`, `bool`, `pointer`, `struct`, `enum`
- **Memory**: 2MB virtual memory, heap, stack, manual & GC allocation
- **Concurrency**: `thread`, `async`, `lock`
- **Safety**: `assert`, `try`, `catch`, `unsafe` blocks
- **Tools**: `benchmark`, `gc collect`, `trace`, `debug`
- **Extensibility**: Modules, imports, JIT, inline assembly

---

🚀 How to Run

gcc kernel.c -o sosu
./sosu kernel.sosu