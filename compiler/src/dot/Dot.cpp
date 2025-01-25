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
      return "Loop Carried";
    case GenericSCC::REDUCTION:
      return "Reduction";
    case GenericSCC::BINARY_REDUCTION:
      return "Binary Reduction";
    case GenericSCC::RECOMPUTABLE:
      return "Recomputable";
    case GenericSCC::SINGLE_ACCUMULATOR_RECOMPUTABLE:
      return "Single Accumulator Recomputable";
    case GenericSCC::INDUCTION_VARIABLE:
      return "IV";
    case GenericSCC::LINEAR_INDUCTION_VARIABLE:
      return "Linear IV";
    case GenericSCC::PERIODIC_VARIABLE:
      return "Periodic Variable";
    case GenericSCC::UNKNOWN_CLOSED_FORM:
      return "Unknown Closed Form";
    case GenericSCC::MEMORY_CLONABLE:
      return "Memory Clonable";
    case GenericSCC::STACK_OBJECT_CLONABLE:
      return "Stack Object Clonable";
    case GenericSCC::LOOP_ITERATION:
      return "Loop Iteration";
    case GenericSCC::LOOP_CARRIED_UNKNOWN:
      return "";
    default:
      return to_string(type);
  }
}

string fixEscapes(const string &str) {
  string escaped;
  for (char c : str) {
    if (c == '"') {
      escaped += "\\\""; // Add escaped double quote
    } else if (c == '\\') {
      escaped += "\\\\"; // Add escaped double quote
    } else {
      escaped += c;
    }
  }
  return escaped;
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
      "graph [style=\"filled,rounded\", fillcolor=\"white\", layout=\"fdp\"]\n"
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
  graph["@EDGES@"] = "";
  for (auto SCCNode : SCCDAG->getSCCs()) {
    auto genericSCC = SCCManager->getSCCAttrs(SCCNode);
    map<string, string> subgraph;
    subgraph["@ID@"] = to_string(subgraphId);
    subgraph["@LABEL@"] = SCCKindToString(genericSCC->getKind());
    genericSCC->getSCC()->getEdges();
    // if (auto LCS = dyn_cast<LoopCarriedSCC>(genericSCC)) {
    if (isa<LoopCarriedUnknownSCC>(genericSCC)) {
      // unknown
      subgraph["@COLOR@"] = "red";
    } else {
      // known
      subgraph["@COLOR@"] = "green";
    }
    for (auto I : SCCNode->getInstructions()) {
      map<string, string> node;
      auto srcId = "i" + pointerToString(I);
      node["@ID@"] = srcId;
      node["@LABEL@"] = fixEscapes(LIV.visitValue(*I));
      subgraph["@NODES@"] += patchTemplate(nodeTemplate, node);
      addedNodes.insert(I);
    }
    auto deps = SCCNode->getEdges();
    for (auto dep : deps) {
      if (isa<ControlDependence<Value, Value>>(dep)) {
        continue;
      }

      map<string, string> node;
      map<string, string> edge;
      auto src = dep->getSrc();
      auto dst = dep->getDst();
      auto srcId = "i" + pointerToString(src);
      auto dstId = "i" + pointerToString(dst);

      if (addedNodes.find(src) == addedNodes.end()
          || addedNodes.find(dst) == addedNodes.end()) {
        errs() << "ERROR: unexpected node\n";
      }

      edge["@SRC@"] = srcId;
      edge["@DST@"] = dstId;

      // if (shouldAddNode(src)) {
      //   node["@ID@"] = srcId;
      //   node["@LABEL@"] = LIV.visitValue(*src);
      //   subgraph["@NODES@"] += patchTemplate(nodeTemplate, node);
      //   addedNodes.insert(src);
      // }
      // if (shouldAddNode(dst)) {
      //   node["@ID@"] = dstId;
      //   node["@LABEL@"] = LIV.visitValue(*dst);
      //   subgraph["@NODES@"] += patchTemplate(nodeTemplate, node);
      //   addedNodes.insert(dst);
      // }

      if (true || shouldAddEdge(src, dst)) {
        auto DD = cast<DataDependence<Value, Value>>(dep);
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

        if (isa<MemoryDependence<Value, Value>>(dep)) {
          edge["@STYLE@"] = "solid";
        } else if (isa<VariableDependence<Value, Value>>(dep)) {
          edge["@STYLE@"] = "dashed";
        }

        if (dep->isLoopCarriedDependence()) {
          if (DA && !DA->canThisDependenceBeLoopCarried(dep, *LS)) {
            // terminable
            edge["@COLOR@"] = "orange";
          } else {
            // non-terminable
            edge["@COLOR@"] = "black";
          }
        } else {
          edge["@COLOR@"] = "grey";
        }
        if (subgraph["@NODES@"] != "" && dep->isLoopCarriedDependence()) {
          graph["@EDGES@"] += patchTemplate(edgeTemplate, edge);
          addedEdges.insert({ src, dst });
        }
      }
    }

    graph["@SUBGRAPHS@"] += patchTemplate(subgraphTemplate, subgraph);
    subgraphId++;
  }

  string dotContent = patchTemplate(graphTemplate, graph);
  emitDotFile(dotContent, outputFile);
}

} // namespace arcana::dt
