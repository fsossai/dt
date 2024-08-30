#ifndef __TERMINATOR_ANALYSIS_HPP__
#define __TERMINATOR_ANALYSIS_HPP__

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "llvm/IR/Instructions.h"

#include "arcana/noelle/core/LoopContent.hpp"
#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/PragmaManager.hpp"

#include "TClause.hpp"

namespace arcana::dt {

enum CoverageType {
  NONE = 1,
  SRC_ONLY = 2,
  DST_ONLY = 4,
  CROSS = 8,
  FULL = 16
};

struct TerminatorAnalysis : public noelle::DependenceAnalysis {
  static char ID;

  struct CoverageSummary {
    int notCovered = 0;
    int srcOnlyCovered = 0;
    int dstOnlyCovered = 0;
    int crossCovered = 0;
    int fullyCovered = 0;
  };

  using Dependence = noelle::DGEdge<llvm::Value, llvm::Value>;

  TerminatorAnalysis(
      noelle::Noelle &noelle,
      noelle::LoopForest *LF,
      llvm::Function &F,
      std::unordered_set<noelle::LoopContentOptimization> optimizations);

  ~TerminatorAnalysis();

  bool canThisDependenceBeLoopCarried(Dependence *LCD,
                                      noelle::LoopStructure &LS) override;

  std::string coverageToString(CoverageType coverageType);

  CoverageType getCoverageType(Dependence *LCD);

  CoverageSummary getCoverageSummary(noelle::LoopStructure *LS);

  int getCoverageCount(noelle::LoopStructure *LS, CoverageType coverageType);

  void printCoverageSummary(noelle::LoopStructure *LS);

  std::unordered_set<noelle::LoopStructure *> getRelevantLoopStructures();

  noelle::LoopContent *fetchLoopContent(noelle::LoopStructure *LS);

  void printDependence(const Dependence *LCD) const;

  bool isRelevant(noelle::LoopStructure *LS) const;

  std::unordered_set<TClause *> getClausesOf(noelle::LoopStructure *LS);

  std::string getLoopDescription(noelle::LoopStructure *LS);

  bool details;

private:
  std::string prefix;
  noelle::Noelle &noelle;
  noelle::MetadataManager *MM;
  noelle::LoopForest *LF;
  llvm::Function &F;
  noelle::PragmaForest PF;
  std::unordered_set<TClause *> clauses;
  std::unordered_set<noelle::LoopStructure *> relevantLoops;
  std::unordered_map<Dependence *, CoverageType> relevantLCDs;
  std::unordered_map<uint64_t, CoverageSummary> loopIdToCoverageSummary;
  std::unordered_map<uint64_t, noelle::LoopContent *> loopIdToContent;
  std::unordered_map<uint64_t, std::unordered_set<TClause *>> loopIdToClauses;

  void collectRelevantLCDs(noelle::LoopContent *LC);

  CoverageType getCoverageTypeFromPragmaTree(Dependence *LCD);
};
} // namespace arcana::dt

#endif // __TERMINATOR_ANALYSIS_HPP__
