
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"

#include "arcana/noelle/core/PragmaForest.hpp"
#include "TClause.hpp"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

static int clauseIncrementalID = 0;

namespace arcana::dt {

TClause::TClause(PragmaTree &PT) : PT(PT) {
  auto pragmaArgs = PT.getArguments();

  if (pragmaArgs.size() == 0) {
    // We call this "strong" clause
    this->function = nullptr;
  } else if (pragmaArgs.size() >= 3) {
    this->variable = pragmaArgs[0];
    assert(this->variable->getType()->isPointerTy());
    this->defaultValue = pragmaArgs[1];

    auto dstType = this->variable->getType()->getPointerElementType();
    assert(this->defaultValue->getType() == dstType);

    assert(isa<Function>(pragmaArgs[2]));
    this->function = cast<Function>(pragmaArgs[2]);
    assert(this->function->getReturnType() == dstType);

    // Remaining arguments are arguments of the clause function
    assert(this->function->arg_size() == (pragmaArgs.size() - 3));
    auto arg_it = this->function->arg_begin();
    for (size_t i = 3; i < pragmaArgs.size(); i++) {
      // The type of the arguments provided to the clause must be compatible
      // with the signature of the clause function
      assert(arg_it->getType() == pragmaArgs[i]->getType());
      this->callArguments.push_back(pragmaArgs[i]);
      ++arg_it;
    }
  } else {
    assert(false && "invalid number of pragma arguments for a clause");
  }

  this->uniqueID = clauseIncrementalID++;
}

Value *TClause::getVariable() const {
  return this->variable;
}

Value *TClause::getDefaultValue() const {
  return this->defaultValue;
}

Function *TClause::getFunction() const {
  return this->function;
}

vector<Value *> TClause::getCallArguments() const {
  return this->callArguments;
}

int TClause::getUniqueID() const {
  return this->uniqueID;
}

string TClause::getUniqueName() const {
  return "C" + to_string(this->uniqueID);
}

const PragmaTree &TClause::getPragmaTree() const {
  return this->PT;
}

void TClause::erase() {
  this->PT.getBeginDelimiter()->eraseFromParent();
  this->PT.getEndDelimiter()->eraseFromParent();
}

bool TClause::isStrong() const {
  return this->function == nullptr;
}

raw_ostream &TClause::print(raw_ostream &stream, string prefix) {
  if (this->isStrong()) {
    stream << prefix << "<nofunc> (" << this->getUniqueName() << ")";
  } else {
    stream << prefix << this->function->getName().str() << " ("
           << this->getUniqueName() << ")";
  }
  return stream;
}

} // namespace arcana::dt
