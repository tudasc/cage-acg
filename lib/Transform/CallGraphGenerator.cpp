#include <Transform/CallgraphGenerator.h>
#include "MetaCGMetadata/VTableMetadata.h"
#include "io/VersionThreeMCGWriter.h"
#include <fstream>

using namespace llvm;

namespace CallgraphGeneration {

    void
    llvmCallGraphToMetaCG(CallGraphAnalysis::Result &llvmCG) {
        metacg::graph::MCGManager &mcgManager = metacg::graph::MCGManager::get();
        //Fixme:
        // This ideally should somehow be the name of the resulting executable, but as we run during LTO we do not have this available
        mcgManager.addToManagedGraphs("LTOGraph", std::make_unique<metacg::Callgraph>());
        metacg::Callgraph &callgraph = *mcgManager.getCallgraph();
        for (auto &llvmNode: llvmCG) {
            if (llvmNode.first == NULL) {
                // If the first value is null, this is a reference edge, marking possible entrys into the graph
                continue;
            }
            if (!llvmNode.first->hasName()) {
#ifndef NDEBUG
                outs() << "Found function without name!\n";
#endif
                continue;
            }

            auto &callerFunction = llvmNode.first->getFunction();
            metacg::CgNode *caller = callgraph.getOrInsertNode(llvmNode.first->getName().str());
            caller->setHasBody(callerFunction.getInstructionCount() != 0);
            //ToDo: Attach additional data here

            for (auto calleeFunctionCandidate: *llvmNode.second) {
                if (!calleeFunctionCandidate.first.has_value()) {
#ifndef NDEBUG
                    outs() << "Function:" << callerFunction.getName() << " calls external node\n";
#endif
                    continue;
                }

                assert(isa<CallBase>(calleeFunctionCandidate.first.value()));
                auto callBase = cast<CallBase>(calleeFunctionCandidate.first.value());

                if (auto *calledFunction = callBase->getCalledFunction(); calledFunction) {// This is a direct call, we add an edge
                    assert(calledFunction->hasName());
                    metacg::CgNode *callee = callgraph.getOrInsertNode(calledFunction->getName().str());
                    callee->setHasBody(callerFunction.getInstructionCount() != 0);
                    if (!mcgManager.getCallgraph()->existEdgeFromTo(caller->getId(), callee->getId())) {
                        callgraph.addEdge(caller, callee);
                    }
                } else {// It was impossible to determine the function, it is a call via pointer
                    //As typed pointers have been removed, we currently can not distinguish plain pointer calls, from virtual function calls
                    //We fall back to function signature based call target approximation:
                    for (const auto &func: callerFunction.getParent()->getFunctionList()) {
                        if (func.getFunctionType() ==
                            callBase->getFunctionType()) {//If the signature of the module function matches the one we call, we add a target:
                            assert(func.hasName() && "Found function in Modules function list, that has no name");
                            metacg::CgNode *callee = callgraph.getOrInsertNode(func.getName().str());
                            callee->setHasBody(callerFunction.getInstructionCount() != 0);
                            if (!callgraph.existEdgeFromTo(caller->getId(), callee->getId())) {
                                callgraph.addEdge(caller, callee);
                            }
                        }
                    }
                }
            }
        }
    }


    bool work(Module &M, ModuleAnalysisManager *MA) {
        /** Use callgraph information provided by CGA Pass
         */
        auto &cgResult = MA->getResult<CallGraphAnalysis>(M);
        //This is where we would get our virtual records information from
        //This has been broken, by opaque pointers
        //auto anaRes = MA->getResult<RecordAnalysis::RecordAnalyzer>(M);
        llvmCallGraphToMetaCG(cgResult);

        metacg::io::JsonSink jsSink;
        metacg::io::VersionThreeMCGWriter mcgw(metacg::graph::MCGManager::get(), {{3,       0},
                                                     {"GenCC", 0, 1, "NO_GIT_SHA_AVAILABLE"}});
        mcgw.write(jsSink);
        char* gencc_cg_name=std::getenv("GENCC_CG_NAME");
        std::ofstream out(gencc_cg_name? gencc_cg_name:"LTO_callgraph.mcg");
        out <<jsSink.getJson().dump(4);
        out.flush();
        out.close();

        //mcg.resetManager();
        return false;
    }
} // namespace CallGraphGeneration
