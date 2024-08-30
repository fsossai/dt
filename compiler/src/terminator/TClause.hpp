#ifndef __CLAUSE_HPP__
#define __CLAUSE_HPP__

#include <vector>

#include "llvm/IR/Instructions.h"
#include "arcana/noelle/core/PragmaForest.hpp"

namespace arcana::dt {

class TClause {
public:
  TClause(noelle::PragmaTree &PT);

  llvm::Value *getVariable() const;

  llvm::Value *getDefaultValue() const;

  llvm::Function *getFunction() const;

  std::vector<llvm::Value *> getCallArguments() const;

  const noelle::PragmaTree &getPragmaTree() const;

  void erase();

  int getUniqueID() const;

  std::string getUniqueName() const;

  llvm::raw_ostream &print(llvm::raw_ostream &stream, std::string prefix = "");

  bool isStrong() const;

private:
  noelle::PragmaTree &PT;
  llvm::Value *variable;
  llvm::Value *defaultValue;
  llvm::Function *function;
  std::vector<llvm::Value *> callArguments;
  int uniqueID;
};

} // namespace arcana::dt

#endif
