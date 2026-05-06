#pragma once

#include "arcana/noelle/core/Noelle.hpp"

using namespace arcana::noelle;

namespace arcana::dt {

class LoopStopwatchPass : public ModulePass {
public:
  static char ID;

  LoopStopwatchPass();
  bool doInitialization(Module &M) override;
  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

} // namespace arcana::dt
