#ifndef __TERMINATOR_ANALYSIS_HPP__
#define __TERMINATOR_ANALYSIS_HPP__

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "llvm/IR/Instructions.h"

#include "arcana/noelle/core/LoopContent.hpp"
#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/Lumberjack.hpp"
#include "LeptoInstVisitor.hpp"

#include "TClause.hpp"

namespace arcana::dt {

enum CoverageType_value {
  NONE = 0, 
  UNCOVERED = 1 << 0,
  SRC_ONLY = 1 << 1,
  DST_ONLY = 1 << 2,
  CROSS = 1 << 3,
  FULL = 1 << 4
};

using CoverageType = unsigned int;

enum DoallTag { YES, NO, MAYBE };

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

  TerminatorAnalysis(
      noelle::Noelle &noelle,
      noelle::LoopForest *LF,
      llvm::Function &F,
      std::unordered_set<noelle::LoopContentOptimization> optimizations,
      CoverageType admissibleCoverage);

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

  void printDependence(const Dependence *LCD);

  void printUnknownLCDs(noelle::LoopContent *LC);

  bool isRelevant(noelle::LoopStructure *LS) const;

  std::unordered_set<TClause *> getClausesOf(noelle::LoopStructure *LS);

  std::string getLoopDescription(noelle::LoopStructure *LS);

  DoallTag getDoallTag(noelle::LoopStructure *LS);

  uint64_t getLoopTag(noelle::LoopStructure *LS);

  void setAdmissibleCoverage(CoverageType coverage);

  bool details;

private:
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
  std::unordered_map<uint64_t, uint64_t> loopIdToTag;
  noelle::PragmaForest doallMarkers;
  CoverageType admissibleCoverage;
  LeptoInstVisitor LIV;
  noelle::Logger log;

  void collectRelevantLCDs(noelle::LoopContent *LC);

  CoverageType getCoverageTypeFromPragmaTree(Dependence *LCD);

  void populateLoopTags();
};

} // namespace arcana::dt

#endif // __TERMINATOR_ANALYSIS_HPP__
