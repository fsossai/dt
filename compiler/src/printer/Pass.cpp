#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace arcana::terminator {

struct Printer : public FunctionPass {
  static char ID;

  Printer() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) {
    for (auto &I : instructions(F)) {
      if (auto C = dyn_cast<CallInst>(&I)) {
        auto callee = C->getCalledFunction();
        if (callee && callee->getName().startswith("_Z15__dt_ldtc_begin")) {
          errs() << "Terminator: Printer: Has clauses: "
                 << F.getName() << "\n";
          return false;
        }
      }
    }
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    return;
  }
};

} // namespace arcana::terminator

// Registering pass

using namespace arcana::terminator;

char Printer::ID = 0;
static RegisterPass<Printer> X("dt-printer", "Print functions that contain clauses");

static Printer *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker =
      new Printer());
    }
  }
);

static RegisterStandardPasses _RegPass2(
  PassManagerBuilder::EP_EnabledOnOptLevel0,
  [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
      PM.add(_PassMaker = new Printer());
    }
  }
);
