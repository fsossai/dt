#include <set>

#include "llvm/IR/Instructions.h"

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/dt/LoopBlocker.hpp"

using namespace std;

using namespace arcana::noelle;

namespace arcana::dt {

BasicBlock *blockLoop(LoopContent *LC, int numBlocks, PHINode **NewIVPHI) {
  auto LS = LC->getLoopStructure();
  auto IVM = LC->getInductionVariableManager();
  auto LGIV = IVM->getLoopGoverningInductionVariable(*LC->getLoopStructure());
  auto LGInnerPHI =
      LGIV ? LGIV->getInductionVariable()->getLoopEntryPHI() : nullptr;
  auto InnerLatches = LS->getLatches();

  auto F = LS->getFunction();
  auto &Context = F->getContext();
  IRBuilder<> Builder(Context);

  assert(LS->numberOfExitBasicBlocks() == 1);

  auto IVs = IVM->getInductionVariables();

  auto ExitBB = LS->getLoopExitBasicBlocks()[0];
  auto InnerHeader = LS->getHeader();
  auto InnerPreheader = LS->getPreHeader();


  auto OuterHeader = BasicBlock::Create(Context, "", F);

  Value *InnerOriginalStartIdx = nullptr;

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
  InnerHeader->getTerminator()->replaceSuccessorWith(ExitBB, OuterLatch);

  // PHI associated to Loop-Governing IV of outer loop
  PHINode *LGOuterPHI = nullptr;

  Builder.SetInsertPoint(OuterHeader);
  for (auto &I : *InnerHeader) {
    if (auto *InnerPHI = dyn_cast<PHINode>(&I)) {
      // Duplicate the InnerPHI in the OuterHeader
      auto OuterPHI = Builder.CreatePHI(InnerPHI->getType(),
                                        InnerPHI->getNumIncomingValues());
      for (size_t i = 0; i < InnerPHI->getNumIncomingValues(); i++) {
        OuterPHI->addIncoming(InnerPHI->getIncomingValue(i),
                              InnerPHI->getIncomingBlock(i));
      }

      // Rewiring new InnerPHI with old ones
      for (size_t i = 0; i < InnerPHI->getNumIncomingValues(); i++) {
        auto BB = InnerPHI->getIncomingBlock(i);
        // If BB is a latch of the inner loop
        if (InnerLatches.find(BB) != InnerLatches.end()) {
          // It should now be replaced with the latch of the outer loop
          OuterPHI->setIncomingValueForBlock(BB, InnerPHI);
          OuterPHI->replaceIncomingBlockWith(BB, OuterLatch);
        } else {
          // BB is not a latch. This means that now `InnerPHI` must the value
          // from the `OuterPHI`, that now represents `InnerPHI`
          InnerPHI->setIncomingValueForBlock(BB, OuterPHI);
          InnerPHI->replaceIncomingBlockWith(BB, OuterHeader);
        }
      }
      if (InnerPHI == LGInnerPHI) {
        LGOuterPHI = OuterPHI;
      }

      // The original induction variable might me used outside the loop.
      // We need to track its last value
      // TODO is this really necessary?
      auto OuterLastValuePHI = OuterPHI->clone();
      OuterLastValuePHI->insertAfter(OuterPHI);

      // Thanks to LCSSA we only need to patch the exit BB
      for (auto &I : *ExitBB) {
        if (auto *ExitPHI = dyn_cast<PHINode>(&I)) {
          if (ExitPHI->getIncomingValueForBlock(InnerHeader) == InnerPHI) {
            ExitPHI->setIncomingValueForBlock(InnerHeader, OuterLastValuePHI);
          }
        } else {
          break;
        }
      }
    }
  }

  // If there's no LGOuterPHI at this point, it means that the inner most loop
  // didn't have one. But we need one.
  if (!LGOuterPHI) {
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

  // Adjusting (presumaby LCSSA) PHIs in the exit block
  for (auto &I : *ExitBB) {
    if (auto *PHI = dyn_cast<PHINode>(&I)) {
      PHI->replaceIncomingBlockWith(InnerHeader, OuterHeader);
    } else {
      break;
    }
  }

  // IV increment for the outermost loop
  Builder.SetInsertPoint(OuterLatch);
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
    auto InnerNewStartIdx = Builder.CreateAdd(
        Builder.CreateSDiv(Builder.CreateMul(LGOuterPHI, NumIterations),
                           ConstantInt::get(OuterTy, numBlocks)),
        InnerOriginalStartIdx);

    // InnerNewEndIdx = (i + 1) * (N - i_start) / numBlocks + i_start
    auto InnerNewEndIdx = Builder.CreateAdd(
        Builder.CreateSDiv(
            Builder.CreateMul(
                Builder.CreateAdd(LGOuterPHI, ConstantInt::get(OuterTy, 1)),
                NumIterations),
            ConstantInt::get(OuterTy, numBlocks)),
        InnerOriginalStartIdx);

    // for (...; j < InnerNewEndIdx; ...)
    LGInnerPHI->setIncomingValueForBlock(OuterHeader, InnerNewStartIdx);
    InnerCmp->setOperand(1, InnerNewEndIdx);
  }

  Builder.SetInsertPoint(OuterHeader);
  auto OuterCmp =
      Builder.CreateICmpSLT(LGOuterPHI, ConstantInt::get(OuterTy, numBlocks));
  Builder.CreateCondBr(OuterCmp, InnerHeader, ExitBB);

  if (NewIVPHI != nullptr) {
    *NewIVPHI = LGOuterPHI;
  }

  // errs() << *F << "\n";

  return OuterHeader;
}

} // namespace arcana::dt
