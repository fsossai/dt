#include <set>

#include "llvm/IR/Instructions.h"

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/NoellePass.hpp"

#include "arcana/dt/LoopBlocker.hpp"
#include "Pass.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

namespace arcana::dt {

static cl::list<int> WhiteList("blocker-white-list",
                               cl::ZeroOrMore,
                               cl::CommaSeparated,
                               cl::desc("Loop Blocker white list"));

static cl::opt<int> NumBlocks("blocker-num-blocks",
                              cl::init(1),
                              cl::ZeroOrMore,
                              cl::desc("Loop Blocker number of blocks"));

LoopBlockerPass::LoopBlockerPass() : ModulePass{ ID } {}

bool LoopBlockerPass::doInitialization(Module &M) {
  this->loopIndexesWhiteList = WhiteList;

  return false;
}

void LoopBlockerPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<ScalarEvolutionWrapperPass>();
  AU.addRequired<AssumptionCacheTracker>();
  AU.addRequired<NoellePass>();

  return;
}

bool LoopBlockerPass::runOnModule(Module &M) {
  auto &noelle = getAnalysis<NoellePass>().getNoelle();
  auto *LSs = noelle.getLoopStructures();

  set<int> selectedIdxs(this->loopIndexesWhiteList.begin(),
                        this->loopIndexesWhiteList.end());

  for (auto *LS : *LSs) {
    auto loopID = LS->getID().value();
    auto toBlock = selectedIdxs.find(loopID) != selectedIdxs.end();

    if (toBlock) {
      auto LC = noelle.getLoopContent(LS);
      auto header = blockLoop(LC, NumBlocks);
      if (header == nullptr) {
        errs() << "LoopBlocker: loopID = " << loopID << " can't be blocked\n";
      } else {
        errs() << "LoopBlocker: blocked loopID = " << loopID
               << ", num blocks = " << NumBlocks << "\n";
      }
    }
  }

  return false;
}

char LoopBlockerPass::ID = 0;
static RegisterPass<LoopBlockerPass> X("LoopBlocker",
                                       "Loop blocking transformation",
                                       false,
                                       false);

} // namespace arcana::dt
