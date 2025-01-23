#pragma once

#include <string>
#include <vector>

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/Lumberjack.hpp"

namespace arcana::dt {

class DotPass : public llvm::ModulePass {
public:
  static char ID;

  DotPass();
  bool doInitialization(llvm::Module &M) override;
  bool runOnModule(llvm::Module &M) override;
  bool runOnFunction(llvm::Function &F,
                     noelle::Noelle &noelle,
                     noelle::LoopForest &LF);
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

private:
  noelle::Logger log;
};

} // namespace arcana::dt
