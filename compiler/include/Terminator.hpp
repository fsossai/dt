#ifndef __TERMINATOR_HPP__
#define __TERMINATOR_HPP__

#include <map>
#include <set>

#include "llvm/IR/Instructions.h"
#include "noelle/core/Noelle.hpp"

namespace arcana::terminator {

class Clause {
public:
  Clause(llvm::Instruction *begin, llvm::Instruction *end);

  void print() const;

  void extractClauseOperands();

  llvm::Value *variable;
  llvm::Function *function;
  llvm::Instruction *begin;
  llvm::Instruction *end;
};

struct Analysis : public llvm::ModulePass {
  static char ID;

  using Dependence = noelle::DGEdge<llvm::Value, llvm::Value>;

  Analysis();

  ~Analysis();

  bool doInitialization(Module &M) override;

  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  void printDependence(const Dependence *LCD) const;

  bool isLDTCBegin(const llvm::Instruction *I) const;

  bool isLDTCEnd(const llvm::Instruction *I) const;

  bool isLDTC(const llvm::Instruction *I) const;

  std::set<llvm::Instruction*> getPragmasInLoop(noelle::LoopStructure *LS) const;

  void findCandidates();

  bool isUnmatched(llvm::Instruction *begin) const;

  llvm::Instruction *findUnmatchedBegin(llvm::BasicBlock *BB) const;

  llvm::Instruction *findMatchingBeginSingleBlock(llvm::Instruction *end) const;

  std::set<llvm::Instruction*> findMatchingBegin(llvm::Instruction *end, llvm::Instruction **beginFound);

  void resolveClauses(noelle::LoopStructure *LS);

  void resolveClauses();

  void printClauses() const;

  std::set<const Clause*> canBeTerminated(Dependence *LCD) const;

  void categorizeDependences();

  void sanityChecks();

  void recomputeLDG();

private:
  std::set<Clause*> clauses_;
  std::map<noelle::LoopStructure*, std::set<Clause*>> loopToClauses_;
  std::map<llvm::Instruction*, Clause*> instToClause_;
  std::map<Clause*, std::set<llvm::Instruction*>> clauseToInsts_;
  std::set<noelle::LoopStructure*> candidateLSs_;
  std::set<noelle::LoopStructure*> targetLSs_;
  std::set<Dependence*> candidateLCDs_;
  std::map<noelle::LoopStructure*, std::set<llvm::Instruction*>> loopToPragmas_;
  std::set<llvm::Instruction*> matchedBegins_;

};
} // namespace arcana::terminator

#endif // __TERMINATOR_HPP__
