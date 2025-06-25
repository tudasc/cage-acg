#include <Transform/CallgraphGenerator.h>
#include "io/VersionThreeMCGWriter.h"
#include <fstream>
#include <llvm/IR/DebugInfo.h>

#include <llvm/IR/InstVisitor.h>
#include <cxxabi.h>

#include "VirtCall.h"
#include "OmpOutline.h"
#include "ArgDataflow.h"
#include "FunctionResolver.h"

using namespace llvm;

namespace CallgraphGeneration {
    struct CallBaseVisitor : InstVisitor<CallBaseVisitor> {
        explicit CallBaseVisitor(CallGraph *lcg)
            : mcg{metacg::graph::MCGManager::get().getOrCreateCallgraph("LTOGraph", true)},
              lcg{lcg}, omp{mcg, lcg}, resolver{lcg->getModule(), mcg}
        {}

        void visitCallBase(CallBase &I) {
            if (I.getCalledFunction() && I.getCalledFunction()->isIntrinsic())
                return;

            const auto *currentNode = mcg->getNode(I.getParent()->getParent()->getName().str());

            if (!I.getCalledFunction()) {
                for (const auto possible = resolver.potentialFuncs(I); const auto *target : possible) {
                    mcg->addEdge(currentNode, target);

                    const auto *loc = getLoc(&I);
                    const auto srcLoc = SrcLoc{ .line = loc ?  loc->getLine() : 0, .col = loc ? loc->getColumn() : 0, };

                    try {
                        if (auto *edgeMd = dynamic_cast<EdgeCallsMd *>(mcg->getEdgeMetaData(currentNode, target, "calls"));
                            edgeMd && !is_contained(edgeMd->locs, srcLoc))
                            edgeMd->addLoc(srcLoc);
                    } catch (const std::out_of_range &) {
                        auto *md = new EdgeCallsMd{};
                        md->addLoc(srcLoc);
                        mcg->addEdgeMetaData(currentNode, target, md);
                    }
                }
            } else if (I.getCalledFunction()->getName() == "__kmpc_fork_call" && omp.enabled())
                omp.add(I);
        }

        void visitFunction(const Function &F) {
            if (F.isIntrinsic())
                return;

            const std::string &funcName = F.getName().str();
            if (omp.enabled()) {
                if (funcName == "__kmpc_fork_call" && parseOmpOutlineMode() != OmpOutlineMode::SPLIT)
                    return;
                if (StringRef{funcName}.contains("omp_outlined") && parseOmpOutlineMode() == OmpOutlineMode::MERGE)
                    return;
            }

            metacg::CgNode *currentNode;
            if (resolver.functionInfoMap().contains(&F)) {
                const std::string &origin = resolver.functionInfoMap().at(&F)->getFilename().str();
                currentNode = mcg->getOrInsertNode(funcName, origin);
            } else {
                currentNode = mcg->getOrInsertNode(funcName);
            }

            auto argFlowMd = argPathsForFn(mcg, resolver, F);
            for (const auto &[_, md] : argFlowMd)
                currentNode->addMetaData(md);

            for (const auto &[key, elem]: *lcg->operator[](&F)) {
                if (!key.has_value())
                    continue;
                if (!elem->getFunction())
                    continue;
                if (elem->getFunction()->isIntrinsic())
                    continue;
                if (omp.enabled() && elem->getFunction()->getName() == "__kmpc_fork_call")
                    continue;

                const Function *childFunc = elem->getFunction();
                assert(childFunc->hasName());
                metacg::CgNode *childNode;
                if (resolver.functionInfoMap().contains(childFunc)) {
                    const std::string &childFuncOrigin = resolver.functionInfoMap().at(childFunc)->getFilename().str();
                    childNode = mcg->getOrInsertNode(childFunc->getName().str(), childFuncOrigin);
                } else {
                    childNode = mcg->getOrInsertNode(childFunc->getName().str());
                }

                mcg->addEdge(currentNode, childNode);
                if (argFlowMd.contains(childFunc)) {
                    const auto *md = argFlowMd[childFunc];
                    auto *edgeCalls = new EdgeCallsMd{};

                    for (const auto &[_, output] : md->args) {
                        for (const auto &out : output) {
                            if (is_contained(edgeCalls->locs, out.loc))
                                continue;

                            edgeCalls->addLoc(out.loc);
                        }
                    }

                    mcg->addEdgeMetaData(currentNode, childNode, edgeCalls);
                }
            }
        }

    private:
        metacg::Callgraph *mcg;
        CallGraph *lcg;
        OmpOutline omp;
        FunctionResolver resolver;
    };

    bool work(Module &M, ModuleAnalysisManager *MA) {
        auto &cgResult = MA->getResult<CallGraphAnalysis>(M);
        auto cbv = CallBaseVisitor(&cgResult);
        cbv.visit(M);

        metacg::io::JsonSink jsSink;
        metacg::io::VersionThreeMCGWriter writer({{3,       0},
                                                 {"GenCC", 0, 1, "NO_GIT_SHA_AVAILABLE"}}, false, true);
        writer.write(metacg::graph::MCGManager::get().getCallgraph(), jsSink);
        char *gencc_cg_name = std::getenv("GENCC_CG_NAME");
        std::ofstream out(gencc_cg_name ? gencc_cg_name : "LTO_callgraph.mcg");
        out << jsSink.getJson().dump(4);
        out.flush();
        out.close();
        return false;
    }
} // namespace CallGraphGeneration
