#include <Transform/CallgraphGenerator.h>
#include "io/VersionThreeMCGWriter.h"
#include <fstream>
#include <llvm/IR/DebugInfo.h>

#include <llvm/IR/InstVisitor.h>

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
            if (!metaDataAvail) return;
            auto vcallData = metavirt::vcall_data_for(&I);
            if (!vcallData.has_value()) {
                outs() << "No additional information for callbase:\n";
                I.dump();
            } else {
                outs() << "Additional Information available:\n";
                outs() << "The CallBase can call:\n";
                outs() << "Any of " << vcallData.value().call_targets.size() << "functions\n";
                for (const auto &dataPoints: metavirt::fn_names_and_origins(vcallData.value())) {
                    outs() << dataPoints.name << " " << dataPoints.origin << "\n";
                }
            }
        }


        void visitFunction(llvm::Function &F) {
            if (F.isIntrinsic()) return;
            const std::string &funcName = F.getName().str();
            if (metaDataAvail) {
                assert(FunctionInfoMap.find(&F) != FunctionInfoMap.end());
                const std::string &origin = FunctionInfoMap[&F]->getFilename().str();
                const auto &currentNode = mcg->getOrInsertNode(funcName, origin);
                for (int i = 0; i < lcg->operator[](&F)->size(); i++) {
                    const Function *childFunc = lcg->operator[](&F)[i].getFunction();
                    assert(childFunc->hasName());
                    const std::string &childFuncName = childFunc->getName().str();
                    assert(FunctionInfoMap.find(childFunc) != FunctionInfoMap.end());
                    const std::string &childFuncOrigin = FunctionInfoMap[childFunc]->getFilename().str();
                    const metacg::CgNode *mcgChildNode = mcg->getOrInsertNode(childFuncName, childFuncOrigin);
                    mcg->addEdge(currentNode, mcgChildNode);
                }
            } else {
                const auto &currentNode = mcg->getOrInsertNode(funcName);
                for (int i = 0; i < lcg->operator[](&F)->size(); i++) {
                    const Function *childFunc = lcg->operator[](&F)[i].getFunction();
                    assert(childFunc->hasName());
                    const std::string &childFuncName = childFunc->getName().str();
                    const metacg::CgNode *mcgChildNode = mcg->getOrInsertNode(childFuncName);
                    mcg->addEdge(currentNode, mcgChildNode);
                }
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
                                                {"GenCC", 0, 1, "NO_GIT_SHA_AVAILABLE"}});
        mcgw.write(metacg::graph::MCGManager::get().getCallgraph(), jsSink);
        char *gencc_cg_name = std::getenv("GENCC_CG_NAME");
        std::ofstream out(gencc_cg_name ? gencc_cg_name : "LTO_callgraph.mcg");
        out << jsSink.getJson().dump(4);
        out.flush();
        out.close();
        //mcg.resetManager();
        return false;
    }
} // namespace CallGraphGeneration
