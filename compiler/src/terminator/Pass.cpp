#include <algorithm>
#include <set>
#include <stack>
#include <unordered_set>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"

#include "arcana/gino/core/DOALL.hpp"
#include "arcana/gino/core/HeuristicsPass.hpp"
#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/NoellePass.hpp"
#include "arcana/noelle/core/PragmaAnalysis.hpp"

#include "arcana/dt/LoopBlocker.hpp"
#include "arcana/dt/Analysis.hpp"
#include "Pass.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;
using namespace arcana::gino;

namespace arcana::dt {

static cl::opt<int> NumBreaks("terminator-breaks",
                              cl::ZeroOrMore,
                              cl::init(0),
                              cl::Hidden,
                              cl::desc("Number of times we break a LCD"));

static cl::opt<string> TargetFunc("terminator-func",
                                  cl::init(""),
                                  cl::Hidden,
                                  cl::desc("Run on one function only"));

static cl::opt<bool> EraseClauses("erase-clauses",
                                  cl::ZeroOrMore,
                                  cl::init(false),
                                  cl::Hidden,
                                  cl::desc("Removes calls to LDTC pragmas"));

static cl::opt<bool> Details("terminator-details",
                             cl::ZeroOrMore,
                             cl::init(false),
                             cl::Hidden,
                             cl::desc("Show analysis details"));

static cl::opt<bool> TaggedOnly(
    "terminator-tagged-only",
    cl::ZeroOrMore,
    cl::init(false),
    cl::Hidden,
    cl::desc("Only terminate loops with a loop.tag attribute"));

static cl::list<int> CraftPlan("terminator-craft-plan",
                               cl::ZeroOrMore,
                               cl::CommaSeparated,
                               cl::Hidden,
                               cl::desc("A new parallel plan is generated"));

TerminatorPass::TerminatorPass()
  : ModulePass{ ID },
    log(NoelleLumberjack, "Terminator.Pass") {}

bool TerminatorPass::doInitialization(Module &M) {
  return false;
}

void TerminatorPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<NoellePass>();
  AU.addRequired<HeuristicsPass>();
}

bool TerminatorPass::runOnModule(Module &M) {
  auto &noelle = getAnalysis<NoellePass>().getNoelle();
  this->MM = noelle.getMetadataManager();

  auto &LF = *noelle.getLoopNestingForest();
  int lastLoopOrder = 0;

  // Pragma must not interfere with dependences
  PragmaAnalysis PA;
  noelle.addAnalysis(&PA);

  log.debug() << "Hello\n";

  if (TargetFunc != "") {
    log.info() << "Running pass only on " << TargetFunc << "\n";
    auto &F = *M.getFunction(TargetFunc);
    if (!F.empty()) {
      runOnFunction(noelle, LF, F, lastLoopOrder);
    }
  } else {
    for (auto &F : M) {
      if (!F.empty()) {
        runOnFunction(noelle, LF, F, lastLoopOrder);
      }
    }
  }

  return true;
}

