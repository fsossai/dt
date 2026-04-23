#pragma once

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"

#include "arcana/noelle/core/Noelle.hpp"

namespace arcana::dt {

void markPrivateAllocas(noelle::LoopStructure *LS,
                        llvm::DominatorTree &DT,
                        llvm::PostDominatorTree &PDT);

} // namespace arcana::dt
