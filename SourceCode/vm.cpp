#define VM_DLL_EXPORT
#include "vm_exports.h"
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// 操作码
enum OpCode : uint8_t {
    OP_PUSH    = 0x01,
    OP_POP     = 0x02,
    OP_ADD     = 0x03,
    OP_SUB     = 0x04,
    OP_MUL     = 0x05,
    OP_DIV     = 0x06,
    OP_STORE   = 0x07,
    OP_LOAD    = 0x08,
    OP_JMP     = 0x09,
    OP_JMP_IF  = 0x0A,
    OP_HALT    = 0x0B,
    OP_PRINT   = 0x0C,
    OP_DUP     = 0x0D,
    OP_SWAP    = 0x0E,
    OP_NOP     = 0x00
};

static const size_t MAX_STACK = 1024;
static const uint32_t GLOBAL_SIZE = 4096;

struct VMContext {
    std::vector<int64_t> stack;
    std::vector<int64_t> globals;
    std::vector<uint8_t> code;
    size_t ip;
    bool running;
    VMError last_error;
    char error_msg[256];

    VMContext() : globals(GLOBAL_SIZE, 0), ip(0), running(false), last_error(VM_OK) {
        error_msg[0] = '\0';
    }

    void clear_state() {
        stack.clear();
        ip = 0;
        running = false;
    }

    void set_error(VMError err, const char* msg) {
        last_error = err;
        if (msg) {
            strncpy(error_msg, msg, sizeof(error_msg) - 1);
            error_msg[sizeof(error_msg)-1] = '\0';
        } else {
            error_msg[0] = '\0';
        }
        running = false;
    }

    bool stack_push(int64_t val) {
        if (stack.size() >= MAX_STACK) {
            set_error(VM_ERR_STACK_OVERFLOW, "Stack overflow");
            return false;
        }
        stack.push_back(val);
        return true;
    }

    bool stack_pop(int64_t& out) {
        if (stack.empty()) {
            set_error(VM_ERR_STACK_UNDERFLOW, "Stack underflow");
            return false;
        }
        out = stack.back();
        stack.pop_back();
        return true;
    }

    bool stack_peek(int64_t& out, size_t offset = 0) {
        if (offset >= stack.size()) {
            set_error(VM_ERR_STACK_UNDERFLOW, "Stack underflow (peek)");
            return false;
        }
        out = stack[stack.size() - 1 - offset];
        return true;
    }

    // 弹出两个元素: 先弹出右操作数，再弹出左操作数
    bool pop_two_operands(int64_t& left, int64_t& right) {
        if (!stack_pop(right)) return false;
        if (!stack_pop(left)) return false;
        return true;
    }
};

// 小端序读取 int64
static inline int64_t read_int64_le(const uint8_t* data) {
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val |= (uint64_t)data[i] << (i * 8);
    }
    return (int64_t)val;
}

