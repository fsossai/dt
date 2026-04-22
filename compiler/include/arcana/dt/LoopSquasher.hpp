#pragma once

#include "llvm/IR/BasicBlock.h"

#include <arcana/noelle/core/Noelle.hpp>

namespace arcana::dt {

void squashLoop(noelle::LoopStructure *LS);

} // namespace arcana::dt
