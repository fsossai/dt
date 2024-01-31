#include <algorithm>
#include <queue>
#include <map>
#include <set>
#include <stack>

#include "llvm/IR/Instructions.h"

#include "noelle/core/LoopCarriedUnknownSCC.hpp"
#include "noelle/core/Noelle.hpp"

#include "DependenceTerminator.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

namespace arcana::terminator {

Clause::Clause(Instruction *begin, Instruction *end)
    : begin(begin)
    , end(end) {
  extractClauseOperands();
}

void Clause::print() const {
  errs() << "DTAnalysis: Clause: Function: "
         << function->getName() << "\n";
}

void Clause::extractClauseOperands() {
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

DTAnalysis::DTAnalysis()
    : ModulePass(ID) {
}

DTAnalysis::~DTAnalysis() {
  for (auto *C : clauses_) {
    delete C;
  }
}

bool DTAnalysis::doInitialization(Module &M) {
  return false;
}

bool DTAnalysis::runOnModule(Module &M) {
  findCandidates();
  resolveClauses();
  printClauses();
  sanityChecks();
  categorizeDependences();
  return false;
}

void DTAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<Noelle>();
}

void DTAnalysis::printDependence(const Dependence *LCD) const {
  auto srcValue = LCD->getSrcNode()->getT();
  auto dstValue = LCD->getDstNode()->getT();
  errs() << "DTAnalysis: [src] "
         << *srcValue << "\n";
  errs() << "DTAnalysis: [dst] "
         << *dstValue << "\n";
}

bool DTAnalysis::isLDTCBegin(const Instruction *I) const {
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

bool DTAnalysis::isLDTCEnd(const Instruction *I) const {
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

bool DTAnalysis::isLDTC(const Instruction *I) const {
  return isLDTCBegin(I) || isLDTCEnd(I);
}

set<Instruction*> DTAnalysis::getPragmasInLoop(LoopStructure *LS) const {
  auto &noelle = getAnalysis<Noelle>();
  auto LF = noelle.getLoopNestingForest();
  set<Instruction*> pragmas;
  for (auto I : LS->getInstructions()) {
    if (isLDTC(I)) {
      // Pragmas are always referred to the innermost loop that contain them
      if (LF->getInnermostLoopThatContains(I)->getLoop()->getHeader()
          == LS->getHeader()) {
        pragmas.insert(I);
      }
    }
  }
  return pragmas;
}

void DTAnalysis::findCandidates() {
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
          if (isa<ControlDependence<Value, Value>>(dep)) {
            it = LCDs.erase(it);
          }
          else {
            ++it;
          }
        }
        candidateLCDs_.insert(LCDs.begin(), LCDs.end());
        candidateLSs_.insert(LS);

        for (auto LCD : LCDs) {
          errs() << "DTAnalysis: Dependece: In "
                 << LS->getFunction()->getName() << "\n";
          printDependence(LCD);
        }
      } 
    }
  }

  errs() << "DTAnalysis: Info: Found "
         << candidateLCDs_.size() << " candidate LCDs\n";
  errs() << "DTAnalysis: Info: Found "
         << candidateLSs_.size() << " candidate loops\n";

  for (auto LS : candidateLSs_) {
    auto pragmas = getPragmasInLoop(LS);
    if (pragmas.size() > 0) {
      targetLSs_.insert(LS);
      loopToPragmas_[LS] = pragmas;
      errs() << "DTAnalysis: Header: Target loop header:\n";
      errs() << *LS->getHeader() << "\n";
    }
  }
  errs() << "DTAnalysis: Info: Found "
         << targetLSs_.size() << " target loops\n";
}

bool DTAnalysis::isUnmatched(Instruction *begin) const {
  return matchedBegins_.find(begin) == matchedBegins_.end();
}

