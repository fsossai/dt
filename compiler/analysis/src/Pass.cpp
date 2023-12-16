#include <algorithm>
#include <queue>
#include <map>
#include <set>

#include "llvm/Pass.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "noelle/core/DominatorForest.hpp"
#include "noelle/core/LoopCarriedUnknownSCC.hpp"
#include "noelle/core/Noelle.hpp"

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

  void print() const {
    errs() << "DependenceTerminator: Clause: Begin: "
           << *begin << "\n";
    errs() << "DependenceTerminator: Clause: End: "
           << *end << "\n";
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

  bool containsEnd(BasicBlock *BB) {
    for (auto &I : *BB) {
      if (isLDTCEnd(&I)) {
        return true;
      }
    }
    return false;
  }

  // @return a non-empty set if the pragmas match
  set<Instruction*> matchPragmas(Instruction *begin, Instruction *end) {
    assert(begin->getParent()->getParent() == end->getParent()->getParent());

    auto &noelle = getAnalysis<Noelle>();
    auto F = begin->getParent()->getParent();
    auto DS = noelle.getDominators(F);
    auto &DT = DS->DT;

    auto beginBB = begin->getParent();
    auto endBB = end->getParent();

    if (!DT.dominates(beginBB, endBB)) {
      return {};
    }

    set<Instruction*> region;
    auto addRangeToRegion = [&region](auto from, auto to) {
      auto it = from;
      while (it++ != to) {
        region.insert(&*it);
        it++;
      }
    };

    queue<BasicBlock*> q;
    q.push(endBB);

    while (!q.empty()) {
      auto current = q.front();
      q.pop();

      for (auto BB : predecessors(current)) {
        if (BB != beginBB) {
          assert(DT.dominates(beginBB, BB));
          addRangeToRegion(BB->begin(), BB->end());
          if (!containsEnd(BB)) {
            q.push(BB);
          }
        }
      }
    }

    // considering all instructions after `begin` in its BB
    auto it1 = beginBB->begin();
    while (&*it1 != begin) it1++;
    addRangeToRegion(it1, beginBB->end());

    // considering all instruction before 'end' in its BB
    auto it2 = endBB->begin();
    while (&*it2 != end) it2++;
    addRangeToRegion(endBB->begin(), it2);

    return region;
  }
  
  void resolveClauses(LoopStructure *LS) {
    auto &pragmas = loopToPragmas_[LS];

    // Find `begin` and `end` pragmas
    set<Instruction*> begins;
    set<Instruction*> ends;
    for (auto I : pragmas) {
      if (isLDTCBegin(I)) {
        begins.insert(I);
      }
      if (isLDTCEnd(I)) {
        ends.insert(I);
      }
    }

    set<Clause*> foundClauses;
    for (auto begin : begins) {
      for (auto end : ends) {
        errs() << "matching " << *begin << "\n  with " << *end << "\n";
        auto region = matchPragmas(begin, end);
        if (region.size() > 0) {
          auto clause = new Clause(begin, end);
          foundClauses.insert(clause);
          clauseToInsts_[clause] = region;
        }
      }
    }
    loopToClauses_[LS] = foundClauses;
    clauses_.insert(foundClauses.begin(), foundClauses.end());
  }

  void resolveClauses() {
    for (auto LS : targetLSs_) {
      resolveClauses(LS);
    }
  }

  void printClauses() const {
    for (auto C : clauses_) {
      C->print();
    }
  }

  bool runOnModule(Module &M) override {
    initialize();
    resolveClauses();
    printClauses();
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
  }

private:
  set<Clause*> clauses_;
  map<LoopStructure*, set<Clause*>> loopToClauses_;
  map<Instruction*, Clause*> instToClause_;
  map<Clause*, set<Instruction*>> clauseToInsts_;
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
