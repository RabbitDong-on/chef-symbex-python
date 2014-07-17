/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

#ifndef _S2E_H
#define _S2E_H

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>

#include <pydebug.h>

#define S2E_INSTRUCTION_COMPLEX(val1, val2)             \
    ".byte 0x0F, 0x3F\n"                                \
    ".byte 0x00, 0x" #val1 ", 0x" #val2 ", 0x00\n"      \
    ".byte 0x00, 0x00, 0x00, 0x00\n"

#define S2E_INSTRUCTION_SIMPLE(val)                     \
    S2E_INSTRUCTION_COMPLEX(val, 00)

#ifdef __x86_64__
#define S2E_INSTRUCTION_REGISTERS_COMPLEX(val1, val2)   \
        "push %%rbx\n"                                  \
        "mov %%rdx, %%rbx\n"                            \
        S2E_INSTRUCTION_COMPLEX(val1, val2)             \
        "pop %%rbx\n"
#else
#define S2E_INSTRUCTION_REGISTERS_COMPLEX(val1, val2)   \
        "pushl %%ebx\n"                                 \
        "movl %%edx, %%ebx\n"                           \
        S2E_INSTRUCTION_COMPLEX(val1, val2)             \
        "popl %%ebx\n"
#endif

#define S2E_INSTRUCTION_REGISTERS_SIMPLE(val)           \
    S2E_INSTRUCTION_REGISTERS_COMPLEX(val, 00)

/** Forces the read of every byte of the specified string.
  * This makes sure the memory pages occupied by the string are paged in
  * before passing them to S2E, which can't page in memory by itself. */
static inline void __s2e_touch_string(volatile const char *string)
{
    while (*string) {
        ++string;
    }
}

static inline void __s2e_touch_buffer(volatile char *buffer, unsigned size)
{
    unsigned i;
    volatile const char *b = (volatile const char *) buffer;
    for (i = 0; i < size; ++i) {
        *b; ++b;
    }
}

/** Get S2E version or 0 when running without S2E. */
static inline int s2e_version(void)
{
    int version;

    if (!Py_EnableS2EFlag)
    	return 0;

    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(00)
        : "=a" (version)  : "a" (0)
    );
    return version;
}

/** Print message to the S2E log. */
static inline void s2e_message(const char *message)
{
    __s2e_touch_string(message);
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(10)
        : : "a" (message)
    );
}

/* Outputs a formatted string as an S2E message */
static inline int s2e_printf(const char *format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    s2e_message(buffer);
    return ret;
}

/** Print warning to the S2E log and S2E stdout. */
static inline void s2e_warning(const char *message)
{
    __s2e_touch_string(message);
    __asm__ __volatile__(
        S2E_INSTRUCTION_COMPLEX(10, 01)
        : : "a" (message)
    );
}

/** Print symbolic expression to the S2E log. */
static inline void s2e_print_expression(const char *name, int expression)
{
    __s2e_touch_string(name);
    __asm__ __volatile__(
        S2E_INSTRUCTION_COMPLEX(07, 01)
        : : "a" (expression), "c" (name)
    );
}

/** Enable forking on symbolic conditions. */
static inline void s2e_enable_forking(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(09)
    );
}

/** Disable forking on symbolic conditions. */
static inline void s2e_disable_forking(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(0A)
    );
}

/** Yield the current state */
static inline void s2e_yield(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(0F)
    );
}

/** Get the current execution path/state id. */
static inline unsigned s2e_get_path_id(void)
{
    unsigned id;
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(05)
        : "=a" (id)
    );
    return id;
}

/** Fill buffer with unconstrained symbolic values. */
static inline void s2e_make_symbolic(void *buf, int size, const char *name)
{
    __s2e_touch_string(name);
    __s2e_touch_buffer((char*)buf, size);
    __asm__ __volatile__(
        S2E_INSTRUCTION_REGISTERS_SIMPLE(03)
        : : "a" (buf), "d" (size), "c" (name) : "memory"
    );
}

