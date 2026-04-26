#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
    #define HMODULE void*
    #define GetProcAddress dlsym
    #define FreeLibrary dlclose
#endif

#include <stdio.h>
#include <stdint.h>

// 定义函数指针类型
typedef void* (*VM_CREATE)(void);
typedef void (*VM_DESTROY)(void*);
typedef int (*VM_LOAD)(void*, const uint8_t*, size_t);
typedef int (*VM_RUN)(void*);
typedef int64_t (*VM_GET_RESULT)(void*);

int main() {
    // 1. 加载 DLL
    #ifdef _WIN32
        HMODULE hDll = LoadLibraryA("simple_vm.dll");
    #else
        HMODULE hDll = dlopen("./libsimple_vm.so", RTLD_LAZY);
    #endif

    if (!hDll) {
        printf("Failed to load DLL\n");
        return 1;
    }

    // 2. 获取函数地址
    VM_CREATE vm_create = (VM_CREATE)GetProcAddress(hDll, "vm_create");
    VM_DESTROY vm_destroy = (VM_DESTROY)GetProcAddress(hDll, "vm_destroy");
    VM_LOAD vm_load_program = (VM_LOAD)GetProcAddress(hDll, "vm_load_program");
    VM_RUN vm_run = (VM_RUN)GetProcAddress(hDll, "vm_run");
    VM_GET_RESULT vm_get_result = (VM_GET_RESULT)GetProcAddress(hDll, "vm_get_result");

    if (!vm_create || !vm_destroy || !vm_load_program || !vm_run || !vm_get_result) {
        printf("Failed to get function pointers\n");
        FreeLibrary(hDll);
        return 1;
    }

    // 3. 使用虚拟机
    void* vm = vm_create();
    if (!vm) return 1;

    // 字节码 (同前)
    uint8_t code[] = {
        0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03,
        0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x05,
        0x0B
    };

    if (vm_load_program(vm, code, sizeof(code)) != 0) {
        printf("Load error\n");
        vm_destroy(vm);
        FreeLibrary(hDll);
        return 1;
    }

    if (vm_run(vm) != 0) {
        printf("Run error\n");
        vm_destroy(vm);
        FreeLibrary(hDll);
        return 1;
    }

    int64_t result = vm_get_result(vm);
    printf("Result: %lld\n", (long long)result);

    vm_destroy(vm);
    FreeLibrary(hDll);
    return 0;
}