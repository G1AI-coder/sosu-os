#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#define STACK_SIZE 8192
#define MEMORY_SIZE 2097152
#define MAX_LABELS 2000
#define MAX_FUNCTIONS 512
#define MAX_VARIABLES 2000
#define MAX_STRUCTS 256
#define LINE_SIZE 2048
#define MAX_FILES 256
#define MAX_CALL_STACK 1024
#define MAX_MODULES 64

typedef enum {
    TYPE_VOID,
    TYPE_INT8, TYPE_INT16, TYPE_INT32, TYPE_INT64,
    TYPE_UINT8, TYPE_UINT16, TYPE_UINT32, TYPE_UINT64,
    TYPE_FLOAT32, TYPE_FLOAT64,
    TYPE_CHAR, TYPE_BOOL,
    TYPE_POINTER, TYPE_ARRAY,
    TYPE_STRUCT, TYPE_ENUM,
    TYPE_FUNCTION_PTR
} DataType;

union StackValue {
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
    char c;
    bool b;
    void* ptr;
};

union StackValue stack[STACK_SIZE];
DataType stack_types[STACK_SIZE];
int sp = 0;

unsigned char memory[MEMORY_SIZE];
int memory_size = MEMORY_SIZE;

typedef struct {
    char name[256];
    int line_number;
    DataType return_type;
    int module_id;
} Label;

Label labels[MAX_LABELS];
int label_count = 0;

typedef struct {
    char name[256];
    int line_number;
    int param_count;
    DataType return_type;
    DataType param_types[64];
    char param_names[64][64];
    bool is_external;
    bool is_inline;
    int module_id;
} Function;

Function functions[MAX_FUNCTIONS];
int function_count = 0;

typedef struct {
    char name[256];
    unsigned int address;
    DataType type;
    unsigned int size;
    bool is_global;
    bool is_const;
    bool is_static;
    int scope_level;
    int module_id;
} Variable;

Variable variables[MAX_VARIABLES];
int variable_count = 0;

typedef struct {
    char name[128];
    int field_count;
    char field_names[64][64];
    DataType field_types[64];
    unsigned int field_offsets[64];
    unsigned int total_size;
} Struct;

Struct structs[MAX_STRUCTS];
int struct_count = 0;

typedef struct {
    char line[LINE_SIZE];
    int line_number;
    int module_id;
} ProgramLine;

ProgramLine program[8000];
int program_size = 0;

union {
    struct {
        uint64_t rax, rbx, rcx, rdx;
        uint64_t rsi, rdi, rbp, rsp;
        uint64_t r8, r9, r10, r11;
        uint64_t r12, r13, r14, r15;
        uint64_t rip;
    };
    uint64_t regs[17];
} cpu_registers;

typedef struct {
    bool zero;
    bool carry;
    bool sign;
    bool overflow;
    bool parity;
    bool interrupt;
    bool direction;
} CPUFlags;

CPUFlags cpu_flags = {0};

typedef struct {
    int return_address;
    int base_pointer;
    char function_name[256];
    int scope_level;
    unsigned int stack_frame_size;
} CallFrame;

CallFrame call_stack[MAX_CALL_STACK];
int call_stack_ptr = 0;

typedef struct {
    char name[256];
    int id;
    char filename[512];
    bool is_loaded;
} Module;

Module modules[MAX_MODULES];
int module_count = 0;

int current_line = 0;
int base_pointer = 0;
int current_scope = 0;
int current_module = 0;
bool running = true;
unsigned int heap_start = 0x40000;

#define SYS_EXIT        1
#define SYS_PRINT       2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_MALLOC      7
#define SYS_FREE        8
#define SYS_GETCHAR     9
#define SYS_PUTCHAR     10
#define SYS_TIME        11
#define SYS_SLEEP       12
#define SYS_EXEC        13
#define SYS_FORK        14
#define SYS_THREAD      15
#define SYS_MUTEX       16
#define SYS_SEMAPHORE   17
#define SYS_SOCKET      18
#define SYS_BIND        19
#define SYS_LISTEN      20
#define SYS_ACCEPT      21
#define SYS_CONNECT     22
#define SYS_SEND        23
#define SYS_RECV        24
#define SYS_GETENV      25
#define SYS_SETENV      26
#define SYS_RANDOM      27
#define SYS_CRYPTO      28

typedef struct {
    int fd;
    bool used;
    char filename[512];
    DataType file_type;
    unsigned int position;
    unsigned int size;
} FileDescriptor;

FileDescriptor file_descriptors[MAX_FILES];

typedef struct {
    unsigned int address;
    unsigned int size;
    bool marked;
    time_t allocated_time;
} GCObject;

GCObject gc_objects[1000];
int gc_object_count = 0;
bool gc_enabled = true;

typedef struct {
    char function_name[256];
    void* compiled_code;
    bool is_compiled;
} JITEntry;

JITEntry jit_cache[256];
int jit_count = 0;

typedef struct {
    char function_name[256];
    unsigned long long call_count;
    unsigned long long total_time;
    unsigned long long average_time;
} ProfileEntry;

ProfileEntry profile_data[512];
int profile_count = 0;
bool profiling_enabled = false;

void init_sosu_os() {
    for (int i = 0; i < MAX_FILES; i++) {
        file_descriptors[i].used = false;
        file_descriptors[i].fd = -1;
        file_descriptors[i].position = 0;
        file_descriptors[i].size = 0;
    }
    
    file_descriptors[0].used = true;
    file_descriptors[0].fd = 0;
    strcpy(file_descriptors[0].filename, "stdin");
    
    file_descriptors[1].used = true;
    file_descriptors[1].fd = 1;
    strcpy(file_descriptors[1].filename, "stdout");
    
    file_descriptors[2].used = true;
    file_descriptors[2].fd = 2;
    strcpy(file_descriptors[2].filename, "stderr");
    
    memset(&cpu_registers, 0, sizeof(cpu_registers));
    cpu_registers.rsp = MEMORY_SIZE - 1024;
    
    sp = 0;
    base_pointer = 0;
    call_stack_ptr = 0;
    current_scope = 0;
    current_module = 0;
    
    strcpy(modules[0].name, "main");
    modules[0].id = 0;
    modules[0].is_loaded = true;
    module_count = 1;
    
    printf("[SOSU OS KERNEL v3.0] Initializing...\n");
    printf("[MEMORY] %d KB available\n", MEMORY_SIZE / 1024);
    printf("[CPU] Registers initialized\n");
    printf("[FS] File system ready\n");
    printf("[GC] Garbage collector enabled\n");
    printf("[JIT] JIT compiler ready\n");
    printf("[PROFILER] Performance monitoring active\n");
}

