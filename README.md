# SimpleVM - 栈式虚拟机 DLL

SimpleVM 是一个轻量级的栈式虚拟机，实现为动态链接库（DLL / SO / DYLIB），可作为自制编程语言的后端。它支持整数算术运算、内存存储/加载、条件跳转等核心功能，提供简洁的 C 语言导出接口，可被 C/C++、Python、C# 等多种语言调用。

## 特性

- 栈式架构，操作数栈深度 1024
- 全局内存 4096 个槽位（64 位整数）
- 小端序字节码，与 x86/ARM 原生对齐
- 15 条精简指令，易于扩展
- 完整的错误处理机制
- 无外部依赖（仅标准库）
- 跨平台支持：Windows / Linux / macOS

## API 参考

所有导出函数均以 `VM_API` 修饰，使用 `extern "C"` 链接约定。

| 函数 | 说明 |
|------|------|
| `VMHandle vm_create(void)` | 创建虚拟机实例，返回句柄 |
| `void vm_destroy(VMHandle vm)` | 销毁虚拟机，释放资源 |
| `void vm_reset(VMHandle vm)` | 重置执行状态（栈、IP），保留全局内存 |
| `VMError vm_load_program(VMHandle vm, const uint8_t* bytecode, size_t size)` | 加载字节码程序 |
| `VMError vm_run(VMHandle vm)` | 执行已加载的程序 |
| `int64_t vm_get_result(VMHandle vm)` | 获取执行后栈顶的值（计算结果） |
| `VMError vm_set_global(VMHandle vm, uint32_t address, int64_t value)` | 设置全局内存的值 |
| `int64_t vm_get_global(VMHandle vm, uint32_t address)` | 读取全局内存的值 |
| `const char* vm_get_error_string(VMHandle vm)` | 获取最后发生的错误描述 |
| `void vm_write_int64_le(uint8_t* dest, int64_t value)` | 辅助函数：以小端序写入 64 位整数 |
| `int64_t vm_read_int64_le(const uint8_t* src)` | 辅助函数：以小端序读取 64 位整数 |

### 错误码

| 枚举值 | 含义 |
|--------|------|
| `VM_OK` | 无错误 |
| `VM_ERR_STACK_OVERFLOW` | 栈溢出（超过 1024 元素） |
| `VM_ERR_STACK_UNDERFLOW` | 栈下溢（空栈弹出） |
| `VM_ERR_INVALID_INSTRUCTION` | 非法操作码或不完整指令 |
| `VM_ERR_MEMORY_OUT_OF_BOUNDS` | 全局内存地址超出 0～4095 |
| `VM_ERR_DIVISION_BY_ZERO` | 除零错误 |
| `VM_ERR_PROGRAM_NOT_LOADED` | 未加载程序即执行 |
| `VM_ERR_BAD_JUMP_TARGET` | 跳转目标超出代码段范围 |
| `VM_ERR_UNKNOWN` | 未知错误 |

## 指令集

| 操作码 | 助记符 | 操作数 | 说明 |
|--------|--------|--------|------|
| 0x01 | `PUSH` | int64 (8字节) | 将立即数压入栈 |
| 0x02 | `POP` | 无 | 弹出栈顶值并丢弃 |
| 0x03 | `ADD` | 无 | 弹出 a, b（b 为栈顶），推入 a+b |
| 0x04 | `SUB` | 无 | 弹出 a, b，推入 a-b |
| 0x05 | `MUL` | 无 | 弹出 a, b，推入 a*b |
| 0x06 | `DIV` | 无 | 弹出 a, b，推入 a/b（整数除法） |
| 0x07 | `STORE` | uint16 (2字节) | 弹出栈顶值，存入全局内存[addr] |
| 0x08 | `LOAD` | uint16 (2字节) | 从全局内存[addr]加载值入栈 |
| 0x09 | `JMP` | int16 (2字节) | 无条件跳转（相对偏移） |
| 0x0A | `JMP_IF` | int16 (2字节) | 弹出条件，非零则相对跳转 |
| 0x0B | `HALT` | 无 | 停止执行 |
| 0x0C | `PRINT` | 无 | 弹出值并打印到 stderr（调试用） |
| 0x0D | `DUP` | 无 | 复制栈顶元素 |
| 0x0E | `SWAP` | 无 | 交换栈顶两个元素 |
| 0x00 | `NOP` | 无 | 空操作 |

> **注意**：立即数均采用**小端序**编码。例如 `PUSH 1000`（0x3E8）的字节序列为 `01 E8 03 00 00 00 00 00 00`。

## 编译

### 前置条件

- C++11 或更高版本的编译器
- CMake（可选）或直接使用命令行
#### Windows (MinGW-w64)

```bash
g++ -shared -O2 -DVM_DLL_EXPORT -o simple_vm.dll vm.cpp -Wl,--out-implib,libsimple_vm.a
```
#### Linux (GCC/Clang)
```bash
g++ -shared -fPIC -O2 -DVM_DLL_EXPORT -o libsimple_vm.so vm.cpp
```
