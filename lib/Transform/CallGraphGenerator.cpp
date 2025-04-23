#include <Transform/CallgraphGenerator.h>
#include "io/VersionThreeMCGWriter.h"
#include <fstream>
#include <llvm/IR/DebugInfo.h>

#include <llvm/IR/InstVisitor.h>
#include <cxxabi.h>

#include "VirtCall.h"

using namespace llvm;

namespace CallgraphGeneration {

    struct CallBaseVisitor : public llvm::InstVisitor<CallBaseVisitor> {

        CallBaseVisitor(llvm::CallGraph *lcg) : lcg(lcg) {
            const Module &m = lcg->getModule();
            llvm::DebugInfoFinder dbg_finder{};
            dbg_finder.processModule(m);
            metaDataAvail = dbg_finder.subprogram_count() != 0;
            for (const auto &i: dbg_finder.subprograms()) {
                if (auto f = m.getFunction(i->getName())) {
                    //We found the function with its normal name (C-Style function)
                    FunctionInfoMap[f] = i;
                } else if (auto f = m.getFunction(i->getLinkageName())) {
                    //We found the function via the linkage name (mangled name / C++ function)
                    FunctionInfoMap[f] = i;
                } else {
                    assert(false);
                }
            }
        }

        void visitCallBase(llvm::CallBase &I) {
            if (I.getCalledFunction() != nullptr && I.getCalledFunction()->isIntrinsic()) return;
            if (!metaDataAvail) return;
            auto vcallData = metavirt::vcall_data_for(&I);
            if (!vcallData.has_value()) return;
            if (vcallData.value().call_targets.empty()) return;

            auto *currentFunction = I.getParent()->getParent();
            auto *currentNode = mcg->getNode(currentFunction->getName().str());
            for (const auto &dataPoints: metavirt::fn_names_and_origins(vcallData.value())) {
                auto *childNode = mcg->getOrInsertNode(dataPoints.name.str(), dataPoints.origin.str());
                mcg->addEdge(currentNode, childNode);
                assert(childNode->getOrigin() == dataPoints.origin);
            }

        }


        void visitFunction(llvm::Function &F) {
            if (F.isIntrinsic()) return;
            const std::string &funcName = F.getName().str();
            metacg::CgNode *currentNode;
            if (metaDataAvail && FunctionInfoMap.find(&F) != FunctionInfoMap.end()) {
                const std::string &origin = FunctionInfoMap[&F]->getFilename().str();
                currentNode = mcg->getOrInsertNode(funcName, origin);
            } else {
                currentNode = mcg->getOrInsertNode(funcName);
            }

            auto *lcgNode = lcg->operator[](&F);
            for (auto [key, elem]: *lcgNode) {
                if (!key.has_value()) continue;
                if (elem->getFunction() == nullptr) continue;
                if (elem->getFunction()->isIntrinsic()) continue;
                const Function *childFunc = elem->getFunction();
                assert(childFunc->hasName());
                metacg::CgNode *childNode;
                if (metaDataAvail && FunctionInfoMap.find(childFunc) != FunctionInfoMap.end()) {
                    const std::string &childFuncOrigin = FunctionInfoMap[childFunc]->getFilename().str();
                    childNode = mcg->getOrInsertNode(childFunc->getName().str(), childFuncOrigin);
                } else {
                    childNode = mcg->getOrInsertNode(childFunc->getName().str());
                }
                mcg->addEdge(currentNode, childNode);
            }
        }

        metacg::Callgraph *mcg = metacg::graph::MCGManager::get().getOrCreateCallgraph("LTOGraph", true);
        llvm::CallGraph *lcg;
        bool metaDataAvail = false;
        std::unordered_map<const Function *, const llvm::DISubprogram *> FunctionInfoMap;

    };

    bool work(Module &M, ModuleAnalysisManager *MA) {
        /** Use callgraph information provided by CGA Pass
         */
        auto &cgResult = MA->getResult<CallGraphAnalysis>(M);
        auto cbv = CallBaseVisitor(&cgResult);
        cbv.visit(M);

        metacg::io::JsonSink jsSink;
        metacg::io::VersionThreeMCGWriter mcgw({{3,       0},
                                                {"GenCC", 0, 1, "NO_GIT_SHA_AVAILABLE"}}, true, true);
        mcgw.write(metacg::graph::MCGManager::get().getCallgraph(), jsSink);
        char *gencc_cg_name = std::getenv("GENCC_CG_NAME");
        std::ofstream out(gencc_cg_name ? gencc_cg_name : "LTO_callgraph.mcg");
        out << jsSink.getJson().dump(4);
        out.flush();
        out.close();
        return false;
    }
} // namespace CallGraphGeneration
