#pragma once

#include <string>

#include <arcana/noelle/core/Noelle.hpp>
#include <arcana/noelle/core/DependenceAnalysis.hpp>

namespace arcana::dt {

enum DotOptions_value {
  ONLY_LC_EDGES = 1 << 0,
  HIDE_KNOWN_SCCS = 1 << 1,
  COLLAPSE_EDGES = 1 << 2,
};

using DotOptions = unsigned int;

void dumpToDotFormat(noelle::LoopContent *LC,
                     std::string outputFile,
                     DotOptions options = 0,
                     noelle::DependenceAnalysis *DA = nullptr);

} // namespace arcana::dt
