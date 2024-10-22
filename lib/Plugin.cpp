//
// Created by tim on 15.04.22.
//

#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"

#include "Analysis/RecordAnalyzer.h"
#include "Analysis/DVA.h"

#include "Transform/CallgraphGenerator.h"

llvm::PassPluginLibraryInfo getPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "CaGe", "0.2",
            [](PassBuilder &PB) {
                //allow registration via optlevel (non-lto)
                PB.registerOptimizerLastEPCallback([](ModulePassManager &PM, OptimizationLevel) {
                    outs() << "Registering CaGe to run in during opt\n";
                    PM.addPass(CallgraphGeneration::CaGe());
                });

                //registering via optlevel during lto appears to still be broken
                PB.registerFullLinkTimeOptimizationLastEPCallback([](ModulePassManager &PM, OptimizationLevel o) {
                    outs() << "Registering CaGe to run in during full-lto\n";
                    PM.addPass(CallgraphGeneration::CaGe());
                });

                //allow registration via pipeline parser
                PB.registerPipelineParsingCallback(
                        [](StringRef Name, ModulePassManager &MPM, ArrayRef<llvm::PassBuilder::PipelineElement>) {
                            if (Name == "CaGe") {
                                outs() << "Registering CaGe to run as pipeline described\n";
                                MPM.addPass(CallgraphGeneration::CaGe());
                                return true;
                            } else {
                                outs() << "Did not register CaGe\n";
                            }
                            return false;
                        });
            }};
}


#ifndef LLVM_GENCC_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
#ifndef NDEBUG
    outs() << "Loading Debugversion of CaGe-Plugin\n";
#else
    outs()<<"Loading Releaseversion of CaGe-Plugin\n";
#endif
    return getPluginInfo();
}
#endif