bool TerminatorPass::runOnFunction(Noelle &noelle,
                                   LoopForest &LF,
                                   Function &F,
                                   int &lastLoopOrder) {
  this->MM = noelle.getMetadataManager();

  if (EraseClauses) {
    assert(false && "Unimplemented");
  }

  // Phase 1
  // Indentifying which loops to analyze

  // Collecting the embedded parallel plan
  auto &functionLSs = *noelle.getLoopStructures(&F);
  set<LoopStructure *> loopsAlreadyInPlan;
  for (auto LS : functionLSs) {
    if (MM->doesHaveMetadata(LS, "noelle.parallelizer.looporder")) {
      loopsAlreadyInPlan.insert(LS);
    }
  }

  populateLoopTags(noelle, F, functionLSs);

  if (CraftPlan.getNumOccurrences() > 0) {
    assert(loopsAlreadyInPlan.size() == 0
           && "A crafted plan is requested but there is one already");
  }

  unordered_set<LoopStructure *> plannedLSs;
  // A new plan may or may not be crafted
  for (auto LS : functionLSs) {
    bool addToPlan = false;
    if (CraftPlan.getNumOccurrences() > 0) {
      auto Tag = getLoopTag(LS);
      if (Tag != 0
          && std::find(CraftPlan.begin(), CraftPlan.end(), Tag)
                 != std::end(CraftPlan)) {
        addToPlan = true;
      }
    } else {
      if (TaggedOnly) {
        if (getLoopTag(LS) != 0) {
          addToPlan = true;
        }
      } else {
        addToPlan = true;
      }
    }
    if (addToPlan) {
      plannedLSs.insert(LS);
    }
  }

  if (plannedLSs.size() == 0) {
    return false;
  }

  // setting metadata to create the parallel plan
  for (auto LS : plannedLSs) {
    auto MD = "noelle.parallelizer.looporder";
    if (this->MM->doesHaveMetadata(LS, MD)) {
      this->MM->setMetadata(LS, MD, to_string(lastLoopOrder++));
    } else {
      this->MM->addMetadata(LS, MD, to_string(lastLoopOrder++));
    }
  }

  // Printing plan information
  log.info() << "Parallel plan { ";
  for (auto LS : plannedLSs) {
    log.info().noPrefix() << getLoopDescription(LS) << " ";
  }
  log.info().noPrefix() << "}\n";

  unordered_set<uint64_t> plannedIDs;
  for (auto LS : plannedLSs) {
    auto ID = LS->getID().value();
    plannedIDs.insert(ID);
  }

  auto optimizations = { LoopContentOptimization::MEMORY_CLONING_ID,
                         LoopContentOptimization::THREAD_SAFE_LIBRARY_ID };

  TerminatorAnalysis TA(noelle, &LF, F, plannedIDs, optimizations);
  TA.details = Details;

  // Phase 2
  // Identifying non-DOALL loops from the loop with clauses

  set<LoopStructure *> retryLSs;
  const gino::DOALL doall(noelle);
  auto heuristics = getAnalysis<HeuristicsPass>().getHeuristics(noelle);

  for (auto *LS : plannedLSs) {
    auto LD = getLoopDescription(LS);
    auto LC = TA.fetchLoopContent(LS);
    if (LC == nullptr) {
      // This loop might be in the plan but might not have been analyzed by TA
      LC = noelle.getLoopContent(LS);
    }
    bool isDOALL = doall.canBeAppliedToLoop(LC, heuristics);

    log.info()
        << "Loop" << LD << ": DOALL: " << (isDOALL ? "yes" : "no") << "\n";

    auto isUnordered = TA.isUnordered(LS);
    log.info() << "Loop" << LD
               << ": Unordered: " << (isUnordered ? "yes" : "no") << "\n";

    if (isDOALL) {
      // Marking the new loop as DOALL
      MM->addMetadata(LS, "gino.doall", "yes");
      MM->addMetadata(LS, "tc.order", isUnordered ? "no" : "yes");
    } else {
      retryLSs.insert(LS);
    }
  }

  // Phase 3
  // Let's give the non-DOALL loops a second chance by exploiting
  // the termination clauses.
  // We collect LoopContents that can be DOALL

  set<LoopContent *> terminationTargetLCs;

  noelle.addAnalysis(&TA);
  log.info() << "Added Termination engine to Noelle\n";

  for (auto *LS : retryLSs) {
    auto LC = noelle.getLoopContent(LS, optimizations);
    auto LD = getLoopDescription(LS);
    auto cert = doall.getCertificate(LC, heuristics);
    bool looksDoall =
        cert == DOALL::Certificate::YES || cert == DOALL::Certificate::NO_IV;

    log.info() << "Loop" << LD << ": DOALL+TC: " << (looksDoall ? "yes" : "no");

    bool treatAsDoall = false;
    auto doallTag = TA.getDoallTag(LS);
    switch (doallTag) {
      case YES:
        treatAsDoall = true;
        log.info().noPrefix() << " (marked yes)";
        break;
      case NO:
        treatAsDoall = false;
        log.info().noPrefix() << " (marked no)";
        break;
      case MAYBE:
        treatAsDoall = looksDoall;
        break;
    }

    log.info().noPrefix() << "\n";

    if (!looksDoall) {
      TA.printUnknownLCDs(LC);
    }

    if (treatAsDoall) {
      terminationTargetLCs.insert(LC);
    } else {
      // Despite the termination clauses, this loop is stil hopeless
      MM->addMetadata(LS, "gino.doall", "no");
    }
  }

  // Phase 4
  // Applying loop blocking transformation to the termination targets.
  // This phase only applies to the collected DOALL loops

  IRBuilder<> Builder(F.getContext());
  auto &M = *F.getParent();
  Value *NumBlocks;
  if (NumBreaks < 0) {
    auto &EntryBB = F.getEntryBlock();
    NumBlocks = Builder.CreateCall(M.getOrInsertFunction(
        "omp_get_max_threads",
        FunctionType::get(Builder.getInt32Ty(), {}, /*isVarArg=*/false)));
    auto NumBlocksI = cast<Instruction>(NumBlocks);
    NumBlocksI->insertBefore(EntryBB.getTerminator());
  } else {
    NumBlocks = Builder.getInt32(NumBreaks + 1);
  }

  for (auto LC : terminationTargetLCs) {
    auto LS = LC->getLoopStructure();
    auto LD = getLoopDescription(LS);
    auto isUnordered = TA.isUnordered(LS);
    PHINode *NewIVPHI = nullptr;
    BasicBlock *NewHeader;
    if (!isUnordered) {
      if (NumBreaks < 0) {
        log.info() << "Loop" << LD << ": Blocks: auto\n";
      } else {
        log.info() << "Loop" << LD << ": Blocks: " << (NumBreaks + 1) << "\n";
      }
    }
    bool willBeUnordered = isUnordered;
    if (isUnordered) {
      NewHeader = LS->getHeader();
      auto IVM = LC->getInductionVariableManager();
      auto LGIV = IVM->getLoopGoverningInductionVariable();

      if (LGIV == nullptr) {
        log.info()
            << "WARNING: " << LD << " does not have a LGIV (implies ordered)\n";
        willBeUnordered = false;
      } else {
        NewIVPHI = LGIV->getInductionVariable()->getLoopEntryPHI();
      }
    }
    if (!willBeUnordered) {
      NewHeader = blockLoop(LC, NumBlocks, &NewIVPHI);
    }
    assert(NewHeader != nullptr && "Failed to block to loop");
    assert(NewIVPHI != nullptr);

    // The old preheader is still stored in LS.
    // At this point, this is the preheader of the new loop introduced
    // by the loop blocking transformation.
    auto PreHeader = LS->getPreHeader();
    auto ClauseInsertionPoint = PreHeader->getTerminator();

    // Applying termination clauses
    int clauseID = 0;
    for (auto clause : TA.getClausesOf(LS)) {
      log.info() << "Loop" << LD << ": Handling: " << *clause << "\n";
      if (clause->isStrong()) {
        // This kind of clauses don't need to be handled
        continue;
      }

      // Solving for an earlier location of the arguments.
      // By contract, we must find a pointer value that dominates
      // the PreHeader.
      auto AdjustedCallArgs = clause->getCallArguments();
      auto F = LS->getHeader()->getParent();
      DominatorTree DT(*F);
      DT.recalculate(*F); // because a new BasicBlock has been created

      for (auto &A : AdjustedCallArgs) {
        if (!isa<Instruction>(A)) {
          // We assume the Value is available
          continue;
        }
        auto CurrentDef = dyn_cast<Instruction>(A);

        // Construct the def-use chain back to the origin
        stack<Instruction *> defUseChain;
        while (!DT.dominates(CurrentDef, ClauseInsertionPoint)) {
          if (auto GEP = dyn_cast<GetElementPtrInst>(CurrentDef)) {
            defUseChain.push(CurrentDef);
            CurrentDef = cast<Instruction>(GEP->getPointerOperand());
          } else if (isa<AllocaInst>(CurrentDef)) {
            break;
          } else {
            log.bypass() << "ERROR: Unhandled\n";
            log.bypass() << *A << "\n";
            abort();
          }
        }

        // Re-construct a new def-use chain in which all instructions
        // dominates any call to a clause function
        auto LastNewDef = CurrentDef;
        while (!defUseChain.empty()) {
          auto Def = defUseChain.top();
          defUseChain.pop();
          auto NewDef = Def->clone();
          assert(isa<GetElementPtrInst>(NewDef));
          NewDef->setOperand(0, LastNewDef);
          PreHeader->getInstList().insert(Builder.GetInsertPoint(), NewDef);
          LastNewDef = NewDef;
        }

        A = LastNewDef;
      }

      // The first argument is always `NumBlocks` by contract
      AdjustedCallArgs.insert(AdjustedCallArgs.begin(), NumBlocks);
      Builder.SetInsertPoint(ClauseInsertionPoint);
      Builder.CreateCall(clause->getFunction(), AdjustedCallArgs);

      // Type manipulation of the `t` induction variable
      // auto SrcTy = NewIVPHI->getType();
      Value *Replacement;
      auto DestTy = clause->getVariable()->getType()->getPointerElementType();
      Builder.SetInsertPoint(clause->getPragmaTree().getBeginDelimiter());
      if (isUnordered) {
        auto ThreadNum = Builder.CreateCall(M.getOrInsertFunction(
            "omp_get_thread_num",
            FunctionType::get(Builder.getInt32Ty(), {}, /*isVarArg=*/false)));
        Replacement = Builder.CreateZExtOrTrunc(ThreadNum, DestTy);
      } else {
        Replacement = Builder.CreateZExtOrTrunc(NewIVPHI, DestTy);
      }
      Builder.CreateStore(Replacement, clause->getVariable());
      clauseID++;
    }

    // Moving the looporder metadata to the new outer loop
    auto LO = MM->getMetadata(LS, "noelle.parallelizer.looporder");
    MM->deleteMetadata(LS->getHeader()->getTerminator(),
                       "noelle.parallelizer.looporder");
    MM->addMetadata(NewHeader->getTerminator(),
                    "noelle.parallelizer.looporder",
                    LO);

    // Marking the new loop as DOALL
    MM->addMetadata(NewHeader->getTerminator(), "gino.doall", "yes");
    MM->addMetadata(NewHeader->getTerminator(),
                    "tc.order",
                    isUnordered ? "no" : "yes");
  }

  return true;
}

