#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "arcana/dt/StripNoInline.hpp"
#include "Pass.hpp"

using namespace llvm;

namespace arcana::dt {

StripNoInlinePass::StripNoInlinePass() : ModulePass{ ID } {}

void StripNoInlinePass::getAnalysisUsage(AnalysisUsage &AU) const {}

bool StripNoInlinePass::runOnModule(Module &M) {
  stripNoInline(M);
  return true;
}

char StripNoInlinePass::ID = 0;
static RegisterPass<StripNoInlinePass> X("StripNoInline",
                                         "Strip noinline attribute from all functions",
                                         false,
                                         false);

} // namespace arcana::dt
