#pragma once

#include "arcana/noelle/core/Noelle.hpp"

using namespace arcana::noelle;

namespace arcana::dt {

class LifetimePass : public ModulePass {
public:
  static char ID;

  LifetimePass();
  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

} // namespace arcana::dt
