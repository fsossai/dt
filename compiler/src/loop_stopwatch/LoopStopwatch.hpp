#pragma once

#include "llvm/IR/Module.h"
#include "arcana/noelle/core/Noelle.hpp"

using namespace llvm;
using namespace arcana::noelle;

namespace arcana::dt {

void instrumentTaggedLoops(Noelle &noelle, Module &M);

} // namespace arcana::dt
