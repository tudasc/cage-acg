//
// Created by tim on 31.03.22.
//

#ifndef CALLGRAPHGENERATION_RECORDANALYZER_H
#define CALLGRAPHGENERATION_RECORDANALYZER_H

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <llvm/Demangle/Demangle.h>
#include <llvm/Support/Debug.h>

using namespace llvm;

namespace RecordAnalysis {

    struct TypeInfo {
    };

    struct Vtable {
        int64_t offset = -1;
        TypeInfo *typeInfo = nullptr;
        //while order is important (i think)
        //no function can occure twice, so we can use a set
        //no we cant you dumbass
        //we need indexing you idiot
        std::vector<Function *> functions;
    };

    struct RecordInformation {
        std::string name;
        Vtable vtable;
        std::unordered_set<std::shared_ptr<RecordInformation>> parents;
    };

    using RecordMap = std::unordered_map<std::string, std::shared_ptr<RecordInformation>>;

    void printRecordAnalyzerResults(raw_ostream &OutS, const RecordMap &recordMap);

    const std::string StructPrefix = "struct.";
    const std::string ClassPrefix = "class.";
    const std::string VTablePrefix = "_ZTV";
    const std::string VTablePrefixDemang = "vtable for ";
    const std::string TypeInfoPrefix = "_ZTI";
    const std::string TypeInfoPrefixDemang = "typeinfo for ";
    const std::string TypeInfoNamePrefixDemang = "typeinfo name for ";
    const std::string NonVirtualThunkPrefix = "_ZThn";
    const std::string NonVirtualThunkPrefixDemang = "non-virtual thunk to ";
    const std::string VirtualThunkPrefixDemang = "virtual thunk to ";

    bool isTypeInfo(const std::string &VarName);

    bool isVTable(const std::string &VarName);

    bool isThunk(const std::string &VarName);

    std::string removeTypeInfoPrefix(std::string VarName);

    std::string removeVTablePrefix(std::string VarName);

    std::string removeStructPrefix(std::string VarName);

    std::string removeThunkPrefix(std::string VarName);

    std::string guessNameFromThunk(std::string VarName);

    Function *getThunkFunction(Function *vtableFunction);

    Vtable toVtable(const GlobalVariable &Global);

    StructType *getFunctionOriginStruct(Function &f);

    void linkTypeHierarchyMap(RecordMap &map, Module &M);

    RecordMap work(Module &M);

    struct RecordAnalyzer : public llvm::AnalysisInfoMixin<RecordAnalyzer> {
        using Result = RecordMap;

        //we are not a required pass, as we don't change semantics
        static bool isRequired() { return false; }

        Result run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
            return work(M);
        }

    private:
        static llvm::AnalysisKey Key;
        friend struct llvm::AnalysisInfoMixin<RecordAnalyzer>;
    };

//------------------------------------------------------------------------------
// Legacy PM interface
//------------------------------------------------------------------------------
    struct LegacyRecordAnalyzer : public llvm::ModulePass {


        LegacyRecordAnalyzer() : llvm::ModulePass(ID) {}

        bool runOnModule(llvm::Module &M) {
            recordMap = work(M);
            return false;
        }

        // The print method must be implemented by Legacy analysis passes in order to
        // print a human readable version of the analysis results:
        void print(raw_ostream &OutS, Module const *) const {
            printRecordAnalyzerResults(OutS, recordMap);
        }

        static char ID;
        RecordMap recordMap;
    };


//------------------------------------------------------------------------------
// New PM interface for the printer pass
//------------------------------------------------------------------------------
    class RecordAnalyzerPrinter : public llvm::PassInfoMixin<RecordAnalyzerPrinter> {
    public:
        explicit RecordAnalyzerPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}

        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
            auto recordMap = MAM.getResult<RecordAnalyzer>(M);
            printRecordAnalyzerResults(OS, recordMap);
            return PreservedAnalyses::all();
        }

        //We always want to output out results if we are explicitly asked for it
        static bool isRequired() { return true; }

    private:
        llvm::raw_ostream &OS;
    };

}
#endif