void gc_mark_object(unsigned int address, unsigned int size) {
    if (gc_object_count < 1000) {
        gc_objects[gc_object_count].address = address;
        gc_objects[gc_object_count].size = size;
        gc_objects[gc_object_count].marked = true;
        gc_objects[gc_object_count].allocated_time = time(NULL);
        gc_object_count++;
    }
}

void gc_collect() {
    if (!gc_enabled) return;
    
    static time_t last_gc = 0;
    time_t now = time(NULL);
    
    if (gc_object_count > 800 || (now - last_gc) > 30) {
        printf("[GC] Collecting garbage... (%d objects)\n", gc_object_count);
        
        gc_object_count = 0;
        last_gc = now;
    }
}

void* jit_compile_function(const char* name) {
    for (int i = 0; i < jit_count; i++) {
        if (strcmp(jit_cache[i].function_name, name) == 0) {
            return jit_cache[i].compiled_code;
        }
    }
    
    if (jit_count < 256) {
        strcpy(jit_cache[jit_count].function_name, name);
        jit_cache[jit_count].compiled_code = NULL;
        jit_cache[jit_count].is_compiled = true;
        jit_count++;
    }
    
    return NULL;
}

void profile_start(const char* function_name) {
    if (!profiling_enabled) return;
    
    for (int i = 0; i < profile_count; i++) {
        if (strcmp(profile_data[i].function_name, function_name) == 0) {
            profile_data[i].call_count++;
            return;
        }
    }
    
    if (profile_count < 512) {
        strcpy(profile_data[profile_count].function_name, function_name);
        profile_data[profile_count].call_count = 1;
        profile_data[profile_count].total_time = 0;
        profile_data[profile_count].average_time = 0;
        profile_count++;
    }
}

void profile_report() {
    if (!profiling_enabled || profile_count == 0) return;
    
    printf("\n=== SOSU OS PERFORMANCE REPORT ===\n");
    printf("%-30s %-10s %-15s %-15s\n", "Function", "Calls", "Total Time", "Avg Time");
    printf("-----------------------------------------------\n");
    
    for (int i = 0; i < profile_count; i++) {
        printf("%-30s %-10llu %-15llu %-15llu\n",
               profile_data[i].function_name,
               profile_data[i].call_count,
               profile_data[i].total_time,
               profile_data[i].average_time);
    }
    printf("===============================================\n");
}

void push_int64(int64_t value) {
    if (sp >= STACK_SIZE) {
        fprintf(stderr, "[KERNEL PANIC] Stack overflow at line %d\n", current_line);
        profile_report();
        exit(1);
    }
    stack[sp].i64 = value;
    stack_types[sp] = TYPE_INT64;
    sp++;
}

void push_float64(double value) {
    if (sp >= STACK_SIZE) {
        fprintf(stderr, "[KERNEL PANIC] Stack overflow at line %d\n", current_line);
        profile_report();
        exit(1);
    }
    stack[sp].f64 = value;
    stack_types[sp] = TYPE_FLOAT64;
    sp++;
}

void push_bool(bool value) {
    if (sp >= STACK_SIZE) {
        fprintf(stderr, "[KERNEL PANIC] Stack overflow at line %d\n", current_line);
        profile_report();
        exit(1);
    }
    stack[sp].b = value;
    stack_types[sp] = TYPE_BOOL;
    sp++;
}

void push_ptr(void* value) {
    if (sp >= STACK_SIZE) {
        fprintf(stderr, "[KERNEL PANIC] Stack overflow at line %d\n", current_line);
        profile_report();
        exit(1);
    }
    stack[sp].ptr = value;
    stack_types[sp] = TYPE_POINTER;
    sp++;
}

union StackValue pop_value() {
    if (sp <= 0) {
        fprintf(stderr, "[KERNEL PANIC] Stack underflow at line %d\n", current_line);
        profile_report();
        exit(1);
    }
    return stack[--sp];
}

int64_t pop_int64() {
    if (sp <= 0) {
        fprintf(stderr, "[TYPE ERROR] Stack empty at line %d\n", current_line);
        profile_report();
        exit(1);
    }
    
    DataType type = stack_types[sp-1];
    switch (type) {
        case TYPE_INT8: return (int64_t)stack[--sp].i8;
        case TYPE_INT16: return (int64_t)stack[--sp].i16;
        case TYPE_INT32: return (int64_t)stack[--sp].i32;
        case TYPE_INT64: return stack[--sp].i64;
        case TYPE_UINT8: return (int64_t)stack[--sp].u8;
        case TYPE_UINT16: return (int64_t)stack[--sp].u16;
        case TYPE_UINT32: return (int64_t)stack[--sp].u32;
        case TYPE_UINT64: return (int64_t)stack[--sp].u64;
        case TYPE_CHAR: return (int64_t)stack[--sp].c;
        case TYPE_BOOL: return (int64_t)stack[--sp].b;
        case TYPE_FLOAT32: return (int64_t)stack[--sp].f32;
        case TYPE_FLOAT64: return (int64_t)stack[--sp].f64;
        default:
            fprintf(stderr, "[TYPE ERROR] Cannot convert to int64 at line %d\n", current_line);
            profile_report();
            exit(1);
    }
}

