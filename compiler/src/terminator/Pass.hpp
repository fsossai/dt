#pragma once

#include <string>
#include <vector>

#include "arcana/noelle/core/Noelle.hpp"

using namespace arcana::noelle;

namespace arcana::dt {

class TerminatorPass : public ModulePass {
public:
  static char ID;

  TerminatorPass();
  bool doInitialization(Module &M) override;
  bool runOnModule(Module &M) override;
  bool runOnFunction(Noelle &noelle,
                     LoopForest &LF,
                     Function &F,
                     int &lastLoopOrder);
  void getAnalysisUsage(AnalysisUsage &AU) const override;

private:
  std::string prefix;
};

} // namespace arcana::dt
