#include <iostream>
#include <string>

#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "noelle/core/Noelle.hpp"
#include "noelle/core/InductionVariableSCC.hpp"
#include "noelle/core/ReductionSCC.hpp"
#include "noelle/core/LoopIterationSCC.hpp"
#include "noelle/core/LoopCarriedUnknownSCC.hpp"
#include "noelle/core/MemoryClonableSCC.hpp"

using namespace llvm;
using namespace llvm::noelle;

namespace {

struct PrinterPass : public ModulePass {
  static char ID;

  PrinterPass() : ModulePass(ID) {}

  bool doInitialization(Module &M) override {
    return false;
  }

  bool runOnModule(Module &M) {
    auto &noelle = getAnalysis<Noelle>();
    auto verbosity = noelle.getVerbosity();

    errs() << "DT: Printer: Start\n";
    /*
     * Fetch all the loops we want to parallelize.
     */
    auto forest = noelle.getLoopNestingForest();
    if (forest->getNumberOfLoops() == 0) {
      errs() << "DependenceTerminator:    There is no loop to consider\n";
      delete forest;

      errs() << "DependenceTerminator: Exit\n";
      return false;
    }

    auto mm = noelle.getMetadataManager();
    for (auto tree : forest->getTrees()) {
      auto collector = [&](LoopTree *n, uint32_t treeLevel) -> bool {
        auto ls = n->getLoop();
        //auto optimizations = {
        //  LoopDependenceInfoOptimization::MEMORY_CLONING_ID,
        //  LoopDependenceInfoOptimization::THREAD_SAFE_LIBRARY_ID
        //};
        auto ldi = noelle.getLoop(ls); //optimizations);
        //auto ls = ldi->getLoopStructure();
        auto header = ls->getPreHeader();
        errs() << "DependenceTerminator:    Function name: "
               << ls->getFunction()->getName().str() << "\n";
        //errs() << *ls->getHeader() << "\n";
        for (auto &I : *header) {
          errs() << I << "\n";
        }
        return false;
      };
      tree->visitPreOrder(collector);
    }

    return false;
  }

  //bool runOnModule(Module &M) override {
  //  auto &noelle = getAnalysis<Noelle>();
  //  auto loopStructures = noelle.getLoopStructures();

  //  for (auto LS : *loopStructures) {
  //    auto entryInst = LS->getEntryInstruction();
  //    errs() << "Loop " << *entryInst << "\n";
  //    auto loop = noelle.getLoop(LS);
  //    auto loopNode = loop->getLoopHierarchyStructures();
  //    errs() << " Function = " << LS->getFunction()->getName() << "\n";
  //    errs() << " Nesting level = " << LS->getNestingLevel() << "\n";
  //    errs() << " This loop has " << loopNode->getNumberOfSubLoops()
  //           << " sub-loops (including sub-loops of sub-loops)\n";

  //    auto LDG = loop->getLoopDG();
  //    errs() << " SCCDAG\n";
  //    auto sccManager = loop->getSCCManager();
  //    auto SCCDAG = sccManager->getSCCDAG();

  //    auto sccIterator = [sccManager](SCC *scc) -> bool {
  //      if (!scc->hasCycle()) {
  //        return false;
  //      }
  //      errs() << "   New SCC\n";
  //      errs() << "     Instructions:\n";
  //      auto mySCCIter = [](Instruction *i) -> bool {
  //        errs() << "       " << *i << "\n";
  //        return false;
  //      };
  //      scc->iterateOverInstructions(mySCCIter);

  //      return false;
  //    };

  //    SCCDAG->iterateOverSCCs(sccIterator);
  //  }
  //  errs() << "\n";

  //  for (auto l : *loopStructures) {
  //    if (l->getNestingLevel() > 1) {
  //      continue;
  //    }
  //    auto ldi = noelle.getLoop(l);
  //  }

  //  return false;
  //}

  //bool runOnModule(Module &M) override {
  //  auto &noelle = getAnalysis<Noelle>();
  //  //auto *LSs = noelle.getLoopStructuresReachableFromEntryFunction();
  //  //errs() << "Printer:  Begin\n";
  //  //for (auto &LS : *LSs) {
  //  //  errs() << "Printer:    Preheader in function " <<
  //  //    LS->getFunction()->getName() << "\n";
  //  //  errs() << *LS->getPreHeader();
  //  //  errs() << "\n\n";
  //  //}
  //  //errs() << "Printer: End\n";

  //  for (auto &F : M) {
  //    for (auto &BB : F) {
  //      for (auto &I : BB) {
  //        //errs() << I << "\n";
  //      }
  //    }
  //  }
  //  return false;
  //}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
    return;
  }
};

} // namespace

// Registering pass

char PrinterPass::ID = 0;
static RegisterPass<PrinterPass> X("printer", "Print clauses");

static PrinterPass *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker =
      new PrinterPass());
    }
  }
);

static RegisterStandardPasses _RegPass2(
  PassManagerBuilder::EP_EnabledOnOptLevel0,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker = new PrinterPass());
    }
  }
);
