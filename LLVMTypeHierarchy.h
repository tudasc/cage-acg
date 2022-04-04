//
// Created by tim on 31.03.22.
//

#ifndef CALLGRAPHGENERATION_LLVMTYPEHIERARCHY_H
#define CALLGRAPHGENERATION_LLVMTYPEHIERARCHY_H

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <llvm/Demangle/Demangle.h>

using namespace llvm;
using typeHierarchyDemo = std::pair<std::unordered_map<std::string, const llvm::GlobalVariable *>, std::unordered_map<std::string, const llvm::GlobalVariable *>>;


struct typeHierarchy {
    std::string name;
    std::vector<Function *> functions;
    std::vector<std::shared_ptr<typeHierarchy>> parents;
};

using typeHierarchyMap = std::unordered_map<std::string, std::shared_ptr<typeHierarchy>>;

static void printTypeHierarchyAnalyzerResults(raw_ostream &OutS, const typeHierarchyMap &hierarchy) {
    //todo: implement this
    outs() << "There are: " << hierarchy.size() << " vtables\n";
    for (const auto &elem: hierarchy) {
        outs() << "VTable for: " << elem.first << " contains:\n";
        for(auto elem2: elem.second->functions){
            outs()<< demangle(elem2->getName().str())<<"\n";
        }
    }
    outs() << "--------------------------------\n";

    //outs()<<"Printing the type hierarchy analysis result is not yet implmeneted\n";
}

const std::string StructPrefix = "struct.";
const std::string ClassPrefix = "class.";
const std::string VTablePrefix = "_ZTV";
const std::string VTablePrefixDemang = "vtable for ";
const std::string TypeInfoPrefix = "_ZTI";
const std::string TypeInfoPrefixDemang = "typeinfo for ";

bool isTypeInfo(const std::string &VarName) {
    auto Demang = demangle(VarName.c_str());
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


std::vector<Function *> getFunctionsFromVtable(const GlobalVariable &Global) {
    //wie generiert  clang code insbesondere mit function pointer
    //nur selbst analysieren wenns nicht wirklich viel ist
    //evtl vorhandene optionen nutzen?
    //
    std::vector<Function *> ret={};
    assert(Global.getType()->isPointerTy());
    assert(Global.getNumOperands()==1);
    auto* vtableStruct = cast<ConstantStruct>(Global.getOperand(0));
    assert(vtableStruct->getNumOperands() == 1);
    assert(vtableStruct->getAggregateElement((unsigned int) 0)->getType()->isArrayTy());
    auto *vtable = cast<ConstantArray>(vtableStruct->getAggregateElement((unsigned int) 0));
    for(auto* elem : vtable->operand_values()){
        auto constant=cast<Constant>(elem);
        if(!constant->isNullValue()){
            assert(isa<ConstantExpr>(constant));
            auto expr=cast<ConstantExpr>(constant);
            assert(expr->isCast());
            assert(expr->getNumOperands()==1);
            auto typeInfoOrFunctionRef=expr->getOperand(0);
            if(isa<Function>(typeInfoOrFunctionRef)){
                ret.push_back(cast<Function>(typeInfoOrFunctionRef));
            }else{
                outs()<<"Found More TypeInfo:\n";
                typeInfoOrFunctionRef->dump();
            }
        }
    }
    return ret;
}

typeHierarchyMap work(Module &M) {
    typeHierarchyMap ret;
    for (const auto &Global: M.globals()) {
        if (Global.hasName()) {
            //get type info if available (rtti)
            if (isTypeInfo(Global.getName().str())) {
                auto Demang = demangle(Global.getName().str());
                auto ClearName = removeTypeInfoPrefix(Demang);
                //Todo: do something with type info, haven't yet figured out what
            }
            //todo: test if internal is the right criterion
            if (isVTable(Global.getName().str()) && Global.hasInternalLinkage()) {
                assert(Global.hasMetadata());
                SmallVector<std::pair<unsigned int, MDNode *>> b;
                Global.getAllMetadata(b);
                auto Demang = demangle(Global.getName().str());
                auto ClearName = removeVTablePrefix(Demang);
                typeHierarchy t = {ClearName, getFunctionsFromVtable(Global), {}};
                ret[t.name]=std::make_shared<typeHierarchy>(t);
            }
        } else {
            outs() << "Found Global without name, dumping:\n";
            Global.dump();
        }
    }

    return ret;
}

typeHierarchyDemo work2(Module &M) {
    work(M);
    std::unordered_map<std::string, const llvm::GlobalVariable *> ClearNameTIMap;
    std::unordered_map<std::string, const llvm::GlobalVariable *> ClearNameTVMap;
    for (const auto &Global: M.globals()) {
        if (Global.hasName()) {
            //get type info if available (rtti)
            if (isTypeInfo(Global.getName().str())) {
                auto Demang = demangle(Global.getName().str());
                auto ClearName = removeTypeInfoPrefix(Demang);
                ClearNameTIMap[ClearName] = &Global;
            }
            //todo: test if internal is the right criterion
            if (isVTable(Global.getName().str()) && Global.hasInternalLinkage()) {
                assert(Global.hasMetadata());
                SmallVector<std::pair<unsigned int, MDNode *>> b;
                Global.getAllMetadata(b);

                outs() << "Dumping mdnode operands:\n";
                for (auto elem: b) {
                    auto a = elem.second->getOperand(1).get();
                    outs() << MetadataAsValue::get(M.getContext(), a) << "\n";

                }
                outs() << "------------------------------\n";


                auto Demang = demangle(Global.getName().str());
                auto ClearName = removeVTablePrefix(Demang);
                ClearNameTVMap[ClearName] = &Global;
            }
        } else {
            outs() << "Found Global without name, dumping:\n";
            Global.dump();
        }
    }

    return {ClearNameTIMap, ClearNameTVMap};
}


struct TypeHierarchyAnalyzer : public llvm::AnalysisInfoMixin<TypeHierarchyAnalyzer> {
    using Result = typeHierarchyMap;

    //we are not a required pass, as we don't change semantics
    static bool isRequired() { return false; }

    Result run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
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

    typeHierarchyMap hierarchy;
};


//------------------------------------------------------------------------------
// New PM interface for the printer pass
//------------------------------------------------------------------------------
class TypeHierarchyAnalyzerPrinter : public llvm::PassInfoMixin<TypeHierarchyAnalyzerPrinter> {
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