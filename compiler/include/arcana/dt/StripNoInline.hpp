#pragma once

#include "llvm/IR/Module.h"

namespace arcana::dt {

uint64_t stripNoInline(llvm::Module &M);

} // namespace arcana::dt
