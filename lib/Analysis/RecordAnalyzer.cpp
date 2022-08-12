//
// Created by tim on 31.03.22.
//

#include "Analysis/RecordAnalyzer.h"

namespace RecordAnalysis{

using RecordMap = std::unordered_map<std::string, std::shared_ptr<RecordInformation>>;

void printRecordAnalyzerResults(raw_ostream &OutS, const RecordMap &recordMap) {
    //todo: implement this
    outs() << "There are: " << recordMap.size() << " vtables\n";
    for (const auto &elem: recordMap) {
        outs() << "VTable for: " << elem.first << " contains:\n";
        for (auto elem2: elem.second->vtable.functions) {
            outs() << demangle(elem2->getName().str()) << "\n";
        }
        outs() << "A Pointer of this type could call methods from:\n";
        for (auto elem2: elem.second->parents) {
            outs() << elem2->name << "\n";
        }
    }
    outs() << "--------------------------------\n";

    //outs()<<"Printing the type recordMap analysis result is not yet implmeneted\n";
}

bool isTypeInfo(const std::string &VarName) {
    auto Demang = demangle(VarName);
    return llvm::StringRef(Demang).startswith(TypeInfoPrefixDemang);
}

bool isVTable(const std::string &VarName) {
    auto Demang = demangle(VarName);
    return llvm::StringRef(Demang).startswith(VTablePrefixDemang);
}

bool isThunk(const std::string &VarName) {
    auto Demang = demangle(VarName);
    return llvm::StringRef(Demang).startswith(NonVirtualThunkPrefixDemang) ||
           llvm::StringRef(Demang).startswith(VirtualThunkPrefixDemang);
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

std::string removeStructPrefix(std::string VarName) {
    llvm::StringRef SR(VarName);
    if (SR.startswith(StructPrefix)) {
        return SR.drop_front(StructPrefix.size()).str();
    }
    return VarName;
}

std::string removeThunkPrefix(std::string VarName) {
    llvm::StringRef SR(VarName);
    if (SR.startswith(NonVirtualThunkPrefixDemang)) {
        return SR.drop_front(NonVirtualThunkPrefixDemang.size()).str();
    }
    if (SR.startswith(VirtualThunkPrefixDemang)) {
        return SR.drop_front(VirtualThunkPrefixDemang.size()).str();
    }
    if (SR.startswith(NonVirtualThunkPrefix)) {
        return SR.drop_front(NonVirtualThunkPrefix.size()).str();
    }
    return VarName;
}

std::string guessNameFromThunk(std::string VarName) {
    auto removedStub=removeThunkPrefix(VarName);
    llvm::StringRef SR(removedStub);
    int dispose;
    SR.consumeInteger(10,dispose);
    auto ret="_Z"+SR.drop_front().str();
    return ret;
}

Function* getThunkFunction(Function *vtableFunction) {
    //if the instruction before return is a call, it is a non trivial thunk,
    //the actual virtual function is the one before
    if (vtableFunction->back().back().getPrevNonDebugInstruction()) {
        assert(isa<ReturnInst>(vtableFunction->back().back()));
        //Dont think invoke can be generated here ?
        assert(isa<CallInst>(vtableFunction->back().back().getPrevNonDebugInstruction()));
        assert(cast<ReturnInst>(vtableFunction->back().back()).getNumOperands() == 1);
        cast<ReturnInst>(vtableFunction->back().back()).getOperand((unsigned int) 0)->dump();
        return cast<CallInst>(vtableFunction->back().back().getPrevNonDebugInstruction())->getFunction();
    } else {
        //if there is no instruction in front of return, we have a trivial thunk,
        //try to find the original function by name,
        //or return the thunk itself if impossible
        auto f = vtableFunction->getParent()->getFunction(guessNameFromThunk(vtableFunction->getName().str()));
        if (f) return f;
        else return vtableFunction;
    }
}

Vtable toVtable(const GlobalVariable &Global) {
    //wie generiert  clang code insbesondere mit function pointer
    //nur selbst analysieren wenns nicht wirklich viel ist
    //evtl vorhandene optionen nutzen?

    Vtable ret = {};
    assert(Global.getType()->isPointerTy());
    assert(Global.getNumOperands() == 1);
    auto *vtableStruct = cast<ConstantStruct>(Global.getOperand(0));
    for (unsigned int i = 0, e = vtableStruct->getNumOperands(); i < e; i++) {
        assert(vtableStruct->getAggregateElement(i)->getType()->isArrayTy());
        auto *vtable = cast<ConstantArray>(vtableStruct->getAggregateElement(i));
        for (auto *elem: vtable->operand_values()) {
            auto constant = cast<Constant>(elem);
            //This skips all null entries
            //These exist for missing: Top Offsets, RTTI, Some other stuff
            //This is bad and should be improved by switching handling according to vtable category
            if (constant->isNullValue()) {
                continue;
            }

            assert(isa<ConstantExpr>(constant));
            auto expr = cast<ConstantExpr>(constant);

            assert(expr->isCast());
            assert(expr->getNumOperands() == 1);

            if (isa<Function>(expr->getOperand(0))) {
                auto vtableFunction = cast<Function>(expr->getOperand(0));
                assert(vtableFunction->hasName());
                //assumption is, that no thunk function will ever change the set
                //Todo: find in standard, or at least validate empirically
                if (isThunk(vtableFunction->getName().str())) {
                    //outs() << "Handling thunk: " << vtableFunction->getName() << "\n";
                    ret.functions.push_back(getThunkFunction(vtableFunction));
                } else {
                    ret.functions.push_back(vtableFunction);
                }
            } else {
                //outs() << "Probably Pointer Offset or TypeInfo:";
                //expr->dump();
            }

        }
    }
    return ret;
}

StructType* getFunctionOriginStruct(Function &f) {
    if (auto functionType = f.getFunctionType()) {
        if (auto paramType = dyn_cast<PointerType>(functionType->getParamType(0))) {
            return paramType->getElementType()->isStructTy() ? cast<StructType>(paramType->getElementType())
                                                             : nullptr;
        }
    }
    return nullptr;
}

void linkTypeHierarchyMap(RecordMap &map, Module& M) {
    for(auto& g : M.getGlobalList()){
        if(g.hasName() && isVTable(g.getName().str())){
            auto gName= removeVTablePrefix(demangle(g.getName().str()));
            SmallVector<std::pair<unsigned, MDNode *>> MD;
            g.getAllMetadata(MD);
            for(auto MDPair : MD){
                assert(MDPair.second->getNumOperands()==2);
                assert(isa<MDString>(MDPair.second->getOperand(1)));
                llvm::StringRef metaDataStringRef=cast<MDString>(MDPair.second->getOperand(1))->getString();;
                if(metaDataStringRef.endswith(".virtual")){
                    continue;
                } else{
                    auto demangledTypeInfo = demangle(metaDataStringRef.str());
                    llvm::StringRef demangledTypeInfoRef(demangledTypeInfo);
                    assert( demangledTypeInfoRef.startswith(TypeInfoNamePrefixDemang));
                    auto parentName=demangledTypeInfoRef.drop_front(TypeInfoNamePrefixDemang.size()).str();
                    if(map.count(parentName)==0 || map.count(gName)==0){
                      outs()<<"parentName: "<<parentName<<" count: "<<map.count(parentName) << "child: "<<map.count(gName)<<"\n";
                      continue;
                    }
                    map.at(parentName)->parents.insert(map.at(gName));
                }
            }
        }
    }
}

RecordMap work(Module &M) {
    RecordMap ret;
    for (const auto &Global: M.globals()) {
        if (Global.hasName()) {
            //get type info if available (rtti)
            if (isTypeInfo(Global.getName().str())) {
                auto Demang = demangle(Global.getName().str());
                auto ClearName = removeTypeInfoPrefix(Demang);
                //Todo: do something with type info, haven't yet figured out what
            }
            //todo: test if internal is the right exclusion criterion
            if (isVTable(Global.getName().str()) && Global.hasInternalLinkage()) {
                //assert(Global.hasMetadata());
                //SmallVector<std::pair<unsigned int, MDNode *>> b;
                //Global.getAllMetadata(b);
                auto Demang = demangle(Global.getName().str());
                auto ClearName = removeVTablePrefix(Demang);
                auto functionFromVtable = toVtable(Global);
                RecordInformation t = {ClearName, functionFromVtable, {}};
                ret[t.name] = std::make_shared<RecordInformation>(t);
            }
        } else {
            outs() << "Found Global without name, dumping:\n";
            Global.dump();
        }
    }
    linkTypeHierarchyMap(ret, M);

    return ret;
}
}