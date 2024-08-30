#pragma once

#include "llvm/IR/BasicBlock.h"

#include <arcana/noelle/core/Noelle.hpp>

namespace arcana::dt {

llvm::BasicBlock *blockLoop(noelle::LoopContent *LC, int numBlocks);

} // namespace arcana::gino