void TerminatorPass::populateLoopTags(Noelle &noelle,
                                      Function &F,
                                      const vector<LoopStructure *> &LSs) {
  PragmaForest LoopPF(F, "loop.tag");

  auto LF = noelle.organizeLoopsInTheirNestingForest(LSs);

  for (auto LT : LF->getTrees()) {
    LT->visitPreOrder([&](LoopTree *T, auto) {
      auto LS = T->getLoop();
      auto ID = LS->getID().value();
      auto p = LoopPF.findInnermostPragmaFor(LS);
      if (p != nullptr) {
        auto args = p->getArguments();
        assert(args.size() >= 1);
        auto tag = cast<ConstantInt>(args[0])->getZExtValue();
        this->loopIdToTag[ID] = tag;
        this->loopTagToId[tag] = ID;
        return true; // stop visit
      }
      return false;
    });
  }
}

string TerminatorPass::getLoopDescription(LoopStructure *LS) {
  auto ID = LS->getID().value();
  auto order = this->MM->getMetadata(LS, "noelle.parallelizer.looporder");
  auto tag =
      (this->loopIdToTag[ID] == 0) ? "" : to_string(this->loopIdToTag[ID]);
  return "(id=" + to_string(ID) + ", tag=" + tag + ", order=" + order + ")";
}

uint64_t TerminatorPass::getLoopTag(LoopStructure *LS) {
  auto ID = LS->getID().value();
  if (this->loopIdToTag.find(ID) != this->loopIdToTag.end()) {
    return this->loopIdToTag[ID];
  }
  return 0;
}

char TerminatorPass::ID = 0;
static RegisterPass<TerminatorPass> X("Terminator",
                                      "Enabler that uses Termination Clauses",
                                      false,
                                      false);

} // namespace arcana::dt
