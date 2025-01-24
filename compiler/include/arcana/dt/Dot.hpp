#pragma once

#include <string>

#include <arcana/noelle/core/Noelle.hpp>
#include <arcana/noelle/core/DependenceAnalysis.hpp>

namespace arcana::dt {

void dumpToDotFormat(noelle::LoopContent *LC,
                     std::string outputFile,
                     noelle::DependenceAnalysis *DA = nullptr);

} // namespace arcana::dt