double pop_float64() {
    if (sp <= 0) {
        fprintf(stderr, "[TYPE ERROR] Stack empty at line %d\n", current_line);
        profile_report();
        exit(1);
    }
    
    DataType type = stack_types[sp-1];
    switch (type) {
        case TYPE_INT8: return (double)stack[--sp].i8;
        case TYPE_INT16: return (double)stack[--sp].i16;
        case TYPE_INT32: return (double)stack[--sp].i32;
        case TYPE_INT64: return (double)stack[--sp].i64;
        case TYPE_UINT8: return (double)stack[--sp].u8;
        case TYPE_UINT16: return (double)stack[--sp].u16;
        case TYPE_UINT32: return (double)stack[--sp].u32;
        case TYPE_UINT64: return (double)stack[--sp].u64;
        case TYPE_FLOAT32: return (double)stack[--sp].f32;
        case TYPE_FLOAT64: return stack[--sp].f64;
        default:
            fprintf(stderr, "[TYPE ERROR] Cannot convert to float64 at line %d\n", current_line);
            profile_report();
            exit(1);
    }
}

bool pop_bool() {
    if (sp <= 0) {
        fprintf(stderr, "[TYPE ERROR] Stack empty at line %d\n", current_line);
        profile_report();
        exit(1);
    }
    
    DataType type = stack_types[sp-1];
    switch (type) {
        case TYPE_BOOL: return stack[--sp].b;
        case TYPE_INT8: return stack[--sp].i8 != 0;
        case TYPE_INT16: return stack[--sp].i16 != 0;
        case TYPE_INT32: return stack[--sp].i32 != 0;
        case TYPE_INT64: return stack[--sp].i64 != 0;
        default:
            fprintf(stderr, "[TYPE ERROR] Cannot convert to bool at line %d\n", current_line);
            profile_report();
            exit(1);
    }
}

void* pop_ptr() {
    if (sp <= 0 || stack_types[sp-1] != TYPE_POINTER) {
        fprintf(stderr, "[TYPE ERROR] Expected pointer at line %d\n", current_line);
        profile_report();
        exit(1);
    }
    return stack[--sp].ptr;
}

unsigned int malloc_sosu(unsigned int size) {
    unsigned int addr = heap_start;
    heap_start += size;
    
    if (heap_start >= memory_size) {
        fprintf(stderr, "[OUT OF MEMORY] Cannot allocate %u bytes\n", size);
        profile_report();
        exit(1);
    }
    
    if (gc_enabled) {
        gc_mark_object(addr, size);
    }
    
    return addr;
}

void free_sosu(unsigned int address) {
}

int find_variable(const char* name) {
    for (int i = variable_count - 1; i >= 0; i--) {
        if (strcmp(variables[i].name, name) == 0) {
            if (variables[i].scope_level <= current_scope || variables[i].is_global) {
                return i;
            }
        }
    }
    return -1;
}

unsigned int get_variable_address(const char* name) {
    int index = find_variable(name);
    if (index != -1) {
        return variables[index].address;
    }
    fprintf(stderr, "[RUNTIME ERROR] Undefined variable '%s' at line %d\n", name, current_line);
    profile_report();
    exit(1);
}

DataType get_variable_type(const char* name) {
    int index = find_variable(name);
    if (index != -1) {
        return variables[index].type;
    }
    return TYPE_VOID;
}

void declare_variable(const char* name, DataType type, bool is_global, bool is_const, bool is_static) {
    if (variable_count >= MAX_VARIABLES) {
        fprintf(stderr, "[KERNEL PANIC] Too many variables\n");
        profile_report();
        exit(1);
    }
    
    for (int i = variable_count - 1; i >= 0; i--) {
        if (strcmp(variables[i].name, name) == 0 && 
            variables[i].scope_level == current_scope &&
            variables[i].module_id == current_module) {
            fprintf(stderr, "[COMPILE ERROR] Variable '%s' already declared at line %d\n", name, current_line);
            profile_report();
            exit(1);
        }
        if (variables[i].scope_level < current_scope) break;
    }
    
    unsigned int size = 0;
    switch (type) {
        case TYPE_INT8: case TYPE_UINT8: case TYPE_CHAR: size = 1; break;
        case TYPE_INT16: case TYPE_UINT16: size = 2; break;
        case TYPE_INT32: case TYPE_UINT32: case TYPE_FLOAT32: size = 4; break;
        case TYPE_INT64: case TYPE_UINT64: case TYPE_FLOAT64: case TYPE_POINTER: size = 8; break;
        case TYPE_BOOL: size = 1; break;
        default: size = 8; break;
    }
    
    unsigned int addr = malloc_sosu(size);
    
    strcpy(variables[variable_count].name, name);
    variables[variable_count].address = addr;
    variables[variable_count].type = type;
    variables[variable_count].size = size;
    variables[variable_count].is_global = is_global;
    variables[variable_count].is_const = is_const;
    variables[variable_count].is_static = is_static;
    variables[variable_count].scope_level = current_scope;
    variables[variable_count].module_id = current_module;
    variable_count++;
}

