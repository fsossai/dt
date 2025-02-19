#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "llvm/IR/Instructions.h"

#include "arcana/noelle/core/LoopCarriedUnknownSCC.hpp"
#include "arcana/noelle/core/LoopContent.hpp"
#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/gino/core/DOALL.hpp"

#include "arcana/dt/Analysis.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::gino;
using namespace arcana::noelle;

namespace arcana::dt {

TerminatorAnalysis::TerminatorAnalysis(
    Noelle &noelle,
    LoopForest *LF,
    Function &F,
    unordered_set<uint64_t> loopIDs,
    unordered_set<LoopContentOptimization> optimizations)
  : TerminatorAnalysis(noelle,
                       LF,
                       F,
                       loopIDs,
                       optimizations,
                       FULL | SRC_ONLY | DST_ONLY | CROSS) {}

TerminatorAnalysis::TerminatorAnalysis(
    Noelle &noelle,
    LoopForest *LF,
    Function &F,
    unordered_set<uint64_t> loopIDs,
    unordered_set<LoopContentOptimization> optimizations,
    CoverageType admissibleCoverage)
  : DependenceAnalysis("Terminator"),
    details(false),
    log(NoelleLumberjack, "Terminator.Analysis"),
    noelle(noelle),
    LF(LF),
    F(F),
    PF(F, "ldtc"),
    doallMarkers(F, "loop.doall"),
    admissibleCoverage(admissibleCoverage) {

  // We only care about loops with clauses. It is not the job of this analysis
  // to study loops that are unrelated to clauses even though they may be
  // parallel `TargetLSs` represents all loops that contain at least one clause.
  // Keep in mind that if a loop contains a clause all ancestors will too.

  this->MM = noelle.getMetadataManager();
  if (this->PF.getTrees().size() != 0) {
    log.info() << this->PF;
  }

  this->PF.visitPreOrder([&, this](PragmaTree *T, auto) -> bool {
    auto Begin = T->getBeginDelimiter();
    auto newClause = new TClause(*T);
    this->clauses.insert(newClause);
    log.info() << "Found: " << *newClause << "\n";

    auto InnerLT = LF->getInnermostLoopThatContains(Begin);
    if (InnerLT != nullptr) {
      // Build the ancestral path
      do {
        auto LS = InnerLT->getLoop();
        auto ID = LS->getID().value();
        this->relevantLoops.insert(LS);
        this->loopIdToClauses[ID].insert(newClause);
        InnerLT = InnerLT->getParent();
      } while (InnerLT != nullptr);
    }
    return false;
  });

  // Print relation among loops and clauses
  for (auto LS : this->relevantLoops) {
    auto ID = LS->getID().value();
    auto clauses = this->loopIdToClauses[ID];
    log.info() << "Loop" << getLoopDescription(LS) << ": Clauses: { ";
    for (auto clause : clauses) {
      log.info().noPrefix() << clause->getUniqueName() << " ";
    }
    log.info().noPrefix() << "}\n";
  }

  auto isSelected = [&](LoopStructure *LS) {
    if (loopIDs.size() == 0) {
      return true;
    }
    auto ID = LS->getID().value();
    return loopIDs.find(ID) != loopIDs.end();
  };

  // We keep the set of dependencies for which at least on instruction that
  // composes it is in contained in a clause. We call these `relevant` LCDs.

  for (auto LS : this->relevantLoops) {
    if (isSelected(LS)) {
      auto LC = noelle.getLoopContent(LS, optimizations);
      this->collectRelevantLCDs(LC);
    }
  }
}

TerminatorAnalysis::~TerminatorAnalysis() {
  for (auto clause : this->clauses) {
    delete clause;
  }
}

bool TerminatorAnalysis::canThisDependenceBeLoopCarried(Dependence *LCD,
                                                        LoopStructure &LS) {
  auto coverageType = this->getCoverageType(LCD);
  if (coverageType & this->admissibleCoverage) {
    return false;
  }
  return true;
}

bool TerminatorAnalysis::canThereBeAMemoryDataDependence(Instruction *src,
                                                         Instruction *dst,
                                                         LoopStructure &LS) {
  auto coverageType = getCoverageTypeFromPragmaTree(src, dst);
  if (coverageType & this->admissibleCoverage) {
    return false;
  }
  return true;
}

void TerminatorAnalysis::printDependence(Value *src, Value *dst) {
  auto s = log.namedSection("Dependence");
  log.info() << "[src] " << lepto(*src);
  log.info() << "[dst] " << lepto(*dst);
}

void TerminatorAnalysis::printDependence(Dependence *dep) {
  printDependence(dep->getSrc(), dep->getDst());
}

CoverageType TerminatorAnalysis::getCoverageType(Dependence *LCD) {
  for (auto &[currentLCD, coverageType] : this->relevantLCDs) {
    if (currentLCD->getSrc() == LCD->getSrc()
        && currentLCD->getDst() == LCD->getDst()) {
      return coverageType;
    }
  }
  return UNCOVERED;
}

CoverageType TerminatorAnalysis::getCoverageTypeFromPragmaTree(
    Dependence *LCD) {
  auto src = cast<Instruction>(LCD->getSrc());
  auto dst = cast<Instruction>(LCD->getDst());
  return getCoverageTypeFromPragmaTree(src, dst);
}

CoverageType TerminatorAnalysis::getCoverageTypeFromPragmaTree(
    Instruction *src,
    Instruction *dst) {

  auto srcClause = this->PF.findInnermostPragmaFor(src);
  auto dstClause = this->PF.findInnermostPragmaFor(dst);

  if (srcClause != nullptr && dstClause == nullptr) {
    return SRC_ONLY;
  }
  if (srcClause == nullptr && dstClause != nullptr) {
    return DST_ONLY;
  }
  if (srcClause != nullptr && dstClause != nullptr) {
    if (srcClause == dstClause) {
      return FULL;
    }
    return CROSS;
  }
  return UNCOVERED;
}

string TerminatorAnalysis::coverageToString(CoverageType coverageType) {
  switch (coverageType) {
    case UNCOVERED:
      return "uncovered";
    case SRC_ONLY:
      return "source-only-covered";
    case DST_ONLY:
      return "destination-only-covered";
    case CROSS:
      return "cross-covered";
    case FULL:
      return "fully-covered";
  }
  return "impossible"; // to slience warnings
}

int TerminatorAnalysis::getCoverageCount(LoopStructure *LS,
                                         CoverageType coverageType) {
  auto ID = LS->getID().value();
  auto coverageSummary = this->loopIdToCoverageSummary[ID];
  switch (coverageType) {
    case UNCOVERED:
      return coverageSummary.notCovered;
    case SRC_ONLY:
      return coverageSummary.srcOnlyCovered;
    case DST_ONLY:
      return coverageSummary.dstOnlyCovered;
    case CROSS:
      return coverageSummary.crossCovered;
    case FULL:
      return coverageSummary.fullyCovered;
  }
  return coverageSummary.notCovered; // to slience warnings
}

void TerminatorAnalysis::printCoverageSummary(LoopStructure *LS) {
  auto cTypes = vector{ UNCOVERED, SRC_ONLY, DST_ONLY, CROSS, FULL };

  for (auto cType : cTypes) {
    log.info() << getCoverageCount(LS, cType) << " " << coverageToString(cType)
               << "\n";
  }
}

void TerminatorAnalysis::collectRelevantLCDs(noelle::LoopContent *LC) {
  int lcdCounter = 0;

  auto LS = LC->getLoopStructure();
  auto ID = LS->getID().value();
  auto sccManager = LC->getSCCManager();
  auto SCCDAG = sccManager->getSCCDAG();

  CoverageSummary coverageSummary;

  for (auto sccNode : SCCDAG->getSCCs()) {
    auto genericSCC = sccManager->getSCCAttrs(sccNode);
    if (auto LCU = dyn_cast<LoopCarriedUnknownSCC>(genericSCC)) {
      auto LCDs = LCU->getLoopCarriedDependences();

      // Filtering out control dependencies
      for (auto LCD : LCDs) {
        if (!isa<ControlDependence<Value, Value>>(LCD)) {
          lcdCounter++;
          auto coverageType = this->getCoverageTypeFromPragmaTree(LCD);
          this->relevantLCDs[LCD] = coverageType;

          switch (coverageType) {
            case UNCOVERED:
              coverageSummary.notCovered++;
              break;
            case SRC_ONLY:
              coverageSummary.srcOnlyCovered++;
              break;
            case DST_ONLY:
              coverageSummary.dstOnlyCovered++;
              break;
            case CROSS:
              coverageSummary.crossCovered++;
              break;
            case FULL:
              coverageSummary.fullyCovered++;
              break;
          }
        }
      }
    }
  }

  this->loopIdToCoverageSummary[ID] = coverageSummary;
  this->loopIdToContent[ID] = LC;

  auto s = log.namedSection("Loop" + getLoopDescription(LS));
  log.info() << "Found " << lcdCounter << " unknown LCDs\n";

  this->printCoverageSummary(LS);
}

unordered_set<LoopStructure *> TerminatorAnalysis::getRelevantLoopStructures() {
  return this->relevantLoops;
}

string TerminatorAnalysis::getLoopDescription(LoopStructure *LS) {
  auto ID = LS->getID().value();
  return "(id=" + to_string(ID) + ")";
}

bool TerminatorAnalysis::isRelevant(noelle::LoopStructure *LS) const {
  auto ID = LS->getID().value();
  if (this->loopIdToCoverageSummary.find(ID)
      != this->loopIdToCoverageSummary.end()) {
    return true;
  }
  return false;
}

LoopContent *TerminatorAnalysis::fetchLoopContent(noelle::LoopStructure *LS) {
  auto ID = LS->getID().value();
  auto it = this->loopIdToContent.find(ID);
  if (it != this->loopIdToContent.end()) {
    return get<LoopContent *>(*it);
  }
  return nullptr;
}

unordered_set<TClause *> TerminatorAnalysis::getClausesOf(
    noelle::LoopStructure *LS) {
  auto ID = LS->getID().value();
  auto it = this->loopIdToClauses.find(ID);
  if (it != this->loopIdToClauses.end()) {
    return it->second;
  }
  return {};
}

DoallTag TerminatorAnalysis::getDoallTag(LoopStructure *LS) {
  auto T = this->doallMarkers.findInnermostPragmaFor(LS);
  if (T != nullptr) {
    auto args = T->getArguments();
    assert(args.size() == 1);
    StringRef str;
    PragmaTree::getStringFromArg(args[0], str);
    if (str.equals("yes")) {
      return DoallTag::YES;
    } else if (str.equals("no")) {
      return DoallTag::NO;
    } else if (str.equals("maybe")) {
      return DoallTag::MAYBE;
    } else {
      assert(false && "Unexpected DOALL tag");
    }
  }
  return DoallTag::MAYBE;
}

void TerminatorAnalysis::printUnknownLCDs(LoopContent *LC) {
  string prefix = "";
  auto sccManager = LC->getSCCManager();
  auto SCCNodes = DOALL::getSCCsThatBlockDOALLToBeApplicable(LC, noelle);
  using dep_t = pair<Value *, Value *>;
  set<size_t> toSkip;
  vector<dep_t> LCDs_seen;
  for (auto node : SCCNodes) {
    auto SCC = sccManager->getSCCAttrs(node);
    if (auto LCSCC = dyn_cast<LoopCarriedSCC>(SCC)) {
      for (auto dep : LCSCC->getLoopCarriedDependences()) {
        if (isa<ControlDependence<Value, Value>>(dep)) {
          continue;
        }
        auto src = dep->getSrc();
        auto dst = dep->getDst();
        bool depAlreadySeen = false;
        for (auto [otherSrc, otherDst] : LCDs_seen) {
          if (otherSrc == src && otherDst == dst) {
            depAlreadySeen = true;
            break;
          }
        }
        if (!depAlreadySeen) {
          LCDs_seen.push_back({ src, dst });
        }
      }
    }
  }
  for (size_t i = 0; i < LCDs_seen.size(); i++) {
    if (toSkip.find(i) != toSkip.end()) {
      continue;
    }
    auto src = LCDs_seen[i].first;
    auto dst = LCDs_seen[i].second;

    if (src == dst) {
      log.info() << " \u21bb " << lepto(*src) << "\n";
    } else {
      bool isSelf = false;
      for (size_t j = 0; j < LCDs_seen.size(); j++) {
        auto otherSrc = LCDs_seen[j].first;
        auto otherDst = LCDs_seen[j].second;
        if (src == otherDst && dst == otherSrc) {
          toSkip.insert(j);
          isSelf = true;
          break;
        }
      }

      if (isSelf) {
        log.info() << "\u250f\u2192 " << lepto(*src) << "\n";
      } else {
        log.info() << "\u250f\u2501 " << lepto(*src) << "\n";
      }
      log.info() << "\u2517\u2192 " << lepto(*dst) << "\n";
    }
  }
}

bool TerminatorAnalysis::isUnordered(LoopStructure *LS) {
  PragmaForest OrderPF(F, "unordered");
  bool unordered = false;
  for (auto LT : this->LF->getTrees()) {
    LT->visitPreOrder([&](LoopTree *T, auto) {
      auto LS = T->getLoop();
      auto p = OrderPF.findInnermostPragmaFor(LS);
      if (p != nullptr) {
        unordered = true;
        return true; // stop visit
      }
      return false;
    });
  }
  return unordered;
}

void TerminatorAnalysis::setAdmissibleCoverage(CoverageType coverage) {
  this->admissibleCoverage = coverage;
}

} // namespace arcana::dt
