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

#include "Analysis.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

namespace arcana::dt {

TerminatorAnalysis::TerminatorAnalysis(
    Noelle &noelle,
    LoopForest *LF,
    Function &F,
    unordered_set<LoopContentOptimization> optimizations)
  : DependenceAnalysis("Terminator"),
    details(false),
    noelle(noelle),
    LF(LF),
    F(F),
    PF(F, "ldtc"),
    doallMarkers(F, "loop.doall"),
    log(NoelleLumberjack, "Terminator.Analysis") {

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

  populateLoopTags();

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

  // We keep the set of dependencies for which at least on instruction that
  // composes it is in contained in a clause. We call these `relevant` LCDs.

  for (auto LS : this->relevantLoops) {
    auto LC = noelle.getLoopContent(LS, optimizations);
    this->collectRelevantLCDs(LC);
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
  if (coverageType & (FULL | SRC_ONLY | DST_ONLY | CROSS)) {
    return false;
  }
  return true;
}

void TerminatorAnalysis::printDependence(const Dependence *LCD) {
  auto srcValue = LCD->getSrc();
  auto dstValue = LCD->getDst();
  auto dep = log.namedSection("Dependence");
  log.info() << "[src] " << LIV.visitValue(*srcValue);
  log.info() << "[dst] " << LIV.visitValue(*dstValue);
}

CoverageType TerminatorAnalysis::getCoverageType(Dependence *LCD) {
  for (auto &[currentLCD, coverageType] : this->relevantLCDs) {
    if (currentLCD->getSrc() == LCD->getSrc()
        && currentLCD->getDst() == LCD->getDst()) {
      return coverageType;
    }
  }
  return NONE;
}

CoverageType TerminatorAnalysis::getCoverageTypeFromPragmaTree(
    Dependence *LCD) {
  auto srcValue = cast<Instruction>(LCD->getSrc());
  auto dstValue = cast<Instruction>(LCD->getDst());

  auto srcClause = this->PF.findInnermostPragmaFor(srcValue);
  auto dstClause = this->PF.findInnermostPragmaFor(dstValue);

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
  return NONE;
}

string TerminatorAnalysis::coverageToString(CoverageType coverageType) {
  switch (coverageType) {
    case NONE:
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
    case NONE:
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
  auto cTypes = vector{ NONE, SRC_ONLY, DST_ONLY, CROSS, FULL };

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
            case NONE:
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
  assert(false && "Cannot get claues for non relevant loops");
}

string TerminatorAnalysis::getLoopDescription(LoopStructure *LS) {
  auto ID = LS->getID().value();
  auto order = this->MM->getMetadata(LS, "noelle.parallelizer.looporder");
  auto tag =
      (this->loopIdToTag[ID] == 0) ? "" : to_string(this->loopIdToTag[ID]);
  return "(id=" + to_string(ID) + ", tag=" + tag + ", order=" + order + ")";
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

void TerminatorAnalysis::populateLoopTags() {
  PragmaForest LoopPF(F, "loop.tag");

  for (auto LS : this->relevantLoops) {
    auto ID = LS->getID().value();
    auto BranchI = LS->getHeader()->getTerminator();
    auto p = LoopPF.findInnermostPragmaFor(BranchI);
    if (p == nullptr) {
      log.info() << "WARNING: loop.id=" << ID
                 << " does not have a loop.tag attribute\n";
      this->loopIdToTag[ID] = 0;
    } else {
      auto args = p->getArguments();
      assert(args.size() >= 1);
      auto tag = cast<ConstantInt>(args[0]);
      this->loopIdToTag[ID] = tag->getZExtValue();
    }
  }
}

uint64_t TerminatorAnalysis::getLoopTag(LoopStructure *LS) {
  auto ID = LS->getID().value();
  if (loopIdToTag.find(ID) != loopIdToTag.end()) {
    return loopIdToTag[ID];
  }
  return 0;
}

} // namespace arcana::dt
