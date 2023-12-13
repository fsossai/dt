// This header provides a mean to interface with the DT compiler.
// The following are attributes to attach information to C++ compound
// statments that the DT compiler will use to extend the LLVM IR.

#ifndef __DTI__
#define __DTI__

#ifdef DT_ENABLE
#define PRAGMA_LDTC(var, func, ...) __dt_ldtc(&var, &func, __VA_ARGS__)
#else
#define PRAGMA_LDTC(var, func, ...)
#endif

template <typename V, typename F, typename... Args>
__attribute__((noinline, optnone))
void __dt_ldtc(V var, F func, Args... args) noexcept {
}


#endif // __DTI__
