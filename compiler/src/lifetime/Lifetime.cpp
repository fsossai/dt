#include <queue>
#include <unordered_set>
#include <vector>

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Metadata.h"

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/Lumberjack.hpp"

#include "arcana/dt/Lifetime.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

namespace arcana::dt {

static Value *stripCastsAndZeroGEPs(Value *V) {
  while (true) {
    if (auto *cast = dyn_cast<CastInst>(V)) {
      V = cast->getOperand(0);
      continue;
    }
    if (auto *gep = dyn_cast<GetElementPtrInst>(V)) {
      bool allZero = true;
      for (auto &idx : gep->indices()) {
        auto *c = dyn_cast<ConstantInt>(idx.get());
        if (!c || !c->isZero()) {
          allZero = false;
          break;
        }
      }
      if (allZero) {
        V = gep->getPointerOperand();
        continue;
      }
    }
    break;
  }
  return V;
}

static bool isCoveredByLifetime(Instruction *use,
                                const vector<Instruction *> &starts,
                                const vector<Instruction *> &ends,
                                DominatorTree &DT,
                                PostDominatorTree &PDT) {
  auto *useBB = use->getParent();
  for (auto *start : starts) {
    auto *startBB = start->getParent();
    if (!DT.dominates(startBB, useBB))
      continue;
    if (startBB == useBB && !start->comesBefore(use))
      continue;

    for (auto *end : ends) {
      auto *endBB = end->getParent();
      if (!PDT.dominates(endBB, useBB))
        continue;
      if (endBB == useBB && !use->comesBefore(end))
        continue;
      return true;
    }
  }
  return false;
}

static bool isPrivatizableInLoop(AllocaInst *A,
                                 LoopStructure *LS,
                                 DominatorTree &DT,
                                 PostDominatorTree &PDT) {
  vector<Instruction *> starts, ends;
  for (auto *inst : LS->getInstructions()) {
    auto *call = dyn_cast<CallInst>(inst);
    if (!call || !call->isLifetimeStartOrEnd())
      continue;
    if (stripCastsAndZeroGEPs(call->getArgOperand(1)) != A)
      continue;
    auto *fn = call->getCalledFunction();
    if (!fn || !fn->isIntrinsic())
      continue;
    if (fn->getIntrinsicID() == Intrinsic::lifetime_start)
      starts.push_back(call);
    else if (fn->getIntrinsicID() == Intrinsic::lifetime_end)
      ends.push_back(call);
  }

  if (starts.empty() || ends.empty())
    return false;

  // BFS through all users (following GEPs and casts as pointer aliases).
  // Every non-alias user must be inside the loop and within a lifetime region.
  queue<Value *> worklist;
  unordered_set<Value *> visited;
  worklist.push(A);
  visited.insert(A);

  while (!worklist.empty()) {
    auto *cur = worklist.front();
    worklist.pop();

    for (auto *user : cur->users()) {
      auto *inst = dyn_cast<Instruction>(user);
      if (!inst)
        return false;

      if (!visited.insert(inst).second)
        continue;

      if (auto *call = dyn_cast<CallInst>(inst)) {
        if (call->isLifetimeStartOrEnd())
          continue;
      }

      if (isa<GetElementPtrInst>(inst) || isa<CastInst>(inst)) {
        worklist.push(inst);
        continue;
      }

      if (!LS->isIncluded(inst))
        return false;
      if (!isCoveredByLifetime(inst, starts, ends, DT, PDT))
        return false;
    }
  }

  return true;
}

void markPrivateAllocas(LoopStructure *LS,
                        DominatorTree &DT,
                        PostDominatorTree &PDT) {
  Logger log(NoelleLumberjack, "Lifetime");

  auto ID = LS->getID().value();
  auto s = log.namedSection("Loop(id=" + to_string(ID) + ")");

  auto *F = LS->getFunction();
  auto &ctx = F->getContext();
  auto *yesNode = MDNode::get(ctx, MDString::get(ctx, "yes"));

  for (auto &I : F->getEntryBlock()) {
    auto *A = dyn_cast<AllocaInst>(&I);
    if (!A)
      continue;

    if (isPrivatizableInLoop(A, LS, DT, PDT)) {
      A->setMetadata("noelle.privatizable", yesNode);
      log.debug() << "Marked as private: " << *A << "\n";
    }
  }
}

} // namespace arcana::dt
