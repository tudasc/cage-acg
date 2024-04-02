//
// Created by tim on 15.04.22.
//

#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"

#include "Analysis/RecordAnalyzer.h"
#include "Transform/CallGraphGenerator.h"

/* Legacy PM Registration */
char RecordAnalysis::LegacyRecordAnalyzer::ID;
static RegisterPass<RecordAnalysis::LegacyRecordAnalyzer> recordAnalyzerRegistrar("legacy-record-analysis",
                                                                                  "generates type hierarchy and vtable information (legacy)",
                                                                                  true /* Only looks at CFG */,
                                                                                  true /* Analysis Pass */);

char CallGraphGeneration::LegacyGenCC::ID;
static RegisterPass<CallGraphGeneration::LegacyGenCC> genCCRegistrar("legacy-genCC",
                                                                     "generates Call Graph Components (legacy)",
                                                                     false /* Only looks at CFG */,
                                                                     false /* Analysis Pass */);

/* New PM Registration */
AnalysisKey RecordAnalysis::RecordAnalyzer::Key;

llvm::PassPluginLibraryInfo getPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "genCC", "0.1",
            [](PassBuilder &PB) {
                //allow registration via optlevel (non-lto)
                PB.registerOptimizerLastEPCallback([](ModulePassManager &PM, OptimizationLevel) {
                    PM.addPass(CallGraphGeneration::genCC());
                });

                //registering via optlevel during lto appears to still be broken
                PB.registerFullLinkTimeOptimizationLastEPCallback([](ModulePassManager &PM, OptimizationLevel o) {
                    PM.addPass(CallGraphGeneration::genCC());
                });

                //allow registration via pipeline parser
                PB.registerPipelineParsingCallback(
                        [](StringRef Name, ModulePassManager &PM, ArrayRef<llvm::PassBuilder::PipelineElement>) {
                            if (Name == "genCC") {
                                PM.addPass(CallGraphGeneration::genCC());
                                return true;
                            }
                            return false;
                        });

                // #1 REGISTRATION FOR "opt -passes=print<record-hierarchy>"
                PB.registerPipelineParsingCallback(
                        [&](StringRef Name, ModulePassManager &MPM,
                            ArrayRef<PassBuilder::PipelineElement>) {
                            if (Name == "print<record-analysis>") {
                                MPM.addPass(RecordAnalysis::RecordAnalyzerPrinter(llvm::errs()));
                                return true;
                            }
                            return false;
                        });
                // #2 REGISTRATION FOR "MAM.getResult<RecordAnalyzer>(Module)"
                PB.registerAnalysisRegistrationCallback(
                        [](ModuleAnalysisManager &MAM) {
                            MAM.registerPass([&] { return RecordAnalysis::RecordAnalyzer(); });
                        });
            }};
}


#ifndef LLVM_GENCC_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
#if !NDEBUG
    outs() << "gencc Debug Info  \n";
#else
    outs()<<"gencc Release Info  \n";
#endif
    return getPluginInfo();
}
#endif