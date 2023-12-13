// This header provides a mean to interface with the DT compiler.
// The following are attributes to attach information to C++ compound
// statments that the DT compiler will use to extend the LLVM IR.

#ifndef __DTI__
#define __DTI__

typedef void (*void_f_ptr_t)();

#ifdef DT_ENABLE
#define PRAGMA_LDTC(var, func) __dt_ldtc((void*)&(var), (void(*)())&(func));
#else
#define PRAGMA_LDTC(var, func)
#endif

//extern "C" {

void __dt_ldtc(void *var, void (*func)()) noexcept;

//}
#endif // __DTI__
