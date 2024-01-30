#include <iostream>
#include <string>
#include <set>
#include <queue>

#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "noelle/core/Noelle.hpp"

using namespace llvm;
using namespace arcana::noelle;

namespace {

struct InvokeToCallPass: public FunctionPass {
  static char ID;

  InvokeToCallPass() : FunctionPass(ID) {}

  CallInst *createCallMatchingInvoke(InvokeInst *II) {
    SmallVector<Value *, 8> Args(II->args());
    SmallVector<OperandBundleDef, 1> OpBundles;
    II->getOperandBundlesAsDefs(OpBundles);
    CallInst *NewCall = CallInst::Create(II->getFunctionType(),
                                         II->getCalledOperand(), Args, OpBundles);
    NewCall->setCallingConv(II->getCallingConv());
    NewCall->setAttributes(II->getAttributes());
    NewCall->setDebugLoc(II->getDebugLoc());
    NewCall->copyMetadata(*II);
   
    return NewCall;
  }

  BasicBlock *changeToCall(InvokeInst *II) {
    auto NewCall = createCallMatchingInvoke(II);
    NewCall->takeName(II);
    NewCall->insertBefore(II);
    II->replaceAllUsesWith(NewCall);
   
    // Follow the call by a branch to the normal destination.
    auto NormalDestBB = II->getNormalDest();
    BranchInst::Create(NormalDestBB, II);
   
    auto BB = II->getParent();
    auto UnwindDestBB = II->getUnwindDest();
    UnwindDestBB->removePredecessor(BB);
    II->eraseFromParent();

    return UnwindDestBB;
  }

  bool runOnFunction(Function &F) {
    std::vector<InvokeInst*> IIs;

    for (auto &BB : F) {
      for (auto &I : BB) {
        if (auto II = dyn_cast<InvokeInst>(&I)) {
          IIs.push_back(II);
        }
      }
    }

    for (auto II : IIs) {
      changeToCall(II); 
    }

    return EliminateUnreachableBlocks(F);

  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
    return;
  }
};

} // namespace

// Registering pass

char InvokeToCallPass::ID = 0;
static RegisterPass<InvokeToCallPass> X("invoke-to-call", "Transform every invoke in a call instruction");

static InvokeToCallPass *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker =
      new InvokeToCallPass());
    }
  }
);

static RegisterStandardPasses _RegPass2(
  PassManagerBuilder::EP_EnabledOnOptLevel0,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker = new InvokeToCallPass());
    }
  }
);
