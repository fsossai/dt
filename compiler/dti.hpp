// This header provides a mean to interface with the DT compiler.
// The following are attributes to attach information to C++
// single-exit-multiple-exit regions that the DT compiler
// will use to extend the LLVM IR.

#ifndef __DTI__
#define __DTI__

#ifdef DT_ENABLE

#define PRAGMA_LDTC_BEGIN(var, def, func, ...) __dt_ldtc_begin(&var, def, &func, ##__VA_ARGS__)
#define PRAGMA_LDTC_END() __dt_ldtc_end()

#else

#define PRAGMA_LDTC_BEGIN(var, def, func, ...)
#define PRAGMA_LDTC_END() 

#endif

template <typename V, typename F, typename... Args>
__attribute__((noinline, optnone))
void __dt_ldtc_begin(V *var, V def, F func, Args... args) noexcept {
}

__attribute((noinline, optnone))
void __dt_ldtc_end() noexcept {
}

#endif // __DTI__