static inline uint16_t read_uint16_le(const uint8_t* data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static inline int16_t read_int16_le(const uint8_t* data) {
    return (int16_t)read_uint16_le(data);
}

// 小端序写入 int64
void vm_write_int64_le(uint8_t* dest, int64_t value) {
    uint64_t v = (uint64_t)value;
    for (int i = 0; i < 8; ++i) {
        dest[i] = (uint8_t)(v >> (i * 8));
    }
}

int64_t vm_read_int64_le(const uint8_t* src) {
    return read_int64_le(src);
}

// 检查跳转目标是否有效
static inline bool is_valid_jump(VMContext* vm, size_t target_ip) {
    if (target_ip >= vm->code.size()) {
        vm->set_error(VM_ERR_BAD_JUMP_TARGET, "Jump target out of code bounds");
        return false;
    }
    return true;
}

static VMError execute(VMContext* vm) {
    vm->clear_state();
    vm->running = true;
    const uint8_t* code = vm->code.data();
    const size_t code_size = vm->code.size();

    while (vm->running && vm->ip < code_size) {
        uint8_t op = code[vm->ip++];
        switch (op) {
            case OP_PUSH:
                if (vm->ip + 8 > code_size) {
                    vm->set_error(VM_ERR_INVALID_INSTRUCTION, "Incomplete PUSH operand");
                    return vm->last_error;
                }
                {
                    int64_t val = read_int64_le(code + vm->ip);
                    vm->ip += 8;
                    if (!vm->stack_push(val)) return vm->last_error;
                }
                break;

            case OP_POP:
                {
                    int64_t dummy;
                    if (!vm->stack_pop(dummy)) return vm->last_error;
                }
                break;

            case OP_ADD:
                {
                    int64_t left, right;
                    if (!vm->pop_two_operands(left, right)) return vm->last_error;
                    if (!vm->stack_push(left + right)) return vm->last_error;
                }
                break;

            case OP_SUB:
                {
                    int64_t left, right;
                    if (!vm->pop_two_operands(left, right)) return vm->last_error;
                    if (!vm->stack_push(left - right)) return vm->last_error;
                }
                break;

            case OP_MUL:
                {
                    int64_t left, right;
                    if (!vm->pop_two_operands(left, right)) return vm->last_error;
                    if (!vm->stack_push(left * right)) return vm->last_error;
                }
                break;

            case OP_DIV:
                {
                    int64_t left, right;
                    if (!vm->pop_two_operands(left, right)) return vm->last_error;
                    if (right == 0) {
                        vm->set_error(VM_ERR_DIVISION_BY_ZERO, "Division by zero");
                        return vm->last_error;
                    }
                    if (!vm->stack_push(left / right)) return vm->last_error;
                }
                break;

            case OP_STORE:
                if (vm->ip + 2 > code_size) {
                    vm->set_error(VM_ERR_INVALID_INSTRUCTION, "Incomplete STORE address");
                    return vm->last_error;
                }
                {
                    uint16_t addr = read_uint16_le(code + vm->ip);
                    vm->ip += 2;
                    if (addr >= GLOBAL_SIZE) {
                        vm->set_error(VM_ERR_MEMORY_OUT_OF_BOUNDS, "Global address out of range");
                        return vm->last_error;
                    }
                    int64_t val;
                    if (!vm->stack_pop(val)) return vm->last_error;
                    vm->globals[addr] = val;
                }
                break;

            case OP_LOAD:
                if (vm->ip + 2 > code_size) {
                    vm->set_error(VM_ERR_INVALID_INSTRUCTION, "Incomplete LOAD address");
                    return vm->last_error;
                }
                {
                    uint16_t addr = read_uint16_le(code + vm->ip);
                    vm->ip += 2;
                    if (addr >= GLOBAL_SIZE) {
                        vm->set_error(VM_ERR_MEMORY_OUT_OF_BOUNDS, "Global address out of range");
                        return vm->last_error;
                    }
                    if (!vm->stack_push(vm->globals[addr])) return vm->last_error;
                }
                break;

            case OP_JMP:
                if (vm->ip + 2 > code_size) {
                    vm->set_error(VM_ERR_INVALID_INSTRUCTION, "Incomplete JMP offset");
                    return vm->last_error;
                }
                {
                    int16_t offset = read_int16_le(code + vm->ip);
                    vm->ip += 2;
                    size_t target = (size_t)((int64_t)vm->ip + offset);
                    if (!is_valid_jump(vm, target)) return vm->last_error;
                    vm->ip = target;
                }
                break;

            case OP_JMP_IF:
                if (vm->ip + 2 > code_size) {
                    vm->set_error(VM_ERR_INVALID_INSTRUCTION, "Incomplete JMP_IF offset");
                    return vm->last_error;
                }
                {
                    int16_t offset = read_int16_le(code + vm->ip);
                    vm->ip += 2;
                    int64_t cond;
                    if (!vm->stack_pop(cond)) return vm->last_error;
                    if (cond != 0) {
                        size_t target = (size_t)((int64_t)vm->ip + offset);
                        if (!is_valid_jump(vm, target)) return vm->last_error;
                        vm->ip = target;
                    }
                }
                break;

            case OP_HALT:
                vm->running = false;
                break;

            case OP_PRINT:
                {
                    int64_t val;
                    if (!vm->stack_pop(val)) return vm->last_error;
                    fprintf(stderr, "[VM] PRINT: %lld\n", (long long)val);
                }
                break;

            case OP_DUP:
                {
                    int64_t top;
                    if (!vm->stack_peek(top, 0)) return vm->last_error;
                    if (!vm->stack_push(top)) return vm->last_error;
                }
                break;

            case OP_SWAP:
                if (vm->stack.size() < 2) {
                    vm->set_error(VM_ERR_STACK_UNDERFLOW, "Not enough elements for SWAP");
                    return vm->last_error;
                }
                {
                    size_t sz = vm->stack.size();
                    std::swap(vm->stack[sz-1], vm->stack[sz-2]);
                }
                break;

            case OP_NOP:
                break;

            default:
                vm->set_error(VM_ERR_INVALID_INSTRUCTION, "Unknown opcode");
                return vm->last_error;
        }
    }

    return VM_OK;
}

// ---------- 导出函数 ----------
VM_API VMHandle vm_create(void) {
    return reinterpret_cast<VMHandle>(new VMContext());
}

VM_API void vm_destroy(VMHandle vm) {
    delete reinterpret_cast<VMContext*>(vm);
}

VM_API void vm_reset(VMHandle vm) {
    if (!vm) return;
    VMContext* ctx = reinterpret_cast<VMContext*>(vm);
    ctx->clear_state();
    ctx->last_error = VM_OK;
    ctx->error_msg[0] = '\0';
}

VM_API VMError vm_load_program(VMHandle vm, const uint8_t* bytecode, size_t size) {
    if (!vm) return VM_ERR_UNKNOWN;
    VMContext* ctx = reinterpret_cast<VMContext*>(vm);
    if (!bytecode || size == 0) {
        ctx->set_error(VM_ERR_INVALID_INSTRUCTION, "Null bytecode or zero size");
        return ctx->last_error;
    }
    ctx->code.assign(bytecode, bytecode + size);
    vm_reset(vm);   // 重置执行状态，但保留全局内存
    return VM_OK;
}

VM_API VMError vm_run(VMHandle vm) {
    if (!vm) return VM_ERR_UNKNOWN;
    VMContext* ctx = reinterpret_cast<VMContext*>(vm);
    if (ctx->code.empty()) {
        ctx->set_error(VM_ERR_PROGRAM_NOT_LOADED, "Program not loaded");
        return ctx->last_error;
    }
    return execute(ctx);
}

VM_API int64_t vm_get_result(VMHandle vm) {
    if (!vm) return 0;
    VMContext* ctx = reinterpret_cast<VMContext*>(vm);
    if (ctx->stack.empty()) return 0;
    return ctx->stack.back();
}

VM_API VMError vm_set_global(VMHandle vm, uint32_t address, int64_t value) {
    if (!vm) return VM_ERR_UNKNOWN;
    VMContext* ctx = reinterpret_cast<VMContext*>(vm);
    if (address >= GLOBAL_SIZE) {
        ctx->set_error(VM_ERR_MEMORY_OUT_OF_BOUNDS, "Global set address out of range");
        return ctx->last_error;
    }
    ctx->globals[address] = value;
    return VM_OK;
}

VM_API int64_t vm_get_global(VMHandle vm, uint32_t address) {
    if (!vm) return 0;
    VMContext* ctx = reinterpret_cast<VMContext*>(vm);
    if (address >= GLOBAL_SIZE) return 0;
    return ctx->globals[address];
}

VM_API const char* vm_get_error_string(VMHandle vm) {
    if (!vm) return "Invalid VM handle";
    VMContext* ctx = reinterpret_cast<VMContext*>(vm);
    return ctx->error_msg;
}