/** Fill buffer with unconstrained symbolic values without discarding concrete data. */
static inline void s2e_make_concolic(void *buf, int size, const char *name)
{
    __s2e_touch_string(name);
    __s2e_touch_buffer((char*)buf, size);
    __asm__ __volatile__(
        S2E_INSTRUCTION_REGISTERS_SIMPLE(11)
        : : "a" (buf), "d" (size), "c" (name) : "memory"
    );
}

/** Prevent the searcher from switching states, unless the current state dies */
static inline void s2e_begin_atomic(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(12)
    );
}

static inline void s2e_end_atomic(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(13)
    );
}

/** Adds a constraint to the current state. The constraint must be satisfiable. */
static inline void s2e_assume(int expression)
{
    if (!Py_EnableS2EFlag)
    	return;

    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(0c)
        : : "a" (expression)
    );
}

/**
  * Adds a constraint to the current state. The constraint must be satisfiable.
  * expression is in [lower, upper]
  */
static inline void s2e_assume_range(unsigned int expression, unsigned int lower, unsigned int upper)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(0e)
        ::  "a" (expression), "c" (lower), "d" (upper)
    );
}


/** Returns true if ptr points to symbolic memory */
static inline int s2e_is_symbolic(const void *ptr, size_t size)
{
    int result;

    if (!Py_EnableS2EFlag)
        	return 0;

    __s2e_touch_buffer((char*)ptr, 1);
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(04)
        : "=a" (result) : "a" (size), "c" (ptr)
    );
    return result;
}

/** Concretize the expression. */
static inline void s2e_concretize(void *buf, int size)
{
    __s2e_touch_buffer((char*)buf, size);
    __asm__ __volatile__(
        S2E_INSTRUCTION_REGISTERS_SIMPLE(20)
        : : "a" (buf), "d" (size) : "memory"
    );
}

/** Get example value for expression (without adding state constraints). */
static inline void s2e_get_example(void *buf, int size)
{
    if (!Py_EnableS2EFlag)
    	return;

    __s2e_touch_buffer((char*)buf, size);
    __asm__ __volatile__(
        S2E_INSTRUCTION_REGISTERS_SIMPLE(21)
        : : "a" (buf), "d" (size) : "memory"
    );
}

/** Get example value for expression (without adding state constraints). */
/** Convenience function to be used in printfs */
static inline unsigned s2e_get_example_uint(unsigned val)
{
    unsigned buf = val;
    __asm__ __volatile__(
        S2E_INSTRUCTION_REGISTERS_SIMPLE(21)
        : : "a" (&buf), "d" (sizeof(buf)) : "memory"
    );
    return buf;
}

/** Get maximum value for unsigned expression. */
static inline unsigned s2e_get_upper_bound(unsigned val)
{
    unsigned buf = val;
    __asm__ __volatile__(
        S2E_INSTRUCTION_REGISTERS_SIMPLE(22)
        : : "a" (&buf), "d" (sizeof(buf)) : "memory"
    );
    return buf;
}

/** Get minimum value for expression. */
static inline unsigned s2e_get_lower_bound(unsigned val)
{
    unsigned buf = val;
    __asm__ __volatile__(
        S2E_INSTRUCTION_REGISTERS_SIMPLE(23)
        : : "a" (&buf), "d" (sizeof(buf)) : "memory"
    );
    return buf;
}

/** Terminate current state. */
static inline void s2e_kill_state(int status, const char *message)
{
    __s2e_touch_string(message);
    __asm__ __volatile__(
        S2E_INSTRUCTION_REGISTERS_SIMPLE(06)
        : : "a" (status), "d" (message)
    );
}

/* Outputs a formatted string as an S2E message */
static inline void s2e_kill_state_printf(int status, const char *message, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, message);
    vsnprintf(buffer, sizeof(buffer), message, args);
    va_end(args);
    s2e_kill_state(status, buffer);
}

