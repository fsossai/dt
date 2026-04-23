#include <map>
#include <vector>

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/NoellePass.hpp"

#include "arcana/dt/Lifetime.hpp"
#include "Pass.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

namespace arcana::dt {

LifetimePass::LifetimePass() : ModulePass{ ID } {}

void LifetimePass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<NoellePass>();
}

bool LifetimePass::runOnModule(Module &M) {
  auto &noelle = getAnalysis<NoellePass>().getNoelle();
  auto *LSs = noelle.getLoopStructures();

  map<Function *, vector<LoopStructure *>> loopsByFunction;
  for (auto *LS : *LSs) {
    loopsByFunction[LS->getFunction()].push_back(LS);
  }

  for (auto &[F, loops] : loopsByFunction) {
    DominatorTree DT(*F);
    PostDominatorTree PDT(*F);
    for (auto *LS : loops) {
      markPrivateAllocas(LS, DT, PDT);
    }
  }

  return true;
}

char LifetimePass::ID = 0;
static RegisterPass<LifetimePass> X("Lifetime",
                                    "Mark per-iteration private allocas",
                                    false,
                                    false);

} // namespace arcana::dt
