#include <iostream>
#include <string>

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "noelle/core/Noelle.hpp"
#include "noelle/core/InductionVariableSCC.hpp"
#include "noelle/core/ReductionSCC.hpp"
#include "noelle/core/LoopIterationSCC.hpp"
#include "noelle/core/LoopCarriedUnknownSCC.hpp"
#include "noelle/core/MemoryClonableSCC.hpp"

using namespace llvm;
using namespace arcana::noelle;

namespace {

struct PrinterPass : public FunctionPass {
  static char ID;

  PrinterPass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) {
    for (auto &I : instructions(F)) {
      if (auto C = dyn_cast<CallInst>(&I)) {
        auto callee = C->getCalledFunction();
        if (callee && callee->getName().startswith("_Z15__dt_ldtc_begin")) {
          errs() << "DepedenceTerminator: Printer: Has clauses: "
                 << F.getName() << "\n";
          return false;
        }
      }
    }
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
    return;
  }
};

} // namespace

// Registering pass

char PrinterPass::ID = 0;
static RegisterPass<PrinterPass> X("dt-printer", "Print functions that contain clauses");

static PrinterPass *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker =
      new PrinterPass());
    }
  }
);

static RegisterStandardPasses _RegPass2(
  PassManagerBuilder::EP_EnabledOnOptLevel0,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker = new PrinterPass());
    }
  }
);