/** Disable timer interrupt in the guest. */
static inline void s2e_disable_timer_interrupt(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_COMPLEX(50, 01)
    );
}

/** Enable timer interrupt in the guest. */
static inline void s2e_enable_timer_interrupt(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(50)
    );
}

/** Disable all APIC interrupts in the guest. */
static inline void s2e_disable_all_apic_interrupts(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_COMPLEX(51, 01)
    );
}

/** Enable all APIC interrupts in the guest. */
static inline void s2e_enable_all_apic_interrupts(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(51)
    );
}

/** Get the current S2E_RAM_OBJECT_BITS configuration macro */
static inline int s2e_get_ram_object_bits(void)
{
    int bits;
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(52)
        : "=a" (bits)  : "a" (0)
    );
    return bits;
}

/** Open file from the guest.
 *
 * NOTE: This requires the HostFiles plugin. */
static inline int s2e_open(const char *fname)
{
    int fd;
    __s2e_touch_string(fname);
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(EE)
        : "=a" (fd) : "a"(-1), "b" (fname), "c" (0)
    );
    return fd;
}

/** Close file from the guest.
 *
 * NOTE: This requires the HostFiles plugin. */
static inline int s2e_close(int fd)
{
    int res;
    __asm__ __volatile__(
        S2E_INSTRUCTION_COMPLEX(EE, 01)
        : "=a" (res) : "a" (-1), "b" (fd)
    );
    return res;
}

/** Read file content from the guest.
 *
 * NOTE: This requires the HostFiles plugin. */
static inline int s2e_read(int fd, char *buf, int count)
{
    int res;
    __s2e_touch_buffer(buf, count);
    __asm__ __volatile__(
#ifdef __x86_64__
        "push %%rbx\n"
        "mov %%rsi, %%rbx\n"
#else
        "pushl %%ebx\n"
        "movl %%esi, %%ebx\n"
#endif
        S2E_INSTRUCTION_COMPLEX(EE, 02)
#ifdef __x86_64__
        "pop %%rbx\n"
#else
        "popl %%ebx\n"
#endif
        : "=a" (res) : "a" (-1), "S" (fd), "c" (buf), "d" (count)
    );
    return res;
}

/** Enable memory tracing */
static inline void s2e_memtracer_enable(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(AC)
    );
}

/** Disable memory tracing */
static inline void s2e_memtracer_disable(void)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_COMPLEX(AC, 01)
    );
}

/** Raw monitor plugin */
/** Communicates to S2E the coordinates of loaded modules. Useful when there is
    no plugin to automatically parse OS data structures. */
static inline void s2e_rawmon_loadmodule(const char *name, unsigned loadbase, unsigned size)
{
    __s2e_touch_string(name);
    __asm__ __volatile__(
        S2E_INSTRUCTION_REGISTERS_SIMPLE(AA)
        : : "a" (name), "d" (loadbase), "c" (size)
    );
}

typedef struct _s2e_opcode_module_config_t {
    uint64_t name;
    uint64_t nativeBase;
    uint64_t loadBase;
    uint64_t entryPoint;
    uint64_t size;
    uint32_t kernelMode;
} __attribute__((packed)) s2e_opcode_module_config_t;

/** Raw monitor plugin */
/** Communicates to S2E the coordinates of loaded modules. Useful when there is
    no plugin to automatically parse OS data structures. */
static inline void s2e_rawmon_loadmodule2(const char *name,
                                          uint64_t nativebase,
                                          uint64_t loadbase,
                                          uint64_t entrypoint,
                                          uint64_t size,
                                          unsigned kernelMode)
{
    s2e_opcode_module_config_t cfg;
    cfg.name = (uintptr_t) name;
    cfg.nativeBase = nativebase;
    cfg.loadBase = loadbase;
    cfg.entryPoint = entrypoint;
    cfg.size = size;
    cfg.kernelMode = kernelMode;

    __s2e_touch_string(name);

    __asm__ __volatile__(
        S2E_INSTRUCTION_COMPLEX(AA, 02)
        : : "c" (&cfg)
    );
}

