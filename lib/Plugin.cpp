//
// Created by tim on 15.04.22.
//

#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "Analysis/RecordAnalyzer.h"
#include "Transform/GenCC.h"


static RegisterPass<RecordAnalysis::LegacyRecordAnalyzer> recordAnalyzerRegistrar("legacy-record-analysis", "generates type hierarchy and vtable information (legacy)",
                                                       true /* Only looks at CFG */,
                                                       true /* Analysis Pass */);

static RegisterPass<GenCC::LegacyGenCC> genCCRegistrar("legacy-genCC", "generates Call Graph Components (legacy)",
                                                       false /* Only looks at CFG */,
                                                       false /* Analysis Pass */);


/* Legacy PM Registration */
static llvm::RegisterStandardPasses RegisterGenCC(
        llvm::PassManagerBuilder::EP_OptimizerLast,
        [](const llvm::PassManagerBuilder &Builder,
           llvm::legacy::PassManagerBase &PM) { PM.add(new GenCC::LegacyGenCC()); }
);

/* New PM Registration */
//todo make registration separate, maybe use tblgen ?
llvm::PassPluginLibraryInfo getPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "genCC", "0.1",
            [](PassBuilder &PB) {
                //allow registration via optlevel
                //this currently does not work for lto
                PB.registerOptimizerLastEPCallback(
                        [](llvm::ModulePassManager &PM,
                           llvm::PassBuilder::OptimizationLevel Level) {
                            PM.addPass(GenCC::genCC());
                        });
                //allow registration via pipeline parser
                PB.registerPipelineParsingCallback(
                        [](StringRef Name, llvm::ModulePassManager &PM,
                           ArrayRef<llvm::PassBuilder::PipelineElement>) {
                            if (Name == "genCC") {
                                PM.addPass(GenCC::genCC());
                                return true;
                            }
                            return false;
                        });
                // #1 REGISTRATION FOR "opt -passes=print<type-hierarchy>"
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


char ID = 0;
#ifndef LLVM_GENCC_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    outs()<<"Got called!!\n\n\n";
    return getPluginInfo();
}
#endif