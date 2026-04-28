#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include "arcana/dt/StripNoInline.hpp"

using namespace llvm;

namespace arcana::dt {

uint64_t stripNoInline(Module &M) {
  uint64_t count = 0;
  for (auto &F : M) {
    if (F.hasFnAttribute(Attribute::NoInline)) {
      F.removeFnAttr(Attribute::NoInline);
      ++count;
    }
  }
  return count;
}

} // namespace arcana::dt
