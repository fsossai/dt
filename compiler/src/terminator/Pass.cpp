#include <algorithm>
#include <set>
#include <stack>

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
#include "Analysis.hpp"
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
                                  cl::desc("Run only on one function"));

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

static cl::opt<bool> CraftPlan(
    "terminator-craft-plan",
    cl::ZeroOrMore,
    cl::init(false),
    cl::Hidden,
    cl::desc("Only target loops become part of the plan"));

TerminatorPass::TerminatorPass()
  : ModulePass{ ID },
    prefix("Terminator: Pass: ") {}

bool TerminatorPass::doInitialization(Module &M) {
  return false;
}

void TerminatorPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<NoellePass>();
  AU.addRequired<HeuristicsPass>();

  return;
}

bool TerminatorPass::runOnModule(Module &M) {
  auto &noelle = getAnalysis<NoellePass>().getNoelle();
  auto &LF = *noelle.getLoopNestingForest();
  int lastLoopOrder = 0;

  if (TargetFunc != "") {
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
  auto MM = noelle.getMetadataManager();

  if (EraseClauses) {
    assert(false && "Unimplemented");
  }

  // Phase 1
  // Indentifying which loops to analyze

  // Collecting the embedded parallel plan
  set<LoopStructure *> loopsInPlan;
  for (auto LS : *noelle.getLoopStructures(&F)) {
    if (MM->doesHaveMetadata(LS, "noelle.parallelizer.looporder")) {
      loopsInPlan.insert(LS);
    }
  }

  auto optimizations = { LoopContentOptimization::MEMORY_CLONING_ID,
                         LoopContentOptimization::THREAD_SAFE_LIBRARY_ID };

  // Pragma must not interfere with dependences
  PragmaAnalysis PA;
  noelle.addAnalysis(&PA);

  TerminatorAnalysis TA(noelle, &LF, F, optimizations);
  TA.details = Details;

  auto relevantLoops = TA.getRelevantLoopStructures();
  if (relevantLoops.size() == 0) {
    return false;
  }

  // A new plan may or may not be crafted
  if (loopsInPlan.size() == 0) {
    // Analyze all loops that contain termination clauses
    if (CraftPlan) {
      for (auto LS : relevantLoops) {
        MM->addMetadata(LS,
                        "noelle.parallelizer.looporder",
                        to_string(lastLoopOrder++));
      }
    }

    // Printing new plan information
    errs() << prefix << "Crafted a new parallel plan { ";
    for (auto LS : relevantLoops) {
      errs() << TA.getLoopDescription(LS) << " ";
    }
    errs() << "}\n";
  } else {
    if (CraftPlan) {
      assert(false && "A crafted plan is requested but there is one already");
    }
  }

  // Phase 2
  // Identifying non-DOALL loops from the loop with clauses

  set<LoopStructure *> retryLSs;
  const gino::DOALL doall(noelle);
  auto heuristics = getAnalysis<HeuristicsPass>().getHeuristics(noelle);

  for (auto *LS : relevantLoops) {
    auto LC = TA.fetchLoopContent(LS);
    assert(LC != nullptr);
    auto LD = TA.getLoopDescription(LS);
    bool isDOALL = doall.canBeAppliedToLoop(LC, heuristics);

    errs() << this->prefix << "Loop" << LD
           << ": DOALL: " << (isDOALL ? "yes" : "no") << "\n";

    if (isDOALL) {
      MM->addMetadata(LS, "gino.doall", "yes");
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
  errs() << this->prefix << "Added Termination engine to Noelle\n";

  for (auto *LS : retryLSs) {
    auto LC = noelle.getLoopContent(LS, optimizations);
    auto LD = TA.getLoopDescription(LS);
    auto cert = doall.getCertificate(LC, heuristics);
    bool isDOALL =
        cert == DOALL::Certificate::YES || cert == DOALL::Certificate::NO_IV;

    errs() << this->prefix << "Loop" << LD
           << ": DOALL+TC: ";

    bool treatAsDoall = false;

    if (isDOALL) {
      errs() << "yes";
      treatAsDoall = true;
    } else {
      if (TA.isMarkedDoall(LS)) {
        errs() << "no (marked yes)";
        treatAsDoall = true;
      } else {
        errs() << "no";
        treatAsDoall = false;
      }
    }

    errs() << "\n";

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
  const int NumBlocks = NumBreaks + 1;

  for (auto LC : terminationTargetLCs) {
    auto LS = LC->getLoopStructure();
    BasicBlock *NewHeader = blockLoop(LC, NumBlocks);
    auto LD = TA.getLoopDescription(LS);
    assert(NewHeader != nullptr && "Failed to block to loop");
    errs() << this->prefix << "Loop" << LD << ": Blocks: " << NumBlocks << "\n";

    auto &FirstPHI = *NewHeader->phis().begin();

    // The old preheader is still stored in LS.
    // At this point, this is the preheader of the new loop introduced
    // by the loop blocking transformation.
    auto PreHeader = LS->getPreHeader();
    auto Zero = Builder.getInt32(0);
    auto ID = LS->getID().value();

    // Applying termination clauses
    int clauseID = 0;
    for (auto clause : TA.getClausesOf(LS)) {
      errs() << this->prefix << "Loop" << LD << ": Handling: ";
      clause->print(errs()) << "\n";
      Builder.SetInsertPoint(PreHeader->getTerminator());
      if (clause->isStrong()) {
        // This kind of clauses don't need to be handled
        continue;
      }
      auto ClauseFuncName = clause->getFunction()->getName().str();
      auto ClausePtrTy = clause->getVariable()->getType();
      auto ClauseElemTy = ClausePtrTy->getPointerElementType();
      auto ArrayTy = ArrayType::get(ClauseElemTy, NumBlocks);
      auto TCValuesName = "TCValues." + clause->getUniqueName() + ".loopid."
                          + to_string(ID) + "." + ClauseFuncName;

      Builder.CreateLoad(ClauseElemTy, clause->getVariable());
      auto TCValues = Builder.CreateAlloca(ArrayTy, nullptr, TCValuesName);

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
        auto ClauseInsertionPoint = NewHeader->getTerminator();

        // Construct the def-use chain back to the origin
        stack<Instruction *> defUseChain;
        while (!DT.dominates(CurrentDef, ClauseInsertionPoint)) {
          if (auto GEP = dyn_cast<GetElementPtrInst>(CurrentDef)) {
            defUseChain.push(CurrentDef);
            CurrentDef = cast<Instruction>(GEP->getPointerOperand());
          } else if (isa<AllocaInst>(CurrentDef)) {
            break;
          } else {
            errs() << this->prefix << "ERROR: Unhandled\n";
            errs() << *A << "\n";
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

      // Creating a new value for each clause variable.
      // This is achieved by generating the necessary calls
      // to the clause function
      for (int i = 0; i < NumBlocks; i++) {
        auto GEP = Builder.CreateInBoundsGEP(ArrayTy,
                                             TCValues,
                                             { Zero, Builder.getInt32(i) });
        if (i == 0) {
          Builder.CreateStore(clause->getDefaultValue(), GEP);
        } else {
          auto TCValue =
              Builder.CreateCall(clause->getFunction(), AdjustedCallArgs);
          Builder.CreateStore(TCValue, GEP);
        }
      }

      // Patching the clause variable with a value from TCValues
      Builder.SetInsertPoint(NewHeader->getTerminator());
      auto GEP =
          Builder.CreateInBoundsGEP(ArrayTy, TCValues, { Zero, &FirstPHI });
      auto LoadTCValue = Builder.CreateLoad(ClauseElemTy, GEP);
      Builder.SetInsertPoint(clause->getPragmaTree().getBeginDelimiter());
      Builder.CreateStore(LoadTCValue, clause->getVariable());
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
  }

  return true;
}

char TerminatorPass::ID = 0;
static RegisterPass<TerminatorPass> X("Terminator",
                                      "Enabler that uses Termination Clauses",
                                      false,
                                      false);

} // namespace arcana::dt
