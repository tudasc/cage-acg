#include <llvm/Analysis/CallGraph.h>
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <llvm/Transforms/IPO/WholeProgramDevirt.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <clang/Analysis/CallGraph.h>

#include "Callgraph.h"
#include "MCGManager.h"
#include "Analysis/RecordAnalyzer.h"
#include "MCGWriter.h"


using namespace llvm;

//static cl::opt<bool> enableGenCC("genCC", cl::init(false),
//                                 cl::desc("generates call-graph component"));

namespace GenCC {

    void generateLibraryFunction(Module &M);

    void generateInitFunction(Module &M);

    void passToRuntimeComponent(Module &M, Value *Arg);

    metacg::graph::MCGManager& llvmCallGraphToMetaCG(CallGraphAnalysis::Result &llvmCG, RecordAnalysis::RecordMap rm);

    bool work(Module &M, ModuleAnalysisManager *MA);

    struct genCC : PassInfoMixin<genCC> {
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MA) {
            if (!work(M, &MA))
                return PreservedAnalyses::all();
            return PreservedAnalyses::none();
        }
    };

    struct LegacyGenCC : public ModulePass {
        static char ID;

        LegacyGenCC() : ModulePass(ID) {}

        bool runOnModule(Module &M) override { return work(M, nullptr); }
    };

} // namespace


