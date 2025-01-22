#include <string>
#include <vector>

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/gino/core/DOALL.hpp"
#include "LeptoInstVisitor.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::gino;
using namespace arcana::noelle;

namespace arcana::dt {

void printUnknownLCDs(noelle::LoopContent *LC, Noelle &noelle) {
  string prefix = "";
  auto sccManager = LC->getSCCManager();
  auto SCCNodes = DOALL::getSCCsThatBlockDOALLToBeApplicable(LC, noelle);
  LeptoInstVisitor LIV;
  using dep_t = pair<Value *, Value *>;
  set<size_t> toSkip;
  vector<dep_t> LCDs_seen;
  for (auto node : SCCNodes) {
    auto SCC = sccManager->getSCCAttrs(node);
    if (auto LCSCC = dyn_cast<LoopCarriedSCC>(SCC)) {
      for (auto dep : LCSCC->getLoopCarriedDependences()) {
        if (isa<ControlDependence<Value, Value>>(dep)) {
          continue;
        }
        auto src = dep->getSrc();
        auto dst = dep->getDst();
        bool depAlreadySeen = false;
        for (auto [otherSrc, otherDst] : LCDs_seen) {
          if (otherSrc == src && otherDst == dst) {
            depAlreadySeen = true;
            break;
          }
        }
        if (!depAlreadySeen) {
          LCDs_seen.push_back({ src, dst });
        }
      }
    }
  }
  for (size_t i = 0; i < LCDs_seen.size(); i++) {
    if (toSkip.find(i) != toSkip.end()) {
      continue;
    }
    auto src = LCDs_seen[i].first;
    auto dst = LCDs_seen[i].second;

    if (src == dst) {
      errs() << prefix << " \u21bb " << LIV.visitValue(*src) << "\n";
    } else {
      bool isSelf = false;
      for (size_t j = 0; j < LCDs_seen.size(); j++) {
        auto otherSrc = LCDs_seen[j].first;
        auto otherDst = LCDs_seen[j].second;
        if (src == otherDst && dst == otherSrc) {
          toSkip.insert(j);
          isSelf = true;
          break;
        }
      }

      if (isSelf) {
        errs() << prefix << "\u250f\u2192 " << LIV.visitValue(*src) << "\n";
      } else {
        errs() << prefix << "\u250f\u2501 " << LIV.visitValue(*src) << "\n";
      }
      errs() << prefix << "\u2517\u2192 " << LIV.visitValue(*dst) << "\n";
    }
  }
}

} // namespace arcana::dt
