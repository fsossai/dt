#include <string>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"

#include "arcana/gino/core/DOALL.hpp"
#include "arcana/gino/core/HeuristicsPass.hpp"
#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/NoellePass.hpp"
#include "arcana/noelle/core/PragmaAnalysis.hpp"

#include "arcana/dt/Analysis.hpp"
#include "arcana/dt/Dot.hpp"
#include "Pass.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::gino;
using namespace arcana::noelle;

namespace arcana::dt {

static cl::opt<string> DotOutput("dot-output", cl::Hidden);

static cl::opt<uint64_t> DotTag("dot-tag", cl::Hidden);

DotPass::DotPass()
  : ModulePass{ ID },
    log(NoelleLumberjack, "Terminator.Dot") {}

bool DotPass::doInitialization(Module &M) {
  return false;
}

void DotPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<NoellePass>();
  AU.addRequired<HeuristicsPass>();
}

bool DotPass::runOnModule(Module &M) {
  auto &noelle = getAnalysis<NoellePass>().getNoelle();
  auto &LF = *noelle.getLoopNestingForest();

  PragmaAnalysis PA;
  noelle.addAnalysis(&PA);

  bool found = false;
  for (auto &F : M) {
    if (runOnFunction(F, noelle, LF)) {
      found = true;
      break;
    }
  }

  if (!found) {
    log.info() << "ERORR: target loop not found\n";
  }

  return false;
}

bool DotPass::runOnFunction(Function &F, Noelle &noelle, LoopForest &LF) {
  PragmaForest PF(F, "loop.tag");

  PragmaTree *targetPragma = nullptr;
  PF.visitPreOrder([&](PragmaTree *PT, auto) {
    auto args = PT->getArguments();
    assert(args.size() >= 1);
    auto tag = cast<ConstantInt>(args[0])->getZExtValue();
    if (tag == DotTag) {
      targetPragma = PT;
      return true;
    }
    return false;
  });

  if (targetPragma == nullptr) {
    // Requested loo tag is not in this function
    return false;
  }

  // Search for corresponding LoopStructure
  LoopStructure *targetLS = nullptr;
  for (auto LT : LF.getTrees()) {
    LT->visitPreOrder([&](LoopTree *LT, auto) {
      auto LS = LT->getLoop();
      if (LS->getFunction() != &F) {
        return true; // stop traversal
      }
      auto p = PF.findInnermostPragmaFor(LS->getHeader());
      if (p == nullptr) {
        return false; // continue traversal
      } else {
        targetLS = LS;
        return true; // stop traversal
      }
    });
  }

  if (targetLS == nullptr) {
    log.info() << "WARNING: selected loop.tag does not mark any loop\n";
    return true;
  }


  string outputFile;
  if (DotOutput.getNumOccurrences() == 0) {
    outputFile = "graph_tag_" + to_string(DotTag) + ".dot";
  } else {
    outputFile = DotOutput;
  }

  auto optimizations = { LoopContentOptimization::MEMORY_CLONING_ID,
                         LoopContentOptimization::THREAD_SAFE_LIBRARY_ID };

  TerminatorAnalysis TA(noelle, &LF, F, optimizations);
  auto LC = noelle.getLoopContent(targetLS);
  dumpToDotFormat(LC, outputFile, &TA);

  log.info() << "Dot file written to " << outputFile << "\n";

  return true;
}

char DotPass::ID = 0;
static RegisterPass<DotPass> X("Dot",
                               "Dumps SCCDAGs into Dot file",
                               false,
                               false);

} // namespace arcana::dt
