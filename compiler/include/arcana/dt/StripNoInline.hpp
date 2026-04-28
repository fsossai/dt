#pragma once

#include "llvm/IR/Module.h"

namespace arcana::dt {

void stripNoInline(llvm::Module &M);

} // namespace arcana::dt
