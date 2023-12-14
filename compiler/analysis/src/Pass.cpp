#include <algorithm>
#include <set>

#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "noelle/core/Noelle.hpp"
#include "noelle/core/InductionVariableSCC.hpp"
#include "noelle/core/ReductionSCC.hpp"
#include "noelle/core/LoopIterationSCC.hpp"
#include "noelle/core/LoopCarriedUnknownSCC.hpp"
#include "noelle/core/MemoryClonableSCC.hpp"

using namespace llvm;
using namespace arcana::noelle;

namespace {

struct AnalysisPass : public ModulePass {
  static char ID;

  using LCDType = DGEdge<Value, Value>;

  AnalysisPass() : ModulePass(ID) {}

  bool doInitialization(Module &M) override {
    return false;
  }

  bool canBeTerminated(const LCDType &LCD) {
    auto srcValue = &*(LCD.getSrcNode()->getT());
    auto dstValue = &*(LCD.getDstNode()->getT());
    auto &CV = clauseVariables;
    if (CV.find(srcValue) != CV.end()) {
      return true;
    }
    return false;
  }

  void showLCD(const LCDType &LCD) const {
    auto srcValue = &*(LCD.getSrcNode()->getT());
    auto dstValue = &*(LCD.getDstNode()->getT());
    errs() << "DependenceTerminator: [src] "
           << *srcValue << "\n";
    errs() << "DependenceTerminator: [dst] "
           << *dstValue << "\n";
  }

  void gatherClauses(Module &M) {
    for (auto &F : M) {
      for (auto &I : instructions(F)) {
        if (isLDTC(I)) {
          clauses.insert(&I);
          errs() << "DependenceTerminator: "
                 << "Found clause " << I << "\n";
        }
      }
    }
  }

  void gatherClauseVariables(Module &M) {
    for (auto *C : clauses) {
      auto CI = cast<CallInst>(C);
      auto op = CI->getArgOperand(0);
      clauseVariables.insert(op);
      errs() << "DependenceTerminator: "
             << "Found clause variable " << *op << "\n";
    }
  }
  
  bool isLDTC(const Instruction &I) const {
    if (auto CI = dyn_cast<CallInst>(&I)) {
      auto callee = CI->getCalledFunction();
      if (callee && callee->getName().startswith("_Z9__dt_ldtc")) {
        return true;
      }
    }
    return false;
  }

  bool runOnModule(Module &M) override {
    auto &noelle = getAnalysis<Noelle>();
    gatherClauses(M);
    gatherClauseVariables(M);

    auto LSs = noelle.getLoopStructures();
    for (auto LS : *LSs) {
      auto entryInst = LS->getEntryInstruction();
      auto LC = noelle.getLoopContent(LS);
      auto LDG = LC->getLoopDG();

      auto sccManager = LC->getSCCManager();
      auto SCCDAG = sccManager->getSCCDAG();
      set<LCDType*> candidateLCDs;
      for (auto sccNode : SCCDAG->getSCCs()) {
        auto genericSCC = sccManager->getSCCAttrs(sccNode);
        if (auto LCU = dyn_cast<LoopCarriedUnknownSCC>(genericSCC)) {
          errs() << "DependenceTerminator: " << "Found unknown SCC in "
                 << LS->getFunction()->getName() << "\n";
          auto LCDs = LCU->getLoopCarriedDependences();
          candidateLCDs.insert(LCDs.begin(), LCDs.end());
        }
      }
      for (auto *LCD : candidateLCDs) {
        if (canBeTerminated(*LCD)) {
          errs() << "DependenceTerminator: " << "The following Loop-carried Dependence can be terminated:\n";
          showLCD(*LCD);
        }
      }
    }
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
  }

  set<Instruction*> clauses;
  set<Value*> clauseVariables;
  
};
} // namespace

// Registering pass

char AnalysisPass::ID = 0;
static RegisterPass<AnalysisPass> X("dt-analysis", "Identifies opportunities for dependences termination");

static AnalysisPass *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker =
      new AnalysisPass());
    }
  }
);

static RegisterStandardPasses _RegPass2(
  PassManagerBuilder::EP_EnabledOnOptLevel0,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker = new AnalysisPass());
    }
  }
);
