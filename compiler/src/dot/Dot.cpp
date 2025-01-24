#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>

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

void dumpToDotFormat(LoopContent *LC,
                     string outputFile,
                     DependenceAnalysis *DA) {

  auto LS = LC->getLoopStructure();
  auto sccManager = LC->getSCCManager();
  auto SCCDAG = sccManager->getSCCDAG();

  LeptoInstVisitor LIV;

  string graphTemplate =
      "digraph G {\n"
      "graph [style=\"filled,rounded\", fillcolor=\"white\"]\n"
      "node [color=\"transparent\", fontname=\"Verdana\"]\n"
      "@NODES@\n"
      "@SUBGRAPHS@\n"
      "}\n";
  string subgraphTemplate = "subgraph cluster_scc@ID@ {\n"
                            "\tcolor=\"@COLOR@\"\n"
                            "@EDGES@"
                            "}\n";
  string nodeTemplate = "\t@ID@ [label=\"@LABEL@\"]\n";
  string edgeTemplate =
      "\t@SRC@ -> @DST@ [color=\"@COLOR@\", style=\"@STYLE@\", arrowhead=\"@ARROWHEAD@\"]\n";

  map<string, string> graph;

  unordered_set<Value *> addedNodes;

  auto notAlreadyAdded = [&](Value *V) {
    return addedNodes.find(V) == addedNodes.end();
  };

  int subgraphId = 0;
  for (auto sccNode : SCCDAG->getSCCs()) {
    auto genericSCC = sccManager->getSCCAttrs(sccNode);
    if (auto LCS = dyn_cast<LoopCarriedSCC>(genericSCC)) {
      map<string, string> subgraph;
      subgraph["@ID@"] = to_string(subgraphId);
      subgraph["@EDGES@"] = "";
      if (isa<LoopCarriedUnknownSCC>(LCS)) {
        // unknown
        subgraph["@COLOR@"] = "red";
      } else {
        // known
        subgraph["@COLOR@"] = "green";
      }
      auto LCDs = LCS->getLoopCarriedDependences();
      for (auto LCD : LCDs) {
        if (isa<ControlDependence<Value, Value>>(LCD)) {
          continue;
        }

        assert(LCD->isLoopCarriedDependence());

        map<string, string> edge;
        map<string, string> node;
        auto src = LCD->getSrc();
        auto dst = LCD->getDst();
        auto srcId = "i" + pointerToString(src);
        auto dstId = "i" + pointerToString(dst);

        edge["@SRC@"] = srcId;
        edge["@DST@"] = dstId;

        if (notAlreadyAdded(src)) {
          node["@ID@"] = srcId;
          node["@LABEL@"] = LIV.visitValue(*src);
          graph["@NODES@"] += patchTemplate(nodeTemplate, node);
          addedNodes.insert(src);
        }

        if (notAlreadyAdded(dst)) {
          node["@ID@"] = dstId;
          node["@LABEL@"] = LIV.visitValue(*dst);
          graph["@NODES@"] += patchTemplate(nodeTemplate, node);
          addedNodes.insert(dst);
        }

        auto DD = cast<DataDependence<Value, Value>>(LCD);
        if (DD->isRAWDependence()) {
          edge["@ARROWHEAD@"] = "normal";
        } else if (DD->isWARDependence()) {
          edge["@ARROWHEAD@"] = "inv";
        } else if (DD->isWAWDependence()) {
          edge["@ARROWHEAD@"] = "none";
        }

        if (isa<MemoryDependence<Value, Value>>(LCD)) {
          edge["@STYLE@"] = "solid";
        } else if (isa<VariableDependence<Value, Value>>(LCD)) {
          edge["@STYLE@"] = "dashed";
        }

        edge["@COLOR@"] = "black";
        if (DA && !DA->canThisDependenceBeLoopCarried(LCD, *LS)) {
          // terminable
          edge["@COLOR@"] = "orange";
        }
        subgraph["@EDGES@"] += patchTemplate(edgeTemplate, edge);
      }
      graph["@SUBGRAPHS@"] += patchTemplate(subgraphTemplate, subgraph);
      subgraphId++;
    }
  }

  string dotContent = patchTemplate(graphTemplate, graph);
  emitDotFile(dotContent, outputFile);
}

} // namespace arcana::dt
