#include <set>

#include "llvm/IR/Instructions.h"

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/dt/LoopBlocker.hpp"

using namespace std;

using namespace arcana::noelle;

namespace arcana::dt {
BasicBlock *blockLoop(LoopContent *LC, int numBlocks, PHINode **NewIVPHI) {
  auto F = LC->getLoopStructure()->getFunction();
  auto &Context = F->getContext();
  auto NumBlocks = ConstantInt::get(Type::getInt32Ty(Context), numBlocks);
  return blockLoop(LC, NumBlocks, NewIVPHI);
}

BasicBlock *blockLoop(LoopContent *LC, Value *NumBlocks, PHINode **NewIVPHI) {
  auto LS = LC->getLoopStructure();
  auto IVM = LC->getInductionVariableManager();
  auto LGIV = IVM->getLoopGoverningInductionVariable(*LC->getLoopStructure());
  auto LGInnerPHI =
      LGIV ? LGIV->getInductionVariable()->getLoopEntryPHI() : nullptr;
  auto InnerLatches = LS->getLatches();

  auto F = LS->getFunction();
  auto &Context = F->getContext();
  IRBuilder<> Builder(Context);

  auto IVs = IVM->getInductionVariables();

  auto InnerHeader = LS->getHeader();
  auto InnerPreheader = LS->getPreHeader();

  auto OuterHeader = BasicBlock::Create(Context, "", F);

  Value *InnerOriginalStartIdx = nullptr;

  // errs() << "=========================================================\n";
  // errs() << "NUM LATCHES = " << LS->getLatches().size() << "\n";

  // for (auto IV : IVM->getInductionVariables()) {
  //   errs() << "IV\n";
  //   for (auto PHI : IV->getPHIs()) {
  //     errs() << " PHI" << *PHI << "\n";
  //   }
  // }

  if (LGInnerPHI) {
    errs() << "LoopBlocker: LGInnerPHI = " << *LGInnerPHI << "\n";
    // TODO can be simplified through the use of the preheader
    // All predecessors of the original header (InnerHeader) must
    // now branch to the new header
    for (size_t i = 0; i < LGInnerPHI->getNumIncomingValues(); i++) {
      auto BB = LGInnerPHI->getIncomingBlock(i);
      if (InnerLatches.find(BB) == InnerLatches.end()) {
        // BB is not a latch of the loop.
        // It needs to be rewired to the new header
        BB->getTerminator()->replaceSuccessorWith(InnerHeader, OuterHeader);

        // The induction variable must have only one initial value
        assert(InnerOriginalStartIdx == nullptr);
        InnerOriginalStartIdx = LGInnerPHI->getIncomingValue(i);
      }
    }
  } else {
    errs() << "LoopBlocker: No LGInnerPHI\n";
  }

  auto OuterLatch = BasicBlock::Create(Context, "", F);

  // When exiting the loop, we now go to the latch of the outermost loop

  // I haven't thought about how to handle a more general case
  assert(LS->getLoopExitBasicBlocks().size() == 1);
  auto ExitBB = LS->getLoopExitBasicBlocks()[0];
  for (auto [BB, E] : LS->getLoopExitEdges()) {
    assert(E == ExitBB);
    BB->getTerminator()->replaceSuccessorWith(ExitBB, OuterLatch);
  }
  auto InnerHeaderSuccInLoop = LS->getSuccessorWithinLoopOfTheHeader();
  for (auto BB : successors(InnerHeader)) {
    if (BB != InnerHeaderSuccInLoop) {
      InnerHeader->getTerminator()->replaceSuccessorWith(BB, OuterLatch);
    }
  }

  // PHI associated to Loop-Governing IV of outer loop
  PHINode *LGOuterPHI = nullptr;

  if (LGInnerPHI) {
    LGOuterPHI = cast<PHINode>(LGInnerPHI->clone());
    OuterHeader->getInstList().push_front(LGOuterPHI);
    // Rewiring new LGInnerPHI with old ones
    for (size_t i = 0; i < LGInnerPHI->getNumIncomingValues(); i++) {
      auto BB = LGInnerPHI->getIncomingBlock(i);
      // If BB is a latch of the inner loop
      if (InnerLatches.find(BB) != InnerLatches.end()) {
        // It should now be replaced with the latch of the outer loop
        // LGOuterPHI->setIncomingBlock(i, BB);
        LGOuterPHI->setIncomingValueForBlock(BB, LGInnerPHI);
        LGOuterPHI->replaceIncomingBlockWith(BB, OuterLatch);
      } else {
        // BB is not a latch. This means that now `LGInnerPHI` must the value
        // from the `OuterPHI`, that now represents `LGInnerPHI`
        LGInnerPHI->setIncomingValueForBlock(BB, LGOuterPHI);
        LGInnerPHI->replaceIncomingBlockWith(BB, OuterHeader);
      }
    }
  } else {
    // If there's no LGOuterPHI, it means that the inner most loop
    // didn't have one. But we need one.
    Builder.SetInsertPoint(OuterHeader);
    auto OuterTy = Type::getInt32Ty(Context);
    auto Zero = ConstantInt::get(OuterTy, 0);
    LGOuterPHI = Builder.CreatePHI(OuterTy, pred_size(InnerHeader) + 1);
    InnerPreheader->getTerminator()->replaceSuccessorWith(InnerHeader,
                                                          OuterHeader);
    // This has a wrong value. It will be patched as soon as we have the
    // `OuterIncrement`
    LGOuterPHI->addIncoming(Zero, OuterLatch);
    LGOuterPHI->addIncoming(Zero, InnerPreheader);
  }

  // Analyzing loop liveouts
  auto ENV = LC->getEnvironment();

  // Get Live-Outs
  set<Instruction *> InnerLiveOuts;
  for (auto id : ENV->getEnvIDsOfLiveOutVars()) {
    auto I = cast<Instruction>(ENV->getProducer(id));
    InnerLiveOuts.insert(I);
  }

  // Duplicate Live-Outs into `OuterHeader`
  assert(InnerLatches.size() == 1); // for simplicity
  auto Latch = *InnerLatches.begin();
  map<Instruction *, Instruction *> OldToNewLiveOuts;
  for (auto LO : InnerLiveOuts) {
    auto NewLO = LO->clone();
    NewLO->insertAfter(LGOuterPHI);
    if (auto NewPHI = dyn_cast<PHINode>(NewLO)) {
      NewPHI->setIncomingValueForBlock(Latch, LO);
      NewPHI->replaceIncomingBlockWith(Latch, OuterLatch);
      auto OldPHI = cast<PHINode>(LO);
      OldPHI->replaceIncomingBlockWith(InnerPreheader, OuterHeader);
      OldPHI->setIncomingValueForBlock(OuterHeader, NewPHI);
    } else {
      // TODO
      assert(false);
    }
    OldToNewLiveOuts[LO] = NewLO;
  }

  // Adjusting (presumaby LCSSA) PHIs in the exit block
  for (auto &I : *ExitBB) {
    if (auto *PHI = dyn_cast<PHINode>(&I)) {
      PHI->replaceIncomingBlockWith(InnerHeader, OuterHeader);
      for (auto [BB, E] : LS->getLoopExitEdges()) {
        assert(E == ExitBB);
        PHI->replaceIncomingBlockWith(BB, OuterHeader);
      }
      // Replace old Live-Outs with new ones
      for (size_t i = 0; i < PHI->getNumIncomingValues(); ++i) {
        auto V = PHI->getIncomingValue(i);
        assert(isa<Instruction>(V));
        auto I = cast<Instruction>(V);

        if (InnerLiveOuts.find(I) != InnerLiveOuts.end()) {
          // I is a Live-Out and need to be replaced
          PHI->setIncomingValue(i, OldToNewLiveOuts[I]);
        } else {
        }
      }
    } else {
      break;
    }
  }

  // IV increment for the outermost loop
  Builder.SetInsertPoint(OuterLatch);
  // errs() << "LGOuterPHI [post]" << *LGOuterPHI << "\n";
  auto OuterTy = LGOuterPHI->getType();
  auto OuterIncrement =
      Builder.CreateAdd(LGOuterPHI, ConstantInt::get(OuterTy, 1));
  Builder.CreateBr(OuterHeader);
  LGOuterPHI->setIncomingValueForBlock(OuterLatch, OuterIncrement);
  LGOuterPHI->setIncomingValueForBlock(InnerPreheader,
                                       ConstantInt::get(OuterTy, 0));

  if (LGIV) {
    auto InnerCmp = LGIV->getHeaderCompareInstructionToComputeExitCondition();

    Builder.SetInsertPoint(OuterHeader);

    // N - InnerOriginalStartIdx
    auto NumIterations =
        Builder.CreateSub(InnerCmp->getOperand(1), InnerOriginalStartIdx);

    // InnerNewStartIdx = i * (N - i_start) / numBlocks + i_start
    auto AdjustedNumBlocks = Builder.CreateZExtOrTrunc(NumBlocks, OuterTy);
    auto InnerNewStartIdx = Builder.CreateAdd(
        Builder.CreateSDiv(Builder.CreateMul(LGOuterPHI, NumIterations),
                           AdjustedNumBlocks),
        InnerOriginalStartIdx);

    // InnerNewEndIdx = (i + 1) * (N - i_start) / numBlocks + i_start
    auto InnerNewEndIdx = Builder.CreateAdd(
        Builder.CreateSDiv(
            Builder.CreateMul(
                Builder.CreateAdd(LGOuterPHI, ConstantInt::get(OuterTy, 1)),
                NumIterations),
            AdjustedNumBlocks),
        InnerOriginalStartIdx);

    // for (...; j < InnerNewEndIdx; ...)
    LGInnerPHI->setIncomingValueForBlock(OuterHeader, InnerNewStartIdx);
    InnerCmp->setOperand(1, InnerNewEndIdx);
  }

  Builder.SetInsertPoint(OuterHeader);
  auto OuterCmp =
      Builder.CreateICmpSLT(LGOuterPHI, NumBlocks);
  Builder.CreateCondBr(OuterCmp, InnerHeader, ExitBB);

  if (NewIVPHI != nullptr) {
    *NewIVPHI = LGOuterPHI;
  }

  // errs() << *F << "\n";

  return OuterHeader;
}

} // namespace arcana::dt
