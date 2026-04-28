#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include "arcana/dt/StripNoInline.hpp"

using namespace llvm;

namespace arcana::dt {

void stripNoInline(Module &M) {
  for (auto &F : M) {
    F.removeFnAttr(Attribute::NoInline);
  }
}

} // namespace arcana::dt
