#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "arcana/noelle/core/Lumberjack.hpp"
#include "arcana/dt/StripNoInline.hpp"
#include "Pass.hpp"

using namespace llvm;
using namespace arcana::noelle;

namespace arcana::dt {

StripNoInlinePass::StripNoInlinePass()
  : ModulePass{ ID },
    log(NoelleLumberjack, "StripNoInline") {}

void StripNoInlinePass::getAnalysisUsage(AnalysisUsage &AU) const {}

bool StripNoInlinePass::runOnModule(Module &M) {
  auto count = stripNoInline(M);
  log.info() << "Removed noinline from " << count << " function(s)\n";
  return count > 0;
}

char StripNoInlinePass::ID = 0;
static RegisterPass<StripNoInlinePass> X(
    "StripNoInline",
    "Strip noinline attribute from all functions",
    false,
    false);

} // namespace arcana::dt