/** CodeSelector plugin */
/** Enable forking in the current process (entire address space or user mode only). */
static inline void s2e_codeselector_enable_address_space(unsigned user_mode_only)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(AE)
        : : "c" (user_mode_only)
    );
}

/** Disable forking in the specified process (represented by its page directory).
    If pagedir is 0, disable forking in the current process. */
static inline void s2e_codeselector_disable_address_space(uintptr_t pagedir)
{
    __asm__ __volatile__(
        S2E_INSTRUCTION_COMPLEX(AE, 01)
        : : "c" (pagedir)
    );
}

static inline void s2e_codeselector_select_module(const char *moduleId)
{
    __s2e_touch_string(moduleId);
    __asm__ __volatile__(
        S2E_INSTRUCTION_COMPLEX(AE, 02)
        : : "c" (moduleId)
    );
}

/** Programmatically add a new configuration entry to the ModuleExecutionDetector plugin. */
static inline void s2e_moduleexec_add_module(const char *moduleId, const char *moduleName, int kernelMode)
{
    __s2e_touch_string(moduleId);
    __s2e_touch_string(moduleName);
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(AF)
            : : "c" (moduleId), "a" (moduleName), "d" (kernelMode)
    );
}

/* Kills the current state if b is zero. */
static inline void _s2e_assert(int b, const char *expression)
{
    if (!b) {
        s2e_kill_state(0, expression);
    }
}

#define s2e_assert(expression) _s2e_assert(expression, "Assertion failed: "  #expression)

/** Returns a symbolic value in [start, end). */
static inline int s2e_range(int start, int end, const char *name)
{
    int x = -1;

    if (start >= end) {
        s2e_kill_state(1, "s2e_range: invalid range");
    }

    if (start + 1 == end) {
        return start;
    } else {
        s2e_make_symbolic(&x, sizeof x, name);

        /* Make nicer constraint when simple... */
        if (start == 0) {
            if ((unsigned) x >= (unsigned) end) {
                s2e_kill_state(0, "s2e_range creating a constraint...");
            }
        } else {
            if (x < start || x >= end) {
                s2e_kill_state(0, "s2e_range creating a constraint...");
            }
        }

        return x;
    }
}

static inline int __raw_invoke_plugin(const char *pluginName, void *data, uint32_t dataSize) {
    int result;

    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(0B)
        : "=a" (result) : "a" (pluginName), "c" (data), "d" (dataSize) : "memory"
    );

    return result;
}

static inline int __raw_invoke_plugin_concrete(const char *pluginName, void *data, uint32_t dataSize) {
    int result;

    __asm__ __volatile__(
        #ifdef __x86_64__
        "push %%rbx\n"
        "push %%rsi\n"
        "push %%rdi\n"
        "push %%rbp\n"
        "push %%r8\n"
        "push %%r9\n"
        "push %%r10\n"
        "push %%r11\n"
        "push %%r12\n"
        "push %%r13\n"
        "push %%r14\n"
        "push %%r15\n"

        "xor  %%rbx, %%rbx\n"
        "xor  %%rsi, %%rsi\n"
        "xor  %%rdi, %%rdi\n"
        "xor  %%rbp, %%rbp\n"
        "xor  %%r8, %%r8\n"
        "xor  %%r9, %%r9\n"
        "xor  %%r10, %%r10\n"
        "xor  %%r11, %%r11\n"
        "xor  %%r12, %%r12\n"
        "xor  %%r13, %%r13\n"
        "xor  %%r14, %%r14\n"
        "xor  %%r15, %%r15\n"
        #else
        "push %%ebx\n"
        "push %%ebp\n"
        "push %%esi\n"
        "push %%edi\n"
        "xor %%ebx, %%ebx\n"
        "xor %%ebp, %%ebp\n"
        "xor %%esi, %%esi\n"
        "xor %%edi, %%edi\n"
        #endif

        S2E_INSTRUCTION_SIMPLE(53) /* Clear temp flags */

        "jmp __sip1\n" /* Force concrete mode */
        "__sip1:\n"

        S2E_INSTRUCTION_SIMPLE(0B)

#ifdef __x86_64__
        "pop %%r15\n"
        "pop %%r14\n"
        "pop %%r13\n"
        "pop %%r12\n"
        "pop %%r11\n"
        "pop %%r10\n"
        "pop %%r9\n"
        "pop %%r8\n"
        "pop %%rbp\n"
        "pop %%rdi\n"
        "pop %%rsi\n"
        "pop %%rbx\n"
#else
        "pop %%edi\n"
        "pop %%esi\n"
        "pop %%ebp\n"
        "pop %%ebx\n"
#endif

            : "=a" (result) : "a" (pluginName), "c" (data), "d" (dataSize) : "memory"
    );

    return result;
}

