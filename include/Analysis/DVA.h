//
// Created by tim on 31.03.22.
//

#ifndef CALLGRAPHGENERATION_DEVIRTANALYZER_H
#define CALLGRAPHGENERATION_DEVIRTANALYZER_H

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <llvm/Demangle/Demangle.h>
#include <llvm/Support/Debug.h>
#include <unordered_set>

using namespace llvm;

namespace DevirtAnalysis {

    using VirtualizationCandidates = std::map<std::string, GlobalValue *>;

    VirtualizationCandidates work(Module &M);

    struct DevirtAnalyzer : public llvm::AnalysisInfoMixin<DevirtAnalyzer> {
        using Result = VirtualizationCandidates;

        //we are not a required pass, as we don't change semantics
        static bool isRequired() { return false; }

        Result run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
            return work(M);
        }

    private:
        static llvm::AnalysisKey Key;
        friend struct llvm::AnalysisInfoMixin<DevirtAnalyzer>;
    };

//------------------------------------------------------------------------------
// New PM interface for the printer pass
//------------------------------------------------------------------------------
    class DevirtAnalyzerPrinter : public llvm::PassInfoMixin<DevirtAnalyzerPrinter> {
    public:
        explicit DevirtAnalyzerPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}

        void printDevirtAnalyzerResults(raw_ostream &OutS, const VirtualizationCandidates &candidates) {
            //todo: implement this
            outs() << "There \n";
        }

        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
            auto candidates = MAM.getResult<DevirtAnalyzer>(M);
            printDevirtAnalyzerResults(OS, candidates);
            return PreservedAnalyses::all();
        }

        //We always want to output out results if we are explicitly asked for it
        static bool isRequired() { return true; }

    private:
        llvm::raw_ostream &OS;
    };

}
#endif // CALLGRAPHGENERATION_DEVIRTANALYZER_H