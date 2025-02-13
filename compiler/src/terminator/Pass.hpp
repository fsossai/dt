#pragma once

#include <string>
#include <vector>

#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/Lumberjack.hpp"
#include "LeptoInstVisitor.hpp"

namespace arcana::dt {

class TerminatorPass : public llvm::ModulePass {
public:
  static char ID;

  TerminatorPass();
  bool doInitialization(llvm::Module &M) override;
  bool runOnModule(llvm::Module &M) override;
  bool runOnFunction(noelle::Noelle &noelle,
                     noelle::LoopForest &LF,
                     llvm::Function &F,
                     int &lastLoopOrder);
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  std::string getLoopDescription(noelle::LoopStructure *LS);
  uint64_t getLoopTag(noelle::LoopStructure *LS);

private:
  void populateLoopTags(noelle::Noelle &noelle, llvm::Function &F,
                        const std::vector<noelle::LoopStructure *> &LSs);

  noelle::Logger log;
  LeptoInstVisitor lepto;
  noelle::MetadataManager *MM;
  std::unordered_map<uint64_t, uint64_t> loopIdToTag;
  std::unordered_map<uint64_t, uint64_t> loopTagToId;
};

} // namespace arcana::dt
