#include "llvm/IR/Instructions.h"

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/NoellePass.hpp"

#include "Pass.hpp"
#include "LoopStopwatch.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

namespace arcana::dt {

LoopStopwatchPass::LoopStopwatchPass() : ModulePass{ ID } {}

bool LoopStopwatchPass::doInitialization(Module &M) {
  return false;
}

void LoopStopwatchPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<ScalarEvolutionWrapperPass>();
  AU.addRequired<AssumptionCacheTracker>();
  AU.addRequired<NoellePass>();
}

bool LoopStopwatchPass::runOnModule(Module &M) {
  auto &noelle = getAnalysis<NoellePass>().getNoelle();
  instrumentTaggedLoops(noelle, M);
  return true;
}

char LoopStopwatchPass::ID = 0;
static RegisterPass<LoopStopwatchPass> X(
    "LoopStopwatch",
    "Instruments loop.tag loops with stopwatches",
    false,
    false);

} // namespace arcana::dt