Instruction *DTAnalysis::findUnmatchedBegin(BasicBlock *BB) const {
  stack<Instruction*> begins;
  for (auto &I : *BB) {
    if (isLDTCBegin(&I) && isUnmatched(&I)) {
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

Instruction *DTAnalysis::findMatchingBeginSingleBlock(Instruction *end) const {
  stack<Instruction*> ends;
  ends.push(end);
  auto BB = end->getParent();
  auto rit = BB->rbegin();
  while (&*rit != end) rit++;
  for (; rit != BB->rend(); rit++) {
    auto &I = *rit;
    if (isLDTCBegin(&I) && isUnmatched(&I)) {
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

set<Instruction*> DTAnalysis::findMatchingBegin(Instruction *end, Instruction **beginFound) {
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

void DTAnalysis::resolveClauses(LoopStructure *LS) {
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
    Instruction *begin;
    auto region = findMatchingBegin(end, &begin);
    matchedBegins_.insert(begin);
    auto clause = new Clause(begin, end);
    foundClauses.insert(clause);
    clauseToInsts_[clause] = region;
    for (auto I : region) {
      instToClause_[I] = clause;
    }
  }
  loopToClauses_[LS] = foundClauses;
  clauses_.insert(foundClauses.begin(), foundClauses.end());
  errs() << "DTAnalysis: Info: Found "
         << clauses_.size() << " clauses\n";
}

void DTAnalysis::resolveClauses() {
  for (auto LS : targetLSs_) {
    resolveClauses(LS);
  }
}

void DTAnalysis::printClauses() const {
  for (auto C : clauses_) {
    C->print();
  }
}

set<const Clause*> DTAnalysis::canBeTerminated(Dependence *LCD) const {
  auto none = instToClause_.end();
  auto srcValue = cast<Instruction>(LCD->getSrcNode()->getT());
  auto dstValue = cast<Instruction>(LCD->getDstNode()->getT());
  auto srcClause = instToClause_.find(srcValue);
  auto dstClause = instToClause_.find(dstValue);

  if (srcClause == none && dstClause == none) {
    return {};
  } else if (srcClause != none && dstClause == none) {
    return {srcClause->second};
  } else if (srcClause == none && dstClause != none) {
    return {dstClause->second};
  } else if (srcClause != none && dstClause != none) {
    if (srcClause->second == dstClause->second) {
      return {srcClause->second};
    } else {
      return {srcClause->second, dstClause->second};
    }
  }
}

void DTAnalysis::categorizeDependences() {
  // here we use being `covered` meaning that an instruction
  // is in a region of code that belongs to a clause
  int notCovered = 0;
  int onlySrcCovered = 0;
  int onlyDstCovered = 0;
  int fullyCovered = 0;
  int crossCovered = 0;
  const auto none = instToClause_.end();
  for (auto LCD : candidateLCDs_) {
    auto srcValue = cast<Instruction>(LCD->getSrcNode()->getT());
    auto dstValue = cast<Instruction>(LCD->getDstNode()->getT());
    auto srcClause = instToClause_.find(srcValue);
    auto dstClause = instToClause_.find(dstValue);

    char *tag;
    if (srcClause == none && dstClause == none) {
      tag = "uncovered";
      notCovered++;
    } else if (srcClause != none && dstClause == none) {
      tag = "source-only-covered";
      onlySrcCovered++;
    } else if (srcClause == none && dstClause != none) {
      tag = "destination-only-covered";
      onlyDstCovered++;
    } else if (srcClause != none && dstClause != none) {
      if (srcClause->second == dstClause->second) {
        tag = "fully-covered";
        fullyCovered++;
      } else {
        tag = "cross-covered";
        crossCovered++;
      }
    }
    if (tag != "fully-covered") {
      errs() << "DTAnalysis: Dependence: Found " << tag << " LCD\n";
      printDependence(LCD);
    }
  }
  errs() << "DTAnalysis: Info: Found "
         << notCovered << " uncovered LCDs\n";
  errs() << "DTAnalysis: Info: Found "
         << fullyCovered << " fully-covered LCDs\n";
  errs() << "DTAnalysis: Info: Found "
         << crossCovered << " cross-covered LCDs\n";
  errs() << "DTAnalysis: Info: Found "
         << onlySrcCovered << " source-only-covered LCDs\n";
  errs() << "DTAnalysis: Info: Found "
         << onlyDstCovered << " destination-only-covered LCDs\n";
}

void DTAnalysis::sanityChecks() {
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
} // namespace arcana::terminator

// Registering pass

using namespace arcana::terminator;

char DTAnalysis::ID = 0;
static RegisterPass<DTAnalysis> X("dt-analysis", "Identifies opportunities for dependences termination");

static DTAnalysis *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker =
      new DTAnalysis());
    }
  }
);

static RegisterStandardPasses _RegPass2(
  PassManagerBuilder::EP_EnabledOnOptLevel0,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker = new DTAnalysis());
    }
  }
);
