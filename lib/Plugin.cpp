//
// Created by tim on 15.04.22.
//

#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"

#include "Analysis/RecordAnalyzer.h"
#include "Analysis/DVA.h"

#include "Transform/CallgraphGenerator.h"


/* New PM Registration */
AnalysisKey RecordAnalysis::RecordAnalyzer::Key;
AnalysisKey DevirtAnalysis::DevirtAnalyzer::Key;

llvm::PassPluginLibraryInfo getPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "CaGe", "0.2",
            [](PassBuilder &PB) {
                //allow registration via optlevel (non-lto)
                PB.registerOptimizerLastEPCallback([](ModulePassManager &PM, OptimizationLevel) {
#ifndef NDEBUG
                    outs() << "Registering CaGe to run in during opt\n";
#endif
                    PM.addPass(CallgraphGeneration::CaGe());
                });

                //registering via optlevel during lto appears to still be broken
                PB.registerFullLinkTimeOptimizationLastEPCallback([](ModulePassManager &PM, OptimizationLevel o) {
#ifndef NDEBUG
                    outs() << "Registering CaGe to run in during full-lto\n";
#endif
                    PM.addPass(CallgraphGeneration::CaGe());
                });

                //allow registration via pipeline parser
                PB.registerPipelineParsingCallback(
                        [](StringRef Name, ModulePassManager &MPM, ArrayRef<llvm::PassBuilder::PipelineElement>) {
                            if (Name == "CaGe") {
#ifndef NDEBUG
                                outs() << "Registering CaGe to run as pipeline described\n";
#endif
                                MPM.addPass(CallgraphGeneration::CaGe());
                                return true;
                            } else {
#ifndef NDEBUG
                                outs() << "Did not register CaGe\n";
#endif
                            }
                            return false;
                        });

                // printer pass to allow for "opt -passes=print<record-analysis>"
                PB.registerPipelineParsingCallback(
                        [&](StringRef Name, ModulePassManager &MPM,
                            ArrayRef<PassBuilder::PipelineElement>) {
                            if (Name == "print<record-analysis>") {
                                outs() << "Registering Record Analysis Printer\n";
                                MPM.addPass(RecordAnalysis::RecordAnalyzerPrinter(llvm::errs()));
                                return true;
                            } else {
#ifndef NDEBUG
                                outs() << "Did not register Record Analysis Printer\n";
#endif
                            }
                            return false;
                        });

                // register basic analysis pass for MAM.getResult<RecordAnalyzer>(Module)
                PB.registerAnalysisRegistrationCallback(
                        [](ModuleAnalysisManager &MAM) {
#ifndef NDEBUG
                            outs() << "Registering Basic Record-Analyzer Pass\n";
#endif
                            MAM.registerPass([&] { return RecordAnalysis::RecordAnalyzer(); });
                        });

                // register devirt analysis pass MAM.getResult<DevirtAnalyzer>(Module)
                PB.registerAnalysisRegistrationCallback(
                        [](ModuleAnalysisManager &MAM) {
#ifndef NDEBUG
                            outs()<<"Registering Devirtualization-Analyzer Pass\n";
#endif
                            MAM.registerPass([&] { return DevirtAnalysis::DevirtAnalyzer(); });
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