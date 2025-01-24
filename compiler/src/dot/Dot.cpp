#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>

#include "arcana/noelle/core/DataDependence.hpp"
#include "arcana/noelle/core/LoopCarriedSCC.hpp"
#include "arcana/noelle/core/LoopCarriedUnknownSCC.hpp"
#include "arcana/noelle/core/MemoryDependence.hpp"
#include "arcana/noelle/core/Noelle.hpp"
#include "arcana/noelle/core/SCCDAGAttrs.hpp"
#include "LeptoInstVisitor.hpp"

#include "arcana/dt/Dot.hpp"
#include "llvm/IR/Value.h"

using namespace std;
using namespace llvm;
using namespace arcana::noelle;

namespace arcana::dt {

string patchTemplate(const string &templateStr,
                     const map<string, string> &patches) {
  string result = templateStr;
  for (const auto &[oldStr, newStr] : patches) {
    size_t pos = 0;
    while ((pos = result.find(oldStr, pos)) != string::npos) {
      result.replace(pos, oldStr.length(), newStr);
      pos += newStr.length();
    }
  }
  return result;
}

void emitDotFile(const string &dotContent, string outputFile) {
  ofstream outputStream(outputFile);
  assert(outputStream.is_open());
  outputStream << dotContent;
  outputStream.close();
}

string pointerToString(const Value *V) {
  ostringstream oss;
  oss << std::setw(8) << std::setfill('0') << std::hex
      << reinterpret_cast<uintptr_t>(V);
  return oss.str();
}

string SCCKindToString(GenericSCC::SCCKind type) {
  switch (type) {
    case GenericSCC::LOOP_CARRIED:
      return "LOOP_CARRIED";
    case GenericSCC::REDUCTION:
      return "REDUCTION";
    case GenericSCC::BINARY_REDUCTION:
      return "BINARY_REDUCTION";
    case GenericSCC::RECOMPUTABLE:
      return "RECOMPUTABLE";
    case GenericSCC::SINGLE_ACCUMULATOR_RECOMPUTABLE:
      return "SINGLE_ACCUMULATOR_RECOMPUTABLE";
    case GenericSCC::INDUCTION_VARIABLE:
      return "INDUCTION_VARIABLE";
    case GenericSCC::LINEAR_INDUCTION_VARIABLE:
      return "LINEAR_INDUCTION_VARIABLE";
    case GenericSCC::PERIODIC_VARIABLE:
      return "PERIODIC_VARIABLE";
    case GenericSCC::UNKNOWN_CLOSED_FORM:
      return "UNKNOWN_CLOSED_FORM";
    case GenericSCC::MEMORY_CLONABLE:
      return "MEMORY_CLONABLE";
    case GenericSCC::STACK_OBJECT_CLONABLE:
      return "STACK_OBJECT_CLONABLE";
    case GenericSCC::LOOP_CARRIED_UNKNOWN:
      return "LOOP_CARRIED_UNKNOWN";
    case GenericSCC::LOOP_ITERATION:
      return "LOOP_ITERATION";
    default:
      assert(false);
  }
}

void dumpToDotFormat(LoopContent *LC,
                     string outputFile,
                     bool collapseEdges,
                     DependenceAnalysis *DA) {

  auto LS = LC->getLoopStructure();
  auto SCCManager = LC->getSCCManager();
  auto SCCDAG = SCCManager->getSCCDAG();

  LeptoInstVisitor LIV;

  string graphTemplate =
      "digraph G {\n"
      "graph [style=\"filled,rounded\", fillcolor=\"white\"]\n"
      "node [color=\"transparent\", fontname=\"Verdana\"]\n"
      "@SUBGRAPHS@\n"
      "@EDGES@\n"
      "}\n";
  string subgraphTemplate = "subgraph cluster_scc@ID@ {\n"
                            "\tlabel=\"@LABEL@\"\n"
                            "\tcolor=\"@COLOR@\"\n"
                            "@NODES@"
                            "}\n";
  string nodeTemplate = "\t@ID@ [label=\"@LABEL@\"]\n";
  string edgeTemplate =
      "\t@SRC@ -> @DST@ [color=\"@COLOR@\", style=\"@STYLE@\", arrowhead=\"@ARROWHEAD@\"]\n";

  map<string, string> graph;
  set<pair<Value *, Value *>> addedEdges;
  unordered_set<Value *> addedNodes;

  auto shouldAddNode = [&](Value *V) {
    return addedNodes.find(V) == addedNodes.end();
  };

  auto shouldAddEdge = [&](Value *src, Value *dst) {
    if (collapseEdges) {
      return addedEdges.find({ src, dst }) == addedEdges.end()
             && addedEdges.find({ dst, src }) == addedEdges.end();
    }
    return true;
  };

  int subgraphId = 0;
  for (auto SCCNode : SCCDAG->getSCCs()) {
    auto genericSCC = SCCManager->getSCCAttrs(SCCNode);
    if (auto LCS = dyn_cast<LoopCarriedSCC>(genericSCC)) {
      map<string, string> subgraph;
      subgraph["@ID@"] = to_string(subgraphId);
      if (isa<LoopCarriedUnknownSCC>(LCS)) {
        // unknown
        subgraph["@COLOR@"] = "red";
      } else {
        // known
        subgraph["@COLOR@"] = "green";
      }
      subgraph["@LABEL@"] = SCCKindToString(genericSCC->getKind());
      auto LCDs = LCS->getLoopCarriedDependences();
      for (auto LCD : LCDs) {
        if (isa<ControlDependence<Value, Value>>(LCD)) {
          continue;
        }

        assert(LCD->isLoopCarriedDependence());

        map<string, string> node;
        map<string, string> edge;
        auto src = LCD->getSrc();
        auto dst = LCD->getDst();
        auto srcId = "i" + pointerToString(src);
        auto dstId = "i" + pointerToString(dst);

        edge["@SRC@"] = srcId;
        edge["@DST@"] = dstId;

        if (shouldAddNode(src)) {
          node["@ID@"] = srcId;
          node["@LABEL@"] = LIV.visitValue(*src);
          subgraph["@NODES@"] += patchTemplate(nodeTemplate, node);
          addedNodes.insert(src);
        }

        if (shouldAddNode(dst)) {
          node["@ID@"] = dstId;
          node["@LABEL@"] = LIV.visitValue(*dst);
          subgraph["@NODES@"] += patchTemplate(nodeTemplate, node);
          addedNodes.insert(dst);
        }

        if (shouldAddEdge(src, dst)) {
          auto DD = cast<DataDependence<Value, Value>>(LCD);
          if (collapseEdges) {
            edge["@ARROWHEAD@"] = "none";
          } else {
            if (DD->isRAWDependence()) {
              edge["@ARROWHEAD@"] = "normal";
            } else if (DD->isWARDependence()) {
              edge["@ARROWHEAD@"] = "inv";
            } else if (DD->isWAWDependence()) {
              edge["@ARROWHEAD@"] = "none";
            }
          }

          if (isa<MemoryDependence<Value, Value>>(LCD)) {
            edge["@STYLE@"] = "solid";
          } else if (isa<VariableDependence<Value, Value>>(LCD)) {
            edge["@STYLE@"] = "dashed";
          }

          if (DA && !DA->canThisDependenceBeLoopCarried(LCD, *LS)) {
            // terminable
            edge["@COLOR@"] = "orange";
          } else {
            // non-terminable
            edge["@COLOR@"] = "black";
          }
          if (subgraph["@NODES@"] != "") {
            graph["@EDGES@"] += patchTemplate(edgeTemplate, edge);
          }
          addedEdges.insert({ src, dst });
        }
      }
      graph["@SUBGRAPHS@"] += patchTemplate(subgraphTemplate, subgraph);
      subgraphId++;
    }
  }

  string dotContent = patchTemplate(graphTemplate, graph);
  emitDotFile(dotContent, outputFile);
}

} // namespace arcana::dt
