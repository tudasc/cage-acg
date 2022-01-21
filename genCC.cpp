#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

static cl::opt<bool> enableGenCC("genCC", cl::init(false),
                                 cl::desc("generates call-graph component"));

namespace genCC {

    bool work(Module &M) {
        //errs() << "Running with new pass manager \n";
        for (Module::iterator F = M.begin(), Fe = M.end(); F != Fe; ++F) {
            llvm::outs() << F->getName() << "\n";
        }

        return false;
    }

    struct LegacyGenCC : public ModulePass {
        static char ID;

        LegacyGenCC() : ModulePass(ID) {}

        bool runOnModule(Module &M) override { return work(M); }
    };

    struct genCC : PassInfoMixin<genCC> {
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
            if (!work(M))
                return PreservedAnalyses::all();
            return PreservedAnalyses::none();
        }
    };

} // namespace

char genCC::LegacyGenCC::ID = 0;

static RegisterPass<genCC::LegacyGenCC> X("legacy-genCC", "generates Call Grap Components (legacy)",
                                          false /* Only looks at CFG */,
                                          false /* Analysis Pass */);

/* Legacy PM Registration */
static llvm::RegisterStandardPasses RegisterGenCC(
        llvm::PassManagerBuilder::EP_OptimizerLast,
        [](const llvm::PassManagerBuilder &Builder,
           llvm::legacy::PassManagerBase &PM) { PM.add(new genCC::LegacyGenCC()); }
);

/* New PM Registration */
llvm::PassPluginLibraryInfo getGenCCPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "genCC", "0.1",
            [](PassBuilder &PB) {
                //allow registration via optlevel
                //this currently does not work for lto
                PB.registerOptimizerLastEPCallback(
                        [](llvm::ModulePassManager &PM,
                           llvm::PassBuilder::OptimizationLevel Level) {
                            PM.addPass(genCC::genCC());
                        });
                //allow registration via pipeline parser
                PB.registerPipelineParsingCallback(
                        [](StringRef Name, llvm::ModulePassManager &PM,
                           ArrayRef<llvm::PassBuilder::PipelineElement>) {
                            if (Name == "genCC") {
                                PM.addPass(genCC::genCC());
                                return true;
                            }
                            return false;
                        });
            }};
}

#ifndef LLVM_GENCC_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getGenCCPluginInfo();
}
#endif
