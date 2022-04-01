//
// Created by tim on 31.03.22.
//

#ifndef CALLGRAPHGENERATION_LLVMTYPEHIERARCHY_H
#define CALLGRAPHGENERATION_LLVMTYPEHIERARCHY_H
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "LLVMTypeHierarchy.h"

using namespace llvm;
using typeHierarchy = std::pair<std::unordered_map<std::string, const llvm::GlobalVariable *>, std::unordered_map<std::string, const llvm::GlobalVariable *>>;

static void printTypeHierarchyAnalyzerResults(raw_ostream &OutS, const typeHierarchy &hierarchy) {
    //todo: implement this
    outs()<<"Printin the type hierarchy analysis result is not yet implmeneted\n";
}

const std::string StructPrefix = "struct.";
const std::string ClassPrefix = "class.";
const std::string VTablePrefix = "_ZTV";
const std::string VTablePrefixDemang = "vtable for ";
const std::string TypeInfoPrefix = "_ZTI";
const std::string TypeInfoPrefixDemang = "typeinfo for ";

bool isTypeInfo(const std::string &VarName) {
    auto Demang =demangle(VarName.c_str());
    return llvm::StringRef(Demang).startswith(TypeInfoPrefixDemang);
}

bool isVTable(const std::string &VarName) {
    auto Demang = demangle(VarName.c_str());
    return llvm::StringRef(Demang).startswith(VTablePrefixDemang);
}

std::string removeTypeInfoPrefix(std::string VarName) {
    llvm::StringRef SR(VarName);
    if (SR.startswith(TypeInfoPrefixDemang)) {
        return SR.drop_front(TypeInfoPrefixDemang.size()).str();
    }
    if (SR.startswith(TypeInfoPrefix)) {
        return SR.drop_front(TypeInfoPrefix.size()).str();
    }
    return VarName;
}

std::string removeVTablePrefix(std::string VarName) {
    llvm::StringRef SR(VarName);
    if (SR.startswith(VTablePrefixDemang)) {
        return SR.drop_front(VTablePrefixDemang.size()).str();
    }
    if (SR.startswith(VTablePrefix)) {
        return SR.drop_front(VTablePrefix.size()).str();
    }
    return VarName;
}

typeHierarchy work(Module &M) {
    std::unordered_map<std::string, const llvm::GlobalVariable *> ClearNameTIMap;
    std::unordered_map<std::string, const llvm::GlobalVariable *> ClearNameTVMap;
    for (const auto &Global : M.globals()) {
        if (Global.hasName()) {
            if (isTypeInfo(Global.getName().str())) {
                auto Demang = demangle(Global.getName().str());
                auto ClearName = removeTypeInfoPrefix(Demang);
                ClearNameTIMap[ClearName] = &Global;
            }
            if (isVTable(Global.getName().str())) {
                auto Demang = demangle(Global.getName().str());
                auto ClearName = removeVTablePrefix(Demang);
                ClearNameTVMap[ClearName] = &Global;
            }
        }
    }

    return {ClearNameTIMap,ClearNameTVMap};
}

struct TypeHierarchyAnalyzer : public llvm::AnalysisInfoMixin<TypeHierarchyAnalyzer> {
    using Result = typeHierarchy;
    //we are not a required pass, as we don't change semantics
    static bool isRequired() { return false; }
    typeHierarchy run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
        return work(M);
    }

private:
    static llvm::AnalysisKey Key;
    friend struct llvm::AnalysisInfoMixin<TypeHierarchyAnalyzer>;
};

//------------------------------------------------------------------------------
// Legacy PM interface
//------------------------------------------------------------------------------
struct LegacyTypeHierarchyAnalyzer : public llvm::ModulePass {
    static char ID;
    LegacyTypeHierarchyAnalyzer() : llvm::ModulePass(ID) {}
    bool runOnModule(llvm::Module &M) {
        hierarchy = work(M);
        return false;
    }
    // The print method must be implemented by Legacy analysis passes in order to
    // print a human readable version of the analysis results:
    void print(raw_ostream &OutS, Module const *) const {
        printTypeHierarchyAnalyzerResults(OutS, hierarchy);
    }

    typeHierarchy hierarchy;
};


//------------------------------------------------------------------------------
// New PM interface for the printer pass
//------------------------------------------------------------------------------
class TypeHierarchyAnalyzerPrinter: public llvm::PassInfoMixin<TypeHierarchyAnalyzerPrinter> {
public:
    explicit TypeHierarchyAnalyzerPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
        auto typeHierarchy = MAM.getResult<TypeHierarchyAnalyzer>(M);
        printTypeHierarchyAnalyzerResults(OS, typeHierarchy);
        return PreservedAnalyses::all();
    }

    //We always want do output out results if we are explicitly asked for it
    static bool isRequired() { return true; }

private:
    llvm::raw_ostream &OS;
};


char LegacyTypeHierarchyAnalyzer::ID = 0;
AnalysisKey TypeHierarchyAnalyzer::Key;

// #1 REGISTRATION FOR "opt -analyze -legacy-static-cc"
static RegisterPass<LegacyTypeHierarchyAnalyzer>
        typeHierarchyRegister(/*PassArg=*/"legacy-type-hierarchy",
        /*Name=*/"LegacyTypeHierarchyAnalyzer",
        /*CFGOnly=*/true,
        /*is_analysis=*/true);
#endif