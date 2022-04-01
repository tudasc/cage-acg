#include <llvm/Analysis/CallGraph.h>
#include <sstream>
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

#include <clang/Analysis/CallGraph.h>

#include "Callgraph.h"
#include "CgNode.h"

#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <MCGManager.h>

#include "LLVMTypeHierarchy.h"

using namespace llvm;

//static cl::opt<bool> enableGenCC("genCC", cl::init(false),
//                                 cl::desc("generates call-graph component"));

namespace genCC {

    void generateLibraryFunction(Module &M) {
        assert(M.getFunction("getGCC") == nullptr && "could not add getGCC runtime component call");

        FunctionType *getCallGraphFT = FunctionType::get(Type::getVoidTy(M.getContext()),
                                                         {Type::getInt8PtrTy(M.getContext())},
                                                         false);

        M.getOrInsertFunction("getGCC", getCallGraphFT);
    }

    void generateInitFunction(Module &M) {
        assert(M.getFunction("genCCInit") == nullptr && "genCCInit function already exists");

        FunctionType *InitFT = FunctionType::get(Type::getVoidTy(M.getContext()),
                                                 {},
                                                 false);

        M.getOrInsertFunction("genCCInit", InitFT);
        auto &InitFunctionBBList = M.getFunction("genCCInit")->getBasicBlockList();
        InitFunctionBBList.insert(InitFunctionBBList.begin(), BasicBlock::Create(M.getContext()));
        InitFunctionBBList.front().getInstList().insert(InitFunctionBBList.front().getInstList().begin(),
                                                        ReturnInst::Create(M.getContext()));
        appendToGlobalCtors(M, M.getFunction("genCCInit"), 101);
    }

    void passToRuntimeComponent(Module &M, Value *Arg) {
        auto &FIlist = M.getFunction("genCCInit")->front().getInstList();
        auto bitCast = BitCastInst::CreateBitOrPointerCast(Arg, Type::getInt8PtrTy(M.getContext()));
        FIlist.insert((--FIlist.end()), bitCast);
        FIlist.insert((--FIlist.end()), CallInst::Create(M.getFunction("getGCC"), bitCast));
    }

    void generateVTableLookup(Module &M) {
        for (auto &g: M.globals()) {

            //wie generiert  clang code insbesondere mit function pointer
            //nur selbst analysieren wenns nicht wirklich viel ist
            //evtl vorhandene optionen nutzen?
            //

            if (g.hasName() && llvm::StringRef(demangle(g.getName().str())).startswith("vtable")) {
                outs() << "The Vtable for: " << g.getName() << " (" << demangle(g.getName().str()) << ")\n";
                if(!g.hasInternalLinkage()){
                    outs()<<"is not internally linked\n";
                    if(g.hasExternalLinkage()){
                        outs()<<"it is instead externally linked, no information available\n\n";
                    }else{
                        outs()<<"THIS SHOULD NEVER HAPPEN!!\n";
                    }
                    continue;
                }

                outs() << "Contains:\n";
                //SmallVector<std::pair<unsigned int, MDNode *>> allMetadata = {};
                //g.getAllMetadata(allMetadata);
                g.dump();

                //Globas are allways pointer types
                if (!g.getType()->isPointerTy()) {
                    outs() << "Apparently g was no pointer type?\n";
                    continue;
                }
                //vtable should always be constant struct with exactly one array inside
                if(g.op_begin()==g.op_end()){
                    outs()<<"Global does not contain any operand values\n";
                    continue;

                }
                auto &opv = *g.op_begin();
                if(opv== nullptr){
                    outs()<<"NULL?\n";
                    continue;
                }

                if (!opv->getType()->isStructTy()) {
                    outs() << "Apparently operand was no struct type?\n";
                    continue;
                }
                auto *struc = cast<ConstantStruct>(opv);
                if(struc->getNumOperands() != 1){
                    outs() << "Apparently the struct contains more than one arrays?\n";
                    outs() << "this might occur on multiple inheritance, haven't tested that?\n";
                    continue;

                }

                if(!struc->getOperand(0)->getType()->isArrayTy()){
                    outs() << "does not contain an array to model the vtable, why?\n";
                    continue;

                }

                //these are all function ptr references of the vtable
                for (auto &ptr: cast<ConstantArray>(struc->getOperand(0))->operands()) {
                    ptr->dump();
                };
                outs() << "-----------------------------\n\n";

            }
        }

    }

