#include <algorithm>
#include <set>
#include <map>

#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "noelle/core/Noelle.hpp"
#include "noelle/core/LoopCarriedUnknownSCC.hpp"

using namespace llvm;
using namespace arcana::noelle;

namespace {

class Clause {
public:
  Clause(Instruction *begin, Instruction *end)
      : begin(begin)
      , end(end) {
    // TODO
  }

  Value *variable;
  FunctionType *function;
  Instruction *begin;
  Instruction *end;
};

struct AnalysisPass : public ModulePass {
  static char ID;

  using Dependence = DGEdge<Value, Value>;

  AnalysisPass() : ModulePass(ID) { }

  ~AnalysisPass() {
    for (auto *C : clauses_) {
      delete C;
    }
  }

  bool doInitialization(Module &M) override {
    return false;
  }

  void printDependence(const Dependence *LCD) const {
    auto srcValue = LCD->getSrcNode()->getT();
    auto dstValue = LCD->getDstNode()->getT();
    errs() << "DependenceTerminator: [src] "
           << *srcValue << "\n";
    errs() << "DependenceTerminator: [dst] "
           << *dstValue << "\n";
  }

  bool isLDTCBegin(const Instruction *I) {
    if (auto *CI = dyn_cast<CallInst>(I)) {
      auto callee = CI->getCalledFunction();
      if (callee) {
        if (callee->getName().startswith("_Z15__dt_ldtc_begin")) {
          return true;
        }
      }
    }
    return false;
  }

  bool isLDTCEnd(const Instruction *I) {
    if (auto *CI = dyn_cast<CallInst>(I)) {
      auto callee = CI->getCalledFunction();
      if (callee) {
        if (callee->getName().startswith("_Z13__dt_ldtc_end")) {
          return true;
        }
      }
    }
    return false;
  }

  bool isLDTC(const Instruction *I) {
    return isLDTCBegin(I) || isLDTCEnd(I);
  }

  Clause *getClauseFor(const Instruction *I) {
    return nullptr;
  }

  set<Instruction*> getPragmasInLoop(LoopStructure *LS) {
    set<Instruction*> pragmas;
    for (auto I : LS->getInstructions()) {
      if (isLDTC(I)) {
        pragmas.insert(I);
      }
    }
    return pragmas;
  }

  void initialize() {
    auto &noelle = getAnalysis<Noelle>();
    auto LSs = noelle.getLoopStructures();

    for (auto LS : *LSs) {
      if (LS->getFunction()->getName() != "main") {
        continue;
      }
      auto LC = noelle.getLoopContent(LS);
      auto sccManager = LC->getSCCManager();
      auto SCCDAG = sccManager->getSCCDAG();

      for (auto sccNode : SCCDAG->getSCCs()) {
        auto genericSCC = sccManager->getSCCAttrs(sccNode);
        if (auto LCU = dyn_cast<LoopCarriedUnknownSCC>(genericSCC)) {
          auto LCDs = LCU->getLoopCarriedDependences();
          // Filtering out control dependences
          for (auto it = LCDs.begin(); it != LCDs.end(); ) {
            auto dep = *it;
            if (dep->isControlDependence()) {
              //errs() << "DependenceTerminator: Dependence: Discarding control dependence in "
              //       << LS->getFunction()->getName() << "\n";
              //printDependence(dep);
              it = LCDs.erase(it);
            }
            else {
              ++it;
            }
          }
          candidateLCDs_.insert(LCDs.begin(), LCDs.end());
          candidateLSs_.insert(LS);

          for (auto LCD : LCDs) {
            errs() << "DependenceTerminator: Dependece: In "
                   << LS->getFunction()->getName() << "\n";
            printDependence(LCD);
          }
        } 
      }
    }

    errs() << "DependenceTerminator: Info: Found "
           << candidateLSs_.size() << " candidate loop structures\n";

    for (auto LS : candidateLSs_) {
      auto pragmas = getPragmasInLoop(LS);
      if (pragmas.size() > 0) {
        targetLSs_.insert(LS);
        loopToPragmas_[LS] = pragmas;
        errs() << "DependenceTerminator: Header: Candidate loop header\n";
        errs() << *LS->getHeader() << "\n";
      }
    }
    errs() << "DependenceTerminator: Info: Found "
           << targetLSs_.size() << " target loops\n";
  }

  Instruction* findMatchingBegin(Instruction *end) {
    // TODO
    return nullptr;
  }

  void resolveClausesForLoop(LoopStructure *LS) {
    auto &pragmas = loopToPragmas_[LS];

    // Find `end` pragmas
    set<Instruction*> ends;
    for (auto I : pragmas) {
      if (isLDTCEnd(I)) {
        ends.insert(I);
      }
    }

    set<Clause*> foundClauses;
    for (auto end : ends) {
      auto begin = findMatchingBegin(end);
      auto clause = new Clause(begin, end);
      foundClauses.insert(clause);
    }
    loopToClauses_[LS] = foundClauses;
    clauses_.insert(foundClauses.begin(), foundClauses.end());
  }

  void resolveAllClauses() {
    for (auto LS : targetLSs_) {
      resolveClausesForLoop(LS);
    }
  }

  bool runOnModule(Module &M) override {
    initialize();
    resolveAllClauses();
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
  }

private:
  set<Clause*> clauses_;
  map<LoopStructure*, set<Clause*>> loopToClauses_;
  map<Instruction*, Clause*> instToClause_;
  set<LoopStructure*> candidateLSs_;
  set<LoopStructure*> targetLSs_;
  set<Dependence*> candidateLCDs_;
  map<LoopStructure*, set<Instruction*>> loopToPragmas_;

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
