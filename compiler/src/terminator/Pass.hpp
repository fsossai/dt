#pragma once

#include <string>
#include <vector>

#include "arcana/noelle/core/Noelle.hpp"

namespace arcana::dt {

class TerminatorPass : public llvm::ModulePass {
public:
  static char ID;

  TerminatorPass();
  bool doInitialization(llvm::Module &M) override;
  bool runOnModule(llvm::Module &M) override;
  bool runOnFunction(noelle::Noelle &noelle,
                     noelle::LoopForest &LF,
                     llvm::Function &F,
                     int &lastLoopOrder);
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

private:
  std::string prefix;
};

} // namespace arcana::dt