    metacg::Callgraph llvmCallGraphToMetaCG(CallGraphAnalysis::Result &llvmCG) {

        metacg::graph::MCGManager &mcgManager = metacg::graph::MCGManager::get();
        mcgManager.addToManagedGraphs("graph",std::make_unique<metacg::Callgraph>());

        for (auto &llvmNode: llvmCG) {
            if (llvmNode.first == NULL) {
                outs() << "Null Node for Entry, skip\n";
            } else {
                if (llvmNode.first->hasName()) {
                    mcgManager.findOrCreateNode(llvmNode.first->getName().str());
                    auto n1 = mcgManager.findOrCreateNode(llvmNode.first->getName().str());

                    auto &function = llvmNode.first->getFunction();

                    n1->setFilename(function.getParent()->getSourceFileName());
                    n1->setHasBody(function.getInstructionCount() != 0);


                    for (auto node: *llvmNode.second) {
                        if (node.first.hasValue()) {
                            auto n2 = mcgManager.findOrCreateNode(node.first.getValue()->getName().str());
                            mcgManager.addEdge(n1, n2);
                        } else {
                            //outs() << "Function:" << n1->getFunctionName() << " calls external node\n";
                        }
                    }
                    //outs() << "Finished creating call Edges!\n";
                } else {
                    //outs() << "Found function without name!\n";
                }
            }
        }

        llvm::outs() << "Completed insertion of all nodes and edges\n";


        return *(mcgManager.getCallgraph());
    }

    bool work(Module &M, ModuleAnalysisManager *MA) {
        generateLibraryFunction(M);
        generateInitFunction(M);
        generateVTableLookup(M);


        /**Get function Information and dump to module
        **/
        {
            std::string callGraph1;
            for (Module::iterator F = M.begin(), Fe = M.end(); F != Fe; ++F) {
                callGraph1.append("FunctionName:");
                callGraph1.append(F->getName().str());
                callGraph1.append(";Instructions:");
                callGraph1.append(std::to_string(F->getInstructionCount()));
                callGraph1.append(";");
            }

            auto insertableCallgraph1 = ConstantDataArray::getString(M.getContext(), callGraph1, true);
            M.getOrInsertGlobal("CallGraph1", insertableCallgraph1->getType());
            auto global1 = M.getNamedGlobal("CallGraph1");
            global1->setLinkage(llvm::GlobalValue::ExternalLinkage);
            global1->setAlignment(MaybeAlign(1));
            global1->setInitializer(insertableCallgraph1);

            passToRuntimeComponent(M, global1);


        }

        /** Use callgraph information provided by CGA Pass
         *  and dump that to module aswell
         */
        {
            auto &cgResult = MA->getResult<CallGraphAnalysis>(M);
            auto anaRes = MA->getResult<TypeHierarchyAnalyzer>(M);
            outs()<<"AnaRes: "<<anaRes.first.size()<<"; "<<anaRes.second.size()<<"\n";
            for(const auto& elem : anaRes.first){
                outs()<<"Type: "<<elem.first<<"\n";
                elem.second->dump();
            }
            outs()<<"--------------------------------\n";
            for(const auto& elem : anaRes.second){
                outs()<<"VTable: "<<elem.first<<"\n";
                elem.second->dump();
            }
            outs()<<"--------------------------------\n";

            //cgResult.print(outs());

            std::string callGraph2;
            llvm::raw_string_ostream llvmso(callGraph2);

            cgResult.print(llvmso);
            cgResult.dump();

            auto mcg = llvmCallGraphToMetaCG(cgResult);
            //maybe steal phasar aproach
            //maybe use clang to pre generate virtual metadata in more readable format
            //focus on more common cases and stay in llvm ir

            auto insertableCallgraph2 = ConstantDataArray::getString(M.getContext(), callGraph2, true);
            M.getOrInsertGlobal("CallGraph2", insertableCallgraph2->getType());
            auto global2 = M.getNamedGlobal("CallGraph2");
            global2->setLinkage(llvm::GlobalValue::ExternalLinkage);
            global2->setAlignment(MaybeAlign(1));
            global2->setInitializer(insertableCallgraph2);
            passToRuntimeComponent(M, global2);
        }


        return false;
    }


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

char genCC::LegacyGenCC::ID = 0;

static RegisterPass<genCC::LegacyGenCC> genCCRegistrar("legacy-genCC", "generates Call Grap Components (legacy)",
                                          false /* Only looks at CFG */,
                                          false /* Analysis Pass */);

/* Legacy PM Registration */
static llvm::RegisterStandardPasses RegisterGenCC(
        llvm::PassManagerBuilder::EP_OptimizerLast,
        [](const llvm::PassManagerBuilder &Builder,
           llvm::legacy::PassManagerBase &PM) { PM.add(new genCC::LegacyGenCC()); }
);

/* New PM Registration */
//todo make registration separate, maybe use tblgen ?
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
                // #1 REGISTRATION FOR "opt -passes=print<type-hierarchy>"
                PB.registerPipelineParsingCallback(
                        [&](StringRef Name, ModulePassManager &MPM,
                            ArrayRef<PassBuilder::PipelineElement>) {
                            if (Name == "print<type-hierarchy>") {
                                MPM.addPass(TypeHierarchyAnalyzerPrinter(llvm::errs()));
                                return true;
                            }
                            return false;
                        });
                // #2 REGISTRATION FOR "MAM.getResult<TypeHierarchyAnalyzer>(Module)"
                PB.registerAnalysisRegistrationCallback(
                        [](ModuleAnalysisManager &MAM) {
                            MAM.registerPass([&] { return TypeHierarchyAnalyzer(); });
                        });
            }};
}

#ifndef LLVM_GENCC_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getGenCCPluginInfo();
}
#endif
