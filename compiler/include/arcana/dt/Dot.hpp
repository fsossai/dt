#pragma once

#include <string>

#include <arcana/noelle/core/Noelle.hpp>
#include <arcana/noelle/core/DependenceAnalysis.hpp>

#define DOT_TEMPLATE_FILE                                                      \
  "/nfs-scratch/fsv1684/repo/dt/compiler/src/dot/template.dot"

namespace arcana::dt {

void dumpToDotFormat(noelle::LoopContent *LC,
                     std::string outputFile,
                     noelle::DependenceAnalysis *DA = nullptr);

} // namespace arcana::dt
