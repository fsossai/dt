#include <string>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/Lumberjack.hpp"
#include "arcana/noelle/core/PragmaAnalysis.hpp"

#include "LoopStopwatch.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

namespace arcana::dt {

static FunctionCallee getDtSwInit(Module &M) {
  auto &Ctx = M.getContext();
  auto *FTy =
      FunctionType::get(Type::getVoidTy(Ctx),
                        { Type::getInt64Ty(Ctx), Type::getInt8PtrTy(Ctx) },
                        false);
  return M.getOrInsertFunction("__dt_sw_init", FTy);
}

static FunctionCallee getDtSwStart(Module &M) {
  auto &Ctx = M.getContext();
  auto *FTy =
      FunctionType::get(Type::getVoidTy(Ctx), { Type::getInt64Ty(Ctx) }, false);
  return M.getOrInsertFunction("__dt_sw_start", FTy);
}

static FunctionCallee getDtSwStop(Module &M) {
  auto &Ctx = M.getContext();
  auto *FTy =
      FunctionType::get(Type::getVoidTy(Ctx), { Type::getInt64Ty(Ctx) }, false);
  return M.getOrInsertFunction("__dt_sw_stop", FTy);
}

static void addModuleCtor(Module &M, uint64_t tag, const string &name) {
  auto &Ctx = M.getContext();
  auto swInit = getDtSwInit(M);

  auto *FTy = FunctionType::get(Type::getVoidTy(Ctx), false);
  auto *F = Function::Create(FTy,
                             GlobalValue::InternalLinkage,
                             "__dt_sw_ctor_tag_" + to_string(tag),
                             &M);
  auto *BB = BasicBlock::Create(Ctx, "entry", F);
  IRBuilder<> B(BB);

  auto *tagVal = ConstantInt::get(Type::getInt64Ty(Ctx), tag);
  auto *nameStr =
      B.CreateGlobalStringPtr(name, ".dt_sw_name_" + to_string(tag));
  B.CreateCall(swInit, { tagVal, nameStr });
  B.CreateRetVoid();

  appendToGlobalCtors(M, F, 0);
}

static void instrumentLoop(Module &M, LoopStructure *LS, uint64_t tag) {
  Logger log(NoelleLumberjack, "LoopStopwatch");
  auto &Ctx = M.getContext();

  string name = "loop.tag." + to_string(tag);
  log.info() << "Instrumenting loop " << name << "\n";

  addModuleCtor(M, tag, name);

  auto swStart = getDtSwStart(M);
  auto swStop = getDtSwStop(M);
  auto *tagVal = ConstantInt::get(Type::getInt64Ty(Ctx), tag);

  // Insert start at end of preheader
  auto *preheader = LS->getPreHeader();
  {
    IRBuilder<> B(preheader->getTerminator());
    B.CreateCall(swStart, { tagVal });
  }

  // Insert stop on each exit edge by splitting it
  for (auto [exitingBB, exitBB] : LS->getLoopExitEdges()) {
    auto *edgeBB = SplitEdge(exitingBB, exitBB);
    if (edgeBB == nullptr) {
      log.info() << "WARNING: could not split exit edge from "
                 << exitingBB->getName() << " to " << exitBB->getName() << "\n";
      continue;
    }
    IRBuilder<> B(edgeBB->getTerminator());
    B.CreateCall(swStop, { tagVal });
  }
}

void instrumentTaggedLoops(Noelle &noelle, Module &M) {
  Logger log(NoelleLumberjack, "LoopStopwatch");

  for (auto &F : M) {
    if (F.empty())
      continue;

    PragmaForest pf(F, "loop.tag");
    auto &LSs = *noelle.getLoopStructures(&F);
    auto LF = noelle.organizeLoopsInTheirNestingForest(LSs);

    for (auto *LT : LF->getTrees()) {
      LT->visitPreOrder([&](LoopTree *T, auto) {
        auto *LS = T->getLoop();
        auto *pragma = pf.findInnermostPragmaFor(LS);
        if (pragma == nullptr)
          return false;

        auto args = pragma->getArguments();
        assert(args.size() >= 1);
        auto tag = cast<ConstantInt>(args[0])->getZExtValue();
        instrumentLoop(M, LS, tag);
        return true;
      });
    }
  }
}

} // namespace arcana::dt
