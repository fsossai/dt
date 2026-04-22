#include <set>

#include "llvm/IR/Instructions.h"

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/NoellePass.hpp"

#include "arcana/dt/LoopSquasher.hpp"
#include "Pass.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

namespace arcana::dt {

static cl::list<int> WhiteList("squasher-white-list",
                               cl::ZeroOrMore,
                               cl::CommaSeparated,
                               cl::desc("Loop Squasher white list"));

LoopSquasherPass::LoopSquasherPass() : ModulePass{ ID } {}

bool LoopSquasherPass::doInitialization(Module &M) {
  this->loopIndexesWhiteList = WhiteList;

  return false;
}

void LoopSquasherPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<ScalarEvolutionWrapperPass>();
  AU.addRequired<AssumptionCacheTracker>();
  AU.addRequired<NoellePass>();

  return;
}

bool LoopSquasherPass::runOnModule(Module &M) {
  auto &noelle = getAnalysis<NoellePass>().getNoelle();
  auto *LSs = noelle.getLoopStructures();

  set<int> selectedIdxs(this->loopIndexesWhiteList.begin(),
                        this->loopIndexesWhiteList.end());

  for (auto *LS : *LSs) {
    auto loopID = LS->getID().value();
    auto toSquash = selectedIdxs.find(loopID) != selectedIdxs.end();

    if (toSquash) {
      squashLoop(LS);
    }
  }

  return true;
}

char LoopSquasherPass::ID = 0;
static RegisterPass<LoopSquasherPass> X("LoopSquasher",
                                        "Loop squashing transformation",
                                        false,
                                        false);

} // namespace arcana::dt
