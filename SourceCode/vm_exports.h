#ifndef VM_EXPORTS_H
#define VM_EXPORTS_H

#ifdef _WIN32
    #ifdef VM_DLL_EXPORT
        #define VM_API __declspec(dllexport)
    #else
        #define VM_API __declspec(dllimport)
    #endif
#else
    #define VM_API __attribute__((visibility("default")))
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VMContext* VMHandle;

typedef enum {
    VM_OK = 0,
    VM_ERR_STACK_OVERFLOW,
    VM_ERR_STACK_UNDERFLOW,
    VM_ERR_INVALID_INSTRUCTION,
    VM_ERR_MEMORY_OUT_OF_BOUNDS,
    VM_ERR_DIVISION_BY_ZERO,
    VM_ERR_PROGRAM_NOT_LOADED,
    VM_ERR_BAD_JUMP_TARGET,
    VM_ERR_UNKNOWN
} VMError;

VM_API VMHandle vm_create(void);
VM_API void vm_destroy(VMHandle vm);
VM_API void vm_reset(VMHandle vm);                       // 重置执行状态，不清除全局内存
VM_API VMError vm_load_program(VMHandle vm, const uint8_t* bytecode, size_t size);
VM_API VMError vm_run(VMHandle vm);
VM_API int64_t vm_get_result(VMHandle vm);
VM_API VMError vm_set_global(VMHandle vm, uint32_t address, int64_t value);
VM_API int64_t vm_get_global(VMHandle vm, uint32_t address);
VM_API const char* vm_get_error_string(VMHandle vm);

// 辅助函数：将 int64 以小端序写入缓冲区（用于简化字节码生成）
VM_API void vm_write_int64_le(uint8_t* dest, int64_t value);
VM_API int64_t vm_read_int64_le(const uint8_t* src);

#ifdef __cplusplus
}
#endif

#endif