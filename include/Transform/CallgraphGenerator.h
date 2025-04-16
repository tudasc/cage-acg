#include <llvm/Analysis/CallGraph.h>
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Transforms/IPO/WholeProgramDevirt.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <clang/Analysis/CallGraph.h>

#include "Callgraph.h"
#include "MCGManager.h"
#include "io/MCGWriter.h"


using namespace llvm;


namespace CallgraphGeneration {

    bool work(Module &M, ModuleAnalysisManager *MA);

    struct CaGe : PassInfoMixin<CaGe> {
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MA) {
            if (!work(M, &MA))
                return PreservedAnalyses::all();
            return PreservedAnalyses::none();

        }

        static StringRef name() {
            return "CaGe";
        }

    };
} // namespace


