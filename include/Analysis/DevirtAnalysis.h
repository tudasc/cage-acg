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





namespace WholeProgramDevirtAnalysis {

    //std::map<std::string, GlobalValue *> work(Module &M);


    struct WholeProgramDevirtPass : public llvm::AnalysisInfoMixin<WholeProgramDevirtPass> {
        ModuleSummaryIndex *ExportSummary;
        const ModuleSummaryIndex *ImportSummary;
        bool UseCommandLine = false;
        using Result = std::map<std::string, GlobalValue *>;
        WholeProgramDevirtPass()
                : ExportSummary(nullptr), ImportSummary(nullptr), UseCommandLine(true) {}

        WholeProgramDevirtPass(ModuleSummaryIndex *ExportSummary,
                               const ModuleSummaryIndex *ImportSummary)
                : ExportSummary(ExportSummary), ImportSummary(ImportSummary) {
            assert(!(ExportSummary && ImportSummary));
        }
        Result run(Module &M, ModuleAnalysisManager &);
    private:
        static llvm::AnalysisKey Key;
        friend struct llvm::AnalysisInfoMixin<WholeProgramDevirtPass>;
    };

//------------------------------------------------------------------------------
// Legacy PM interface
//------------------------------------------------------------------------------
    struct LegacyRecordAnalyzer : public llvm::ModulePass {


        LegacyRecordAnalyzer() : llvm::ModulePass(ID) {}

        bool runOnModule(llvm::Module &M) {
            //recordMap = work(M);
            return false;
        }

        // The print method must be implemented by Legacy analysis passes in order to
        // print a human readable version of the analysis results:
        void print(raw_ostream &OutS, Module const *) const {
            //printRecordAnalyzerResults(OutS, recordMap);
        }

        static char ID;
        std::map<std::string, GlobalValue *>  recordMap;
    };


//------------------------------------------------------------------------------
// New PM interface for the printer pass
//------------------------------------------------------------------------------
    class RecordAnalyzerPrinter : public llvm::PassInfoMixin<RecordAnalyzerPrinter> {
    public:
        explicit RecordAnalyzerPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}

        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
            auto recordMap = MAM.getResult<WholeProgramDevirtPass>(M);
            //printRecordAnalyzerResults(OS, recordMap);
            return PreservedAnalyses::all();
        }

        //We always want to output out results if we are explicitly asked for it
        static bool isRequired() { return true; }

    private:
        llvm::raw_ostream &OS;
    };

}// end of WholeProgramDevirtAnalysis
#endif //CALLGRAPHGENERATION_DEVIRTANALYZER_H