/**
 *  Transmits a buffer of dataSize length to the plugin named in pluginName.
 *  eax contains the failure code upon return, 0 for success.
 */
static inline int s2e_invoke_plugin(const char *pluginName, void *data, uint32_t dataSize)
{
    if (!Py_EnableS2EFlag)
    	return 1;

    __s2e_touch_string(pluginName);
    __s2e_touch_buffer((char*)data, dataSize);

    return __raw_invoke_plugin(pluginName, data, dataSize);
}

/**
 *  Transmits a buffer of dataSize length to the plugin named in pluginName.
 *  eax contains the failure code upon return, 0 for success.
 *  The functions ensures that the CPU state is concrete before invoking the plugin.
 */
static inline int s2e_invoke_plugin_concrete(const char *pluginName, void *data, uint32_t dataSize)
{
    if (!Py_EnableS2EFlag)
        return 1;

    __s2e_touch_string(pluginName);
    __s2e_touch_buffer(data, dataSize);

    return __raw_invoke_plugin_concrete(pluginName, data, dataSize);
}


typedef struct _merge_desc_t {
    uint64_t start;
} merge_desc_t;

static inline void s2e_merge_group_begin(void)
{
    merge_desc_t desc;
    desc.start = 1;
    s2e_invoke_plugin("MergingSearcher", &desc, sizeof(desc));
}

static inline void s2e_merge_group_end(void)
{
    merge_desc_t desc;
    desc.start = 0;
    s2e_invoke_plugin_concrete("MergingSearcher", &desc, sizeof(desc));
}

static inline int s2e_hex_dump(const char *name, void *addr, unsigned size)
{
    int fd = -1;
    __s2e_touch_string(name);
    __asm__ __volatile__(
        S2E_INSTRUCTION_SIMPLE(36)
        :: "a"(addr), "b" (size), "c" (name)
    );
    return fd;
}


typedef struct {
    uint32_t id;
    uint32_t data;
    uint32_t dataSize;
} __attribute__((packed)) syscall_t;


static inline int s2e_system_call(const char *pluginName,
        uint32_t id, void *data, uint32_t dataSize) {
    if (!Py_EnableS2EFlag)
        return 1;

    syscall_t syscall;
    syscall.id = id;
    syscall.data = (uint32_t)data;
    syscall.dataSize = dataSize;

    __s2e_touch_string(pluginName);
    if (data) {
        __s2e_touch_buffer((char*)data, dataSize);
    }

    return __raw_invoke_plugin(pluginName, &syscall, sizeof(syscall));
}

static inline int s2e_system_call_concrete(const char *pluginName,
        uint32_t id, void *data, uint32_t dataSize) {
    if (!Py_EnableS2EFlag)
        return 1;

    syscall_t syscall;
    syscall.id = id;
    syscall.data = (uint32_t)data;
    syscall.dataSize = dataSize;

    __s2e_touch_string(pluginName);
    if (data) {
        __s2e_touch_buffer((char*)data, dataSize);
    }

    return __raw_invoke_plugin(pluginName, &syscall, sizeof(syscall));
}

#endif /* _S2E_H */