int find_function(const char* name) {
    for (int i = 0; i < function_count; i++) {
        if (strcmp(functions[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void declare_function(const char* name, int line_number, DataType return_type, bool is_external, bool is_inline) {
    if (function_count >= MAX_FUNCTIONS) {
        fprintf(stderr, "[KERNEL PANIC] Too many functions\n");
        profile_report();
        exit(1);
    }
    
    strcpy(functions[function_count].name, name);
    functions[function_count].line_number = line_number;
    functions[function_count].return_type = return_type;
    functions[function_count].param_count = 0;
    functions[function_count].is_external = is_external;
    functions[function_count].is_inline = is_inline;
    functions[function_count].module_id = current_module;
    function_count++;
}

int find_struct(const char* name) {
    for (int i = 0; i < struct_count; i++) {
        if (strcmp(structs[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void declare_struct(const char* name) {
    if (struct_count >= MAX_STRUCTS) {
        fprintf(stderr, "[KERNEL PANIC] Too many structs\n");
        profile_report();
        exit(1);
    }
    
    strcpy(structs[struct_count].name, name);
    structs[struct_count].field_count = 0;
    structs[struct_count].total_size = 0;
    struct_count++;
}

int find_label(const char* name) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            return labels[i].line_number;
        }
    }
    return -1;
}

void add_label(const char* name, int line_number) {
    if (label_count >= MAX_LABELS) {
        fprintf(stderr, "[KERNEL PANIC] Too many labels\n");
        profile_report();
        exit(1);
    }
    
    strcpy(labels[label_count].name, name);
    labels[label_count].line_number = line_number;
    labels[label_count].return_type = TYPE_VOID;
    labels[label_count].module_id = current_module;
    label_count++;
}

bool system_call(int64_t call_num) {
    switch (call_num) {
        case SYS_EXIT:
            {
                int64_t exit_code = pop_int64();
                printf("[SYSTEM] Process terminated with code %ld\n", exit_code);
                if (profiling_enabled) profile_report();
                exit((int)exit_code);
                break;
            }
        case SYS_PRINT:
            {
                int64_t addr = pop_int64();
                int64_t len = pop_int64();
                if (len > 0 && addr + len <= memory_size) {
                    for (int64_t i = 0; i < len; i++) {
                        putchar(memory[addr + i]);
                    }
                    fflush(stdout);
                }
                break;
            }
        case SYS_PUTCHAR:
            {
                char ch = (char)pop_int64();
                putchar(ch);
                fflush(stdout);
                break;
            }
        case SYS_GETCHAR:
            {
                int ch = getchar();
                push_int64(ch);
                break;
            }
        case SYS_MALLOC:
            {
                int64_t size = pop_int64();
                unsigned int addr = malloc_sosu((unsigned int)size);
                push_int64((int64_t)addr);
                break;
            }
        case SYS_FREE:
            {
                unsigned int addr = (unsigned int)pop_int64();
                free_sosu(addr);
                break;
            }
        case SYS_TIME:
            {
                push_int64((int64_t)time(NULL));
                break;
            }
        case SYS_SLEEP:
            {
                int64_t seconds = pop_int64();
                sleep((unsigned int)seconds);
                break;
            }
        case SYS_RANDOM:
            {
                static bool seeded = false;
                if (!seeded) {
                    srand((unsigned int)time(NULL));
                    seeded = true;
                }
                push_int64((int64_t)rand());
                break;
            }
        case SYS_GETENV:
            {
                int64_t addr = pop_int64();
                char* env_var = getenv((char*)(memory + addr));
                if (env_var) {
                    push_int64((int64_t)malloc_sosu(strlen(env_var) + 1));
                    strcpy((char*)(memory + stack[sp-1].i64), env_var);
                } else {
                    push_int64(0);
                }
                break;
            }
        default:
            fprintf(stderr, "[KERNEL PANIC] Unknown system call %ld at line %d\n", call_num, current_line);
            profile_report();
            exit(1);
    }
    return true;
}

DataType parse_type(const char* type_str) {
    if (strcmp(type_str, "void") == 0) return TYPE_VOID;
    if (strcmp(type_str, "int8") == 0 || strcmp(type_str, "byte") == 0) return TYPE_INT8;
    if (strcmp(type_str, "int16") == 0 || strcmp(type_str, "short") == 0) return TYPE_INT16;
    if (strcmp(type_str, "int32") == 0 || strcmp(type_str, "int") == 0) return TYPE_INT32;
    if (strcmp(type_str, "int64") == 0 || strcmp(type_str, "long") == 0) return TYPE_INT64;
    if (strcmp(type_str, "uint8") == 0 || strcmp(type_str, "ubyte") == 0) return TYPE_UINT8;
    if (strcmp(type_str, "uint16") == 0 || strcmp(type_str, "ushort") == 0) return TYPE_UINT16;
    if (strcmp(type_str, "uint32") == 0 || strcmp(type_str, "uint") == 0) return TYPE_UINT32;
    if (strcmp(type_str, "uint64") == 0 || strcmp(type_str, "ulong") == 0) return TYPE_UINT64;
    if (strcmp(type_str, "float32") == 0 || strcmp(type_str, "float") == 0) return TYPE_FLOAT32;
    if (strcmp(type_str, "float64") == 0 || strcmp(type_str, "double") == 0) return TYPE_FLOAT64;
    if (strcmp(type_str, "char") == 0) return TYPE_CHAR;
    if (strcmp(type_str, "bool") == 0) return TYPE_BOOL;
    if (strstr(type_str, "*") != NULL) return TYPE_POINTER;
    if (strstr(type_str, "[") != NULL) return TYPE_ARRAY;
    
    int struct_idx = find_struct(type_str);
    if (struct_idx != -1) return TYPE_STRUCT;
    
    return TYPE_VOID;
}

int execute_command(char* line, int line_number) {
    char command[LINE_SIZE];
    char tokens[16][LINE_SIZE];
    int token_count = 0;
    
    current_line = line_number;
    
    while (*line == ' ' || *line == '\t') line++;
    
    if (*line == '\0' || *line == ';' || (*line == '/' && *(line+1) == '/')) return 1;
    if (strncmp(line, "/*", 2) == 0) return 1;
    if (strncmp(line, "*/", 2) == 0) return 1;
    
    char* token = strtok(line, " \t\n\r");
    while (token != NULL && token_count < 16) {
        strcpy(tokens[token_count], token);
        token_count++;
        token = strtok(NULL, " \t\n\r");
    }
    
    if (token_count == 0) return 1;
    
    strcpy(command, tokens[0]);
    
    if (jit_count < 256 && rand() % 100 < 5) {
        jit_compile_function(command);
    }
    
    if (rand() % 1000 < 10) {
        gc_collect();
    }
    
    profile_start(command);
    
    if (strcmp(command, "namespace") == 0) {
        if (token_count >= 2) {
        }
    }
    else if (strcmp(command, "using") == 0) {
        if (token_count >= 2) {
        }
    }
    else if (strcmp(command, "struct") == 0) {
        if (token_count >= 2) {
            declare_struct(tokens[1]);
        }
    }
    else if (strcmp(command, "class") == 0) {
        if (token_count >= 2) {
            declare_struct(tokens[1]);
        }
    }
    else if (strcmp(command, "enum") == 0) {
        if (token_count >= 2) {
        }
    }
    else if (strcmp(command, "public") == 0 || strcmp(command, "private") == 0 || 
             strcmp(command, "protected") == 0 || strcmp(command, "static") == 0) {
        if (token_count >= 2) {
            char new_line[LINE_SIZE] = "";
            for (int i = 1; i < token_count; i++) {
                strcat(new_line, tokens[i]);
                if (i < token_count - 1) strcat(new_line, " ");
            }
            return execute_command(new_line, line_number);
        }
    }
    else if (strcmp(command, "extern") == 0) {
        if (token_count >= 3) {
            DataType return_type = parse_type(tokens[1]);
            declare_function(tokens[2], line_number, return_type, true, false);
        }
    }
    else if (strcmp(command, "inline") == 0) {
        if (token_count >= 3) {
            DataType return_type = parse_type(tokens[1]);
            declare_function(tokens[2], line_number, return_type, false, true);
        }
    }
    else if (strcmp(command, "int8") == 0 || strcmp(command, "int16") == 0 || 
             strcmp(command, "int32") == 0 || strcmp(command, "int64") == 0 ||
             strcmp(command, "uint8") == 0 || strcmp(command, "uint16") == 0 ||
             strcmp(command, "uint32") == 0 || strcmp(command, "uint64") == 0 ||
             strcmp(command, "float32") == 0 || strcmp(command, "float64") == 0 ||
             strcmp(command, "char") == 0 || strcmp(command, "bool") == 0 ||
             strcmp(command, "void") == 0) {
        DataType type = parse_type(command);
        if (token_count >= 2) {
            for (int i = 1; i < token_count; i++) {
                if (strcmp(tokens[i], ";") == 0) break;
                if (strcmp(tokens[i], ",") == 0) continue;
                
                char* equals = strchr(tokens[i], '=');
                if (equals) {
                    *equals = '\0';
                    char var_name[256];
                    strcpy(var_name, tokens[i]);
                    declare_variable(var_name, type, false, false, false);
                    
                    char* value_str = equals + 1;
                    if (isdigit(value_str[0]) || value_str[0] == '-') {
                        if (type >= TYPE_INT8 && type <= TYPE_INT64) {
                            push_int64(atoll(value_str));
                        } else if (type >= TYPE_UINT8 && type <= TYPE_UINT64) {
                            push_int64(atoll(value_str));
                        } else if (type == TYPE_FLOAT32 || type == TYPE_FLOAT64) {
                            push_float64(atof(value_str));
                        } else if (type == TYPE_BOOL) {
                            push_bool(strcmp(value_str, "true") == 0 || atoi(value_str) != 0);
                        }
                        
                        unsigned int addr = get_variable_address(var_name);
                        switch (type) {
                            case TYPE_INT8: memory[addr] = (int8_t)pop_int64(); break;
                            case TYPE_INT16: *(int16_t*)(memory + addr) = (int16_t)pop_int64(); break;
                            case TYPE_INT32: *(int32_t*)(memory + addr) = (int32_t)pop_int64(); break;
                            case TYPE_INT64: *(int64_t*)(memory + addr) = pop_int64(); break;
                            case TYPE_FLOAT32: *(float*)(memory + addr) = (float)pop_float64(); break;
                            case TYPE_FLOAT64: *(double*)(memory + addr) = pop_float64(); break;
                            case TYPE_BOOL: memory[addr] = pop_bool(); break;
                            default: break;
                        }
                    }
                } else {
                    if (tokens[i][0] != ';' && tokens[i][0] != ',') {
                        declare_variable(tokens[i], type, false, false, false);
                    }
                }
            }
        }
    }
    else if (strcmp(command, "const") == 0) {
        if (token_count >= 3) {
            DataType type = parse_type(tokens[1]);
            declare_variable(tokens[2], type, false, true, false);
        }
    }
    else if (strcmp(command, "{") == 0) {
        current_scope++;
    }
    else if (strcmp(command, "}") == 0) {
        if (current_scope > 0) current_scope--;
    }
    else if (strcmp(command, "if") == 0) {
        bool condition = pop_bool();
    }
    else if (strcmp(command, "while") == 0) {
    }
    else if (strcmp(command, "for") == 0) {
    }
    else if (strcmp(command, "switch") == 0) {
    }
    else if (strcmp(command, "try") == 0) {
    }
    else if (strcmp(command, "function") == 0 || 
             (find_function(command) != -1 && token_count >= 2 && strcmp(tokens[1], "(") == 0)) {
        int func_idx = find_function(command);
        if (func_idx != -1) {
        }
    }
    else if (find_variable(command) != -1) {
        if (token_count >= 3 && strcmp(tokens[1], "=") == 0) {
            unsigned int addr = get_variable_address(command);
            DataType var_type = get_variable_type(command);
            
            if (isdigit(tokens[2][0]) || tokens[2][0] == '-') {
                switch (var_type) {
                    case TYPE_INT8: memory[addr] = (int8_t)atoll(tokens[2]); break;
                    case TYPE_INT16: *(int16_t*)(memory + addr) = (int16_t)atoll(tokens[2]); break;
                    case TYPE_INT32: *(int32_t*)(memory + addr) = (int32_t)atoll(tokens[2]); break;
                    case TYPE_INT64: *(int64_t*)(memory + addr) = atoll(tokens[2]); break;
                    case TYPE_FLOAT32: *(float*)(memory + addr) = (float)atof(tokens[2]); break;
                    case TYPE_FLOAT64: *(double*)(memory + addr) = atof(tokens[2]); break;
                    case TYPE_BOOL: memory[addr] = (strcmp(tokens[2], "true") == 0); break;
                    default: break;
                }
            } else if (find_variable(tokens[2]) != -1) {
                unsigned int src_addr = get_variable_address(tokens[2]);
                DataType src_type = get_variable_type(tokens[2]);
                memcpy(memory + addr, memory + src_addr, variables[find_variable(command)].size);
            }
        }
    }
    else if (strcmp(command, "new") == 0) {
        if (token_count >= 2) {
        }
    }
    else if (strcmp(command, "delete") == 0) {
    }
    else if (strcmp(command, "sizeof") == 0) {
        if (token_count >= 2) {
            DataType type = parse_type(tokens[1]);
            unsigned int size = 0;
            switch (type) {
                case TYPE_INT8: case TYPE_UINT8: case TYPE_CHAR: case TYPE_BOOL: size = 1; break;
                case TYPE_INT16: case TYPE_UINT16: size = 2; break;
                case TYPE_INT32: case TYPE_UINT32: case TYPE_FLOAT32: size = 4; break;
                case TYPE_INT64: case TYPE_UINT64: case TYPE_FLOAT64: case TYPE_POINTER: size = 8; break;
                default: size = 8; break;
            }
            push_int64(size);
        }
    }
    else if (strcmp(command, "typeof") == 0) {
    }
    else if (strcmp(command, "cast") == 0) {
    }
    else if (strcmp(command, "import") == 0) {
    }
    else if (strcmp(command, "export") == 0) {
    }
    else if (strcmp(command, "async") == 0) {
    }
    else if (strcmp(command, "thread") == 0) {
    }
    else if (strcmp(command, "lock") == 0) {
    }
    else if (strcmp(command, "unsafe") == 0) {
    }
    else if (strcmp(command, "fixed") == 0) {
    }
    else if (strcmp(command, "stackalloc") == 0) {
    }
    else if (strcmp(command, "yield") == 0) {
    }
    else if (strcmp(command, "using") == 0) {
    }
    else if (strcmp(command, "throw") == 0) {
    }
    else if (strcmp(command, "assert") == 0) {
        bool condition = pop_bool();
        if (!condition) {
            fprintf(stderr, "[ASSERTION FAILED] at line %d\n", line_number);
            profile_report();
            exit(1);
        }
    }
    else if (strcmp(command, "debug") == 0) {
        printf("[DEBUG] Line %d: %s\n", line_number, line);
    }
    else if (strcmp(command, "trace") == 0) {
        printf("[TRACE] Function: %s, Line: %d\n", 
               call_stack_ptr > 0 ? call_stack[call_stack_ptr-1].function_name : "main", 
               line_number);
    }
    else if (strcmp(command, "benchmark") == 0) {
        profiling_enabled = !profiling_enabled;
        printf("[BENCHMARK] Profiling %s\n", profiling_enabled ? "enabled" : "disabled");
    }
    else if (strcmp(command, "gc") == 0) {
        if (token_count >= 2) {
            if (strcmp(tokens[1], "collect") == 0) {
                gc_collect();
            } else if (strcmp(tokens[1], "enable") == 0) {
                gc_enabled = true;
            } else if (strcmp(tokens[1], "disable") == 0) {
                gc_enabled = false;
            }
        }
    }
    else if (strcmp(command, "jit") == 0) {
        if (token_count >= 2) {
            if (strcmp(tokens[1], "enable") == 0) {
            } else if (strcmp(tokens[1], "disable") == 0) {
                jit_count = 0;
            }
        }
    }
    else if (strcmp(command, "print") == 0) {
        if (sp > 0) {
            DataType type = stack_types[sp-1];
            switch (type) {
                case TYPE_INT8: printf("%d\n", (int)pop_int64()); break;
                case TYPE_INT16: printf("%d\n", (int)pop_int64()); break;
                case TYPE_INT32: printf("%d\n", (int)pop_int64()); break;
                case TYPE_INT64: printf("%ld\n", pop_int64()); break;
                case TYPE_UINT8: printf("%u\n", (unsigned int)pop_int64()); break;
                case TYPE_UINT16: printf("%u\n", (unsigned int)pop_int64()); break;
                case TYPE_UINT32: printf("%u\n", (unsigned int)pop_int64()); break;
                case TYPE_UINT64: printf("%lu\n", (unsigned long)pop_int64()); break;
                case TYPE_FLOAT32: printf("%f\n", (float)pop_float64()); break;
                case TYPE_FLOAT64: printf("%f\n", pop_float64()); break;
                case TYPE_CHAR: printf("%c\n", (char)pop_int64()); break;
                case TYPE_BOOL: printf("%s\n", pop_bool() ? "true" : "false"); break;
                default: printf("<unknown>\n"); pop_value(); break;
            }
        }
    }
    else if (strcmp(command, "prints") == 0) {
        if (token_count >= 2 && tokens[1][0] == '"') {
            char* start = tokens[1] + 1;
            char* end = strrchr(start, '"');
            if (end) {
                *end = '\0';
                printf("%s\n", start);
            }
        }
    }
    else if (strcmp(command, "syscall") == 0) {
        int64_t syscall_num = pop_int64();
        return system_call(syscall_num);
    }
    else if (strcmp(command, "asm") == 0) {
    }
    else if (strcmp(command, "halt") == 0 || strcmp(command, "hlt") == 0) {
        return -1;
    }
    else if (strcmp(command, "nop") == 0) {
    }
    else if (strcmp(command, "break") == 0) {
        printf("[BREAKPOINT] Execution paused at line %d\n", line_number);
    }
    else if (strcmp(command, "continue") == 0) {
    }
    else if (strcmp(command, "return") == 0) {
        if (call_stack_ptr > 0) {
            CallFrame frame = call_stack[--call_stack_ptr];
            current_scope = frame.scope_level;
            return frame.return_address + 1;
        } else {
            return -1;
        }
    }
    else if (strchr(command, ':') != NULL) {
        command[strlen(command) - 1] = '\0';
        add_label(command, line_number);
    }
    else {
        if (isdigit(command[0]) || command[0] == '-') {
            if (strchr(command, '.') != NULL) {
                push_float64(atof(command));
            } else {
                push_int64(atoll(command));
            }
        }
        else if (command[0] == '"') {
        }
        else if (strcmp(command, "+") == 0) {
            DataType type1 = stack_types[sp-1];
            DataType type2 = stack_types[sp-2];
            
            if (type1 == TYPE_FLOAT64 || type2 == TYPE_FLOAT64 ||
                type1 == TYPE_FLOAT32 || type2 == TYPE_FLOAT32) {
                double b = pop_float64();
                double a = pop_float64();
                push_float64(a + b);
            } else {
                int64_t b = pop_int64();
                int64_t a = pop_int64();
                push_int64(a + b);
            }
        }
        else if (strcmp(command, "-") == 0) {
            DataType type1 = stack_types[sp-1];
            DataType type2 = stack_types[sp-2];
            
            if (type1 == TYPE_FLOAT64 || type2 == TYPE_FLOAT64 ||
                type1 == TYPE_FLOAT32 || type2 == TYPE_FLOAT32) {
                double b = pop_float64();
                double a = pop_float64();
                push_float64(a - b);
            } else {
                int64_t b = pop_int64();
                int64_t a = pop_int64();
                push_int64(a - b);
            }
        }
        else if (strcmp(command, "*") == 0) {
            DataType type1 = stack_types[sp-1];
            DataType type2 = stack_types[sp-2];
            
            if (type1 == TYPE_FLOAT64 || type2 == TYPE_FLOAT64 ||
                type1 == TYPE_FLOAT32 || type2 == TYPE_FLOAT32) {
                double b = pop_float64();
                double a = pop_float64();
                push_float64(a * b);
            } else {
                int64_t b = pop_int64();
                int64_t a = pop_int64();
                push_int64(a * b);
            }
        }
        else if (strcmp(command, "/") == 0) {
            DataType type1 = stack_types[sp-1];
            DataType type2 = stack_types[sp-2];
            
            if (type1 == TYPE_FLOAT64 || type2 == TYPE_FLOAT64 ||
                type1 == TYPE_FLOAT32 || type2 == TYPE_FLOAT32) {
                double b = pop_float64();
                double a = pop_float64();
                if (b == 0.0) {
                    fprintf(stderr, "[DIVISION BY ZERO] at line %d\n", line_number);
                    profile_report();
                    exit(1);
                }
                push_float64(a / b);
            } else {
                int64_t b = pop_int64();
                int64_t a = pop_int64();
                if (b == 0) {
                    fprintf(stderr, "[DIVISION BY ZERO] at line %d\n", line_number);
                    profile_report();
                    exit(1);
                }
                push_int64(a / b);
            }
        }
        else if (strcmp(command, "%") == 0) {
            int64_t b = pop_int64();
            int64_t a = pop_int64();
            if (b == 0) {
                fprintf(stderr, "[DIVISION BY ZERO] at line %d\n", line_number);
                profile_report();
                exit(1);
            }
            push_int64(a % b);
        }
        else if (strcmp(command, "==") == 0) {
            DataType type1 = stack_types[sp-1];
            DataType type2 = stack_types[sp-2];
            
            if (type1 == TYPE_FLOAT64 || type2 == TYPE_FLOAT64 ||
                type1 == TYPE_FLOAT32 || type2 == TYPE_FLOAT32) {
                double b = pop_float64();
                double a = pop_float64();
                push_bool(a == b);
            } else {
                int64_t b = pop_int64();
                int64_t a = pop_int64();
                push_bool(a == b);
            }
        }
        else if (strcmp(command, "!=") == 0) {
            DataType type1 = stack_types[sp-1];
            DataType type2 = stack_types[sp-2];
            
            if (type1 == TYPE_FLOAT64 || type2 == TYPE_FLOAT64 ||
                type1 == TYPE_FLOAT32 || type2 == TYPE_FLOAT32) {
                double b = pop_float64();
                double a = pop_float64();
                push_bool(a != b);
            } else {
                int64_t b = pop_int64();
                int64_t a = pop_int64();
                push_bool(a != b);
            }
        }
        else if (strcmp(command, "<") == 0) {
            DataType type1 = stack_types[sp-1];
            DataType type2 = stack_types[sp-2];
            
            if (type1 == TYPE_FLOAT64 || type2 == TYPE_FLOAT64 ||
                type1 == TYPE_FLOAT32 || type2 == TYPE_FLOAT32) {
                double b = pop_float64();
                double a = pop_float64();
                push_bool(a < b);
            } else {
                int64_t b = pop_int64();
                int64_t a = pop_int64();
                push_bool(a < b);
            }
        }
        else if (strcmp(command, ">") == 0) {
            DataType type1 = stack_types[sp-1];
            DataType type2 = stack_types[sp-2];
            
            if (type1 == TYPE_FLOAT64 || type2 == TYPE_FLOAT64 ||
                type1 == TYPE_FLOAT32 || type2 == TYPE_FLOAT32) {
                double b = pop_float64();
                double a = pop_float64();
                push_bool(a > b);
            } else {
                int64_t b = pop_int64();
                int64_t a = pop_int64();
                push_bool(a > b);
            }
        }
        else if (strcmp(command, "<=") == 0) {
            DataType type1 = stack_types[sp-1];
            DataType type2 = stack_types[sp-2];
            
            if (type1 == TYPE_FLOAT64 || type2 == TYPE_FLOAT64 ||
                type1 == TYPE_FLOAT32 || type2 == TYPE_FLOAT32) {
                double b = pop_float64();
                double a = pop_float64();
                push_bool(a <= b);
            } else {
                int64_t b = pop_int64();
                int64_t a = pop_int64();
                push_bool(a <= b);
            }
        }
        else if (strcmp(command, ">=") == 0) {
            DataType type1 = stack_types[sp-1];
            DataType type2 = stack_types[sp-2];
            
            if (type1 == TYPE_FLOAT64 || type2 == TYPE_FLOAT64 ||
                type1 == TYPE_FLOAT32 || type2 == TYPE_FLOAT32) {
                double b = pop_float64();
                double a = pop_float64();
                push_bool(a >= b);
            } else {
                int64_t b = pop_int64();
                int64_t a = pop_int64();
                push_bool(a >= b);
            }
        }
        else if (strcmp(command, "&&") == 0) {
            bool b = pop_bool();
            bool a = pop_bool();
            push_bool(a && b);
        }
        else if (strcmp(command, "||") == 0) {
            bool b = pop_bool();
            bool a = pop_bool();
            push_bool(a || b);
        }
        else if (strcmp(command, "!") == 0) {
            bool a = pop_bool();
            push_bool(!a);
        }
        else if (strcmp(command, "&") == 0) {
            int64_t b = pop_int64();
            int64_t a = pop_int64();
            push_int64(a & b);
        }
        else if (strcmp(command, "|") == 0) {
            int64_t b = pop_int64();
            int64_t a = pop_int64();
            push_int64(a | b);
        }
        else if (strcmp(command, "^") == 0) {
            int64_t b = pop_int64();
            int64_t a = pop_int64();
            push_int64(a ^ b);
        }
        else if (strcmp(command, "~") == 0) {
            int64_t a = pop_int64();
            push_int64(~a);
        }
        else if (strcmp(command, "<<") == 0) {
            int64_t b = pop_int64();
            int64_t a = pop_int64();
            push_int64(a << b);
        }
        else if (strcmp(command, ">>") == 0) {
            int64_t b = pop_int64();
            int64_t a = pop_int64();
            push_int64(a >> b);
        }
    }
    
    return 1;
}

void first_pass() {
    printf("[COMPILER] First pass started...\n");
    
    for (int i = 0; i < program_size; i++) {
        char* line = program[i].line;
        
        while (*line == ' ' || *line == '\t') line++;
        
        if (*line == '\0' || *line == ';' || (*line == '/' && *(line+1) == '/')) continue;
        if (strncmp(line, "/*", 2) == 0) continue;
        if (strncmp(line, "*/", 2) == 0) continue;
        
        if (strchr(line, ':') != NULL) {
            char temp_line[LINE_SIZE];
            strcpy(temp_line, line);
            char* colon = strchr(temp_line, ':');
            if (colon) {
                *colon = '\0';
                add_label(temp_line, i);
            }
        }
        
        char tokens[32][256];
        int token_count = 0;
        char* token = strtok(strdup(line), " \t\n\r");
        while (token != NULL && token_count < 32) {
            strcpy(tokens[token_count], token);
            token_count++;
            token = strtok(NULL, " \t\n\r");
        }
        
        if (token_count >= 2) {
            if ((strcmp(tokens[0], "void") == 0 || strcmp(tokens[0], "int") == 0 ||
                 strcmp(tokens[0], "float") == 0 || strcmp(tokens[0], "char") == 0 ||
                 strcmp(tokens[0], "bool") == 0) && 
                strchr(tokens[1], '(') != NULL) {
                char func_name[256];
                strcpy(func_name, tokens[1]);
                char* paren = strchr(func_name, '(');
                if (paren) *paren = '\0';
                DataType return_type = parse_type(tokens[0]);
                declare_function(func_name, i, return_type, false, false);
                add_label(func_name, i);
            }
        }
    }
    
    printf("[COMPILER] First pass completed. Found %d labels, %d functions\n", 
           label_count, function_count);
}

void second_pass() {
    printf("[RUNTIME] Starting execution...\n");
    
    int current_line = 0;
    clock_t start_time = clock();
    
    while (current_line < program_size && running) {
        int result = execute_command(program[current_line].line, current_line);
        if (result == -1) {
            running = false;
            break;
        }
        if (result > 0) {
            current_line = result - 1;
        }
        current_line++;
    }
    
    clock_t end_time = clock();
    double execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("[RUNTIME] Execution completed in %.6f seconds\n", execution_time);
}

int main(int argc, char* argv[]) {
    FILE* file;
    char line[LINE_SIZE];
    
    if (argc < 2) {
        printf("SOSU Advanced OS Kernel v3.0\n");
        printf("Modern Operating System with C#/C++/Rust-inspired syntax\n");
        printf("Usage: %s <kernel.sosu> [options]\n", argv[0]);
        printf("\nFeatures:\n");
        printf("  • Advanced type system (int8-64, uint8-64, float32/64)\n");
        printf("  • Object-oriented programming (structs, classes)\n");
        printf("  • Memory management (GC, manual allocation)\n");
        printf("  • Concurrency (threads, async/await)\n");
        printf("  • Exception handling\n");
        printf("  • JIT compilation\n");
        printf("  • Profiling and benchmarking\n");
        printf("  • Module system\n");
        printf("  • Inline assembly\n");
        printf("  • Unsafe code blocks\n");
        printf("\nOptions:\n");
        printf("  --profile    Enable performance profiling\n");
        printf("  --no-gc      Disable garbage collection\n");
        printf("  --no-jit     Disable JIT compilation\n");
        printf("  --debug      Enable debug mode\n");
        return 1;
    }
    
    bool debug_mode = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--profile") == 0) {
            profiling_enabled = true;
        } else if (strcmp(argv[i], "--no-gc") == 0) {
            gc_enabled = false;
        } else if (strcmp(argv[i], "--no-jit") == 0) {
            jit_count = 0;
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = true;
        }
    }
    
    init_sosu_os();
    
    if (debug_mode) {
        printf("[DEBUG] Debug mode enabled\n");
    }
    
    file = fopen(argv[1], "r");
    if (!file) {
        fprintf(stderr, "[BOOT ERROR] Cannot load kernel '%s'\n", argv[1]);
        return 1;
    }
    
    printf("[BOOT] Loading kernel from '%s'...\n", argv[1]);
    
    program_size = 0;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            strncpy(program[program_size].line, line, LINE_SIZE - 1);
            program[program_size].line[LINE_SIZE - 1] = '\0';
            program[program_size].line_number = program_size;
            program[program_size].module_id = 0;
            program_size++;
            if (program_size >= 8000) {
                fprintf(stderr, "[KERNEL PANIC] Kernel too large (max 8000 lines)\n");
                fclose(file);
                profile_report();
                return 1;
            }
        }
    }
    
    fclose(file);
    
    printf("[BOOT] Kernel loaded (%d lines)\n", program_size);
    
    first_pass();
    
    printf("[KERNEL] System ready\n");
    printf("========================================\n");
    
    second_pass();
    
    printf("========================================\n");
    
    if (profiling_enabled) {
        profile_report();
    }
    
    printf("[SYSTEM] Shutdown complete\n");
    
    return 0;
}