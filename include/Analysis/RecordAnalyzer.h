//
// Created by tim on 31.03.22.
//

#ifndef CALLGRAPHGENERATION_RECORDANALYZER_H
#define CALLGRAPHGENERATION_RECORDANALYZER_H

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <llvm/Demangle/Demangle.h>
#include <llvm/Support/Debug.h>
#include <unordered_set>

using namespace llvm;

namespace RecordAnalysis {

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

    struct TypeInfo {
    };


    struct Vtable {
        int64_t offset = -1;
        TypeInfo *typeInfo = nullptr;
        std::vector<Function *> functions;
    };

    struct RecordInformation {
        std::string name;
        Vtable vtable;
        std::unordered_set<std::shared_ptr<RecordInformation>> callSet;
    };

    using RecordMap = std::unordered_map<std::string, std::shared_ptr<RecordInformation>>;

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
// New PM interface for the printer pass
//------------------------------------------------------------------------------
    class RecordAnalyzerPrinter : public llvm::PassInfoMixin<RecordAnalyzerPrinter> {
    public:
        explicit RecordAnalyzerPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}

        void printRecordAnalyzerResults(raw_ostream &OutS, const RecordMap &recordMap) {
            //todo: implement this
            outs() << "There are: " << recordMap.size() << " vtables\n";
            for (const auto &elem: recordMap) {
                outs() << "VTable for: " << elem.first << " contains:\n";
                for (auto elem2: elem.second->vtable.functions) {
                    outs() << demangle(elem2->getName().str()) << "\n";
                }
                outs() << "A Pointer of this type could call methods from:\n";
                for (auto elem2: elem.second->callSet) {
                    outs() << elem2->name << "\n";
                }
            }
            outs() << "--------------------------------\n";

            //outs()<<"Printing the type recordMap analysis result is not yet implmeneted\n";
        }

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