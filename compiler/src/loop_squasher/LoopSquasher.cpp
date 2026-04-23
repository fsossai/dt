#include <set>

#include "llvm/IR/CFG.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/Lumberjack.hpp"
#include "arcana/dt/LoopSquasher.hpp"

#include <string>
#include <unordered_set>

using namespace std;

using namespace arcana::noelle;

namespace arcana::dt {
void squashLoop(LoopStructure *LS) {
  Logger log(NoelleLumberjack, "LoopSquasher");

  auto ID = LS->getID().value();
  auto s = log.namedSection("Loop(id=" + to_string(ID) + ")");

  // Creating a new unique latch
  auto F = LS->getFunction();
  auto &Context = F->getContext();
  auto Header = LS->getHeader();
  auto NewLatch =
      BasicBlock::Create(Context,
                         "loop.id." + std::to_string(ID) + ".uniquelatch",
                         F);
  IRBuilder<> Builder(NewLatch);

  // Clone IV step instructions into NewLatch
  for (auto PredBB : predecessors(Header)) {
    if (PredBB != LS->getPreHeader()) {
      for (auto &PHI : Header->phis()) {
        log.debug() << "Patching: " << PHI << "\n";
        auto Value = PHI.getIncomingValueForBlock(PredBB);
        if (auto I = dyn_cast<Instruction>(Value)) {
          if (I->getOpcode() == Instruction::Add) {
            log.debug() << "Found Add: " << *I << "\n";
            auto IClone = I->clone();
            Builder.Insert(IClone);
            PHI.setIncomingValueForBlock(PredBB, IClone);
          }
        } else {
          log.bypass() << "ERROR: Not implemented";
          abort();
        }
      }
    }
  }
  Builder.CreateBr(Header);
  Builder.SetInsertPoint(NewLatch);

  // Rewiring latches
  for (auto Latch : LS->getLatches()) {
    // Rewiring PHIs
    for (auto &PHI : Header->phis()) {
      log.debug() << "Rewiring PHI " << PHI << "\n";
      PHI.replaceIncomingBlockWith(Latch, NewLatch);
    }

    auto Terminator = Latch->getTerminator();
    log.debug() << "Rewiring latch: " << *Terminator << "\n";
    Terminator->replaceSuccessorWith(Header, NewLatch);
  }

  // Rewiring exiting edges
  for (auto [BB, E] : LS->getLoopExitEdges()) {
    if (BB != Header) {
      auto Terminator = BB->getTerminator();
      log.debug() << "Rewiring exiting edge: " << *Terminator << "\n";
      Terminator->replaceSuccessorWith(E, NewLatch);
    }
  }
}

} // namespace arcana::dt
