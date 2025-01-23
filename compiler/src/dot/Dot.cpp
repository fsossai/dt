#include <string>

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/SCCDAGAttrs.hpp"
#include "arcana/noelle/core/LoopCarriedUnknownSCC.hpp"

#include "arcana/dt/Dot.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

void dumpToDotFormat(LoopContent *LC,
                     string outputFile,
                     DependenceAnalysis *DA) {

  auto LS = LC->getLoopStructure();
  auto sccManager = LC->getSCCManager();
  auto SCCDAG = sccManager->getSCCDAG();

  enum DepType { KNOWN, UNKNOWN, TERMINABLE };

  for (auto sccNode : SCCDAG->getSCCs()) {
    auto genericSCC = sccManager->getSCCAttrs(sccNode);
    if (auto LCU = dyn_cast<LoopCarriedUnknownSCC>(genericSCC)) {
      auto LCDs = LCU->getLoopCarriedDependences();

      for (auto LCD : LCDs) {
        if (isa<ControlDependence<Value, Value>>(LCD)) {
          continue;
        }

        auto src = LCD->getSrc();
        auto dst = LCD->getDst();

        if (DA != nullptr) {
          if (!DA->canThisDependenceBeLoopCarried(LCD, *LS)) {
            // terminable
          }
        }
        // unknown
      }
    } else {

      // known
    }
  }
}
