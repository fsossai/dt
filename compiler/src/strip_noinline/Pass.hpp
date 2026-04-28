#pragma once

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "arcana/noelle/core/Lumberjack.hpp"

namespace arcana::dt {

class StripNoInlinePass : public llvm::ModulePass {
public:
  static char ID;

  StripNoInlinePass();
  bool runOnModule(llvm::Module &M) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

private:
  noelle::Logger log;
};

} // namespace arcana::dt
