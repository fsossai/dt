#include <algorithm>
#include <queue>
#include <map>
#include <set>
#include <stack>

#include "llvm/Pass.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DerivedTypes.h"

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
    extractClauseOperands();
  }

  void print() const {
    errs() << "DependenceTerminator: Clause: Begin: "
           << *begin << "\n";
    errs() << "DependenceTerminator: Clause: End: "
           << *end << "\n";
    errs() << "DependenceTerminator: Clause: Function: "
           << function->getName() << "\n";
  }

  void extractClauseOperands() {
    auto CI = cast<CallInst>(begin);
    variable = CI->getArgOperand(0);
    auto f = CI->getArgOperand(1);
    if (auto fp = dyn_cast<Function>(f)) {
      function = fp;
      return;
    }

    // It may happen that the clause function is stored
    // as a `i64 ptrtoint (...)`

    auto BB = begin->getParent();
    auto load = cast<LoadInst>(f);
    auto gep = cast<GetElementPtrInst>(load->getPointerOperand());
    auto alloca = gep->getPointerOperand();

    for (auto it = BasicBlock::reverse_iterator(begin); it != BB->rend(); it++) {
      if (auto store = dyn_cast<StoreInst>(&*it)) {
        if (store->getPointerOperand() == alloca) {
          auto ptr = cast<User>(store->getValueOperand())->getOperand(0);
          auto fPtr = cast<PtrToIntOperator>(ptr)->getPointerOperand();
          function = cast<Function>(fPtr);
          return;
        }
      }
    }
  }

  Value *variable;
  Function *function;
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

  bool runOnModule(Module &M) override {
    initialize();
    resolveClauses();
    printClauses();
    sanityChecks();
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
  }

  void printDependence(const Dependence *LCD) const {
    auto srcValue = LCD->getSrcNode()->getT();
    auto dstValue = LCD->getDstNode()->getT();
    errs() << "DependenceTerminator: [src] "
           << *srcValue << "\n";
    errs() << "DependenceTerminator: [dst] "
           << *dstValue << "\n";
  }

  bool isLDTCBegin(const Instruction *I) const {
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

  bool isLDTCEnd(const Instruction *I) const {
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

  bool isLDTC(const Instruction *I) const {
    return isLDTCBegin(I) || isLDTCEnd(I);
  }

  set<Instruction*> getPragmasInLoop(LoopStructure *LS) const {
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
          for (auto it = LCDs.begin(); it != LCDs.end();) {
            auto dep = *it;
            if (dep->isControlDependence()) {
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
           << candidateLCDs_.size() << " candidate loop-carried dependences\n";
    errs() << "DependenceTerminator: Info: Found "
           << candidateLSs_.size() << " candidate loop structures\n";

    for (auto LS : candidateLSs_) {
      auto pragmas = getPragmasInLoop(LS);
      if (pragmas.size() > 0) {
        targetLSs_.insert(LS);
        loopToPragmas_[LS] = pragmas;
        errs() << "DependenceTerminator: Header: Candidate loop header:\n";
        errs() << *LS->getHeader() << "\n";
      }
    }
    errs() << "DependenceTerminator: Info: Found "
           << targetLSs_.size() << " target loops\n";
  }

  Instruction *findUnmatchedBegin(BasicBlock *BB) const {
    stack<Instruction*> begins;
    for (auto &I : *BB) {
      if (isLDTCBegin(&I)) {
        begins.push(&I);
      } else if (isLDTCEnd(&I)) {
        if (!begins.empty()) {
          begins.pop();
        }
      }
    }
    if (begins.empty()) {
      return nullptr;
    }
    return begins.top();
  }

  Instruction *findMatchingBeginSingleBlock(Instruction *end) const {
    stack<Instruction*> ends;
    ends.push(end);
    auto BB = end->getParent();
    auto rit = BB->rbegin();
    while (&*rit != end) rit++;
    for (; rit != BB->rend(); rit++) {
      auto &I = *rit;
      if (isLDTCBegin(&I)) {
        if (ends.top() == end) {
          return &I;
        } else {
          ends.pop();
        }
      } else if (isLDTCEnd(&I)) {
        ends.push(&I);
      }
    }
    return nullptr;
  }

  set<Instruction*> findMatchingBegin(Instruction *end, Instruction **beginFound) {
    auto &noelle = getAnalysis<Noelle>();
    auto F = end->getParent()->getParent();
    auto DS = noelle.getDominators(F);
    auto &DT = DS->DT;

    auto endBB = end->getParent();
    BasicBlock *beginBB = nullptr;
    *beginFound = nullptr;

    set<Instruction*> region;
    auto addRangeToRegion = [&](auto from, auto to) {
      auto it = from;
      while (it != to) {
        region.insert(&*it);
        it++;
      }
    };

    if (auto begin = findMatchingBeginSingleBlock(end)) {
      auto itFrom = endBB->begin();
      while (&*itFrom != begin) itFrom++;
      auto itTo = itFrom;
      while (&*itTo != end) itTo++;
      addRangeToRegion(++itFrom, itTo);
      *beginFound = begin;
      return region;
    }

    queue<BasicBlock*> q;
    set<BasicBlock*> selected;
    auto alreadySelected = [&](BasicBlock *BB) {
      return selected.find(BB) != selected.end();
    };
    q.push(endBB);

    while (!q.empty()) {
      auto current = q.front();
      q.pop();

      for (auto BB : predecessors(current)) {
        auto begin = findUnmatchedBegin(BB);
        if (begin == nullptr) {
          if (!alreadySelected(BB)) {
            selected.insert(BB);
            q.push(BB);
          }
        } else {
          assert(beginBB == nullptr);
          beginBB = BB;
          *beginFound = begin;
        }
      }
    }

    for (auto BB : selected) {
      addRangeToRegion(BB->begin(), BB->end());
    }

    // considering all instructions after `begin` in its BB
    auto it1 = beginBB->begin();
    auto begin = findUnmatchedBegin(beginBB);
    assert(begin != nullptr);
    while (&*it1 != begin) it1++;
    addRangeToRegion(++it1, beginBB->end());

    // considering all instruction before 'end' in its BB
    auto it2 = endBB->begin();
    while (&*it2 != end) it2++;
    addRangeToRegion(endBB->begin(), it2);

    *beginFound = begin;
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
    for (auto end : ends) {
      Instruction *begin;
      auto region = findMatchingBegin(end, &begin);
      auto clause = new Clause(begin, end);
      foundClauses.insert(clause);
      clauseToInsts_[clause] = region;
    }
    loopToClauses_[LS] = foundClauses;
    clauses_.insert(foundClauses.begin(), foundClauses.end());
    errs() << "DependenceTerminator: Info: Found "
           << clauses_.size() << " clauses\n";
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

  void sanityChecks() {
    // `begin` and `end` instructions must belong to one and only one clause
    set<Instruction*> beginSeen;
    set<Instruction*> endSeen;
    for (auto C : clauses_) {
      bool duplicatedBegin = !beginSeen.insert(C->begin).second;
      assert(!duplicatedBegin);
      bool duplicatedEnd = !endSeen.insert(C->end).second;
      assert(!duplicatedEnd);
    }
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
