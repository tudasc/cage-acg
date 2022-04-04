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

    metacg::Callgraph llvmCallGraphToMetaCG(CallGraphAnalysis::Result &llvmCG) {

        metacg::graph::MCGManager &mcgManager = metacg::graph::MCGManager::get();
        mcgManager.addToManagedGraphs("graph", std::make_unique<metacg::Callgraph>());

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

    StructType *getFunctionOriginStruct(CallInst &callInst) {
        if (auto ptrType = callInst.getCalledOperand()->getType()) {
            if (auto functionType = dyn_cast<FunctionType>(ptrType->getPointerElementType())) {
                if (auto paramType = dyn_cast<PointerType>(functionType->getParamType(0))) {
                    return paramType->getElementType()->isStructTy() ? cast<StructType>(paramType->getElementType())
                                                                     : nullptr;
                }
            }
        }
        return nullptr;
    }

    bool work(Module &M, ModuleAnalysisManager *MA) {
        generateLibraryFunction(M);
        generateInitFunction(M);


        /**Get function Information and dump to module
        **/
        /* Left in for demo purpose
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
            global1->setLinkage(llvm::GlobalValue::InternalLinkage);
            global1->setAlignment(MaybeAlign(1));
            global1->setInitializer(insertableCallgraph1);

            passToRuntimeComponent(M, global1);


        }
        */

        /** Use callgraph information provided by CGA Pass
         *  and dump that to module aswell
         */

        auto &cgResult = MA->getResult<CallGraphAnalysis>(M);
        auto anaRes = MA->getResult<TypeHierarchyAnalyzer>(M);
        printTypeHierarchyAnalyzerResults(outs(), anaRes);

        //cgResult.print(outs());

        std::string callGraph2;
        llvm::raw_string_ostream llvmso(callGraph2);

        cgResult.print(llvmso);

        auto mcg = llvmCallGraphToMetaCG(cgResult);
        //maybe steal phasar aproach
        //maybe use clang to pre generate virtual metadata in more readable format
        //focus on more common cases and stay in llvm ir

        auto insertableCallgraph2 = ConstantDataArray::getString(M.getContext(), callGraph2, true);
        M.getOrInsertGlobal("CallGraph2", insertableCallgraph2->getType());
        auto global2 = M.getNamedGlobal("CallGraph2");
        global2->setLinkage(llvm::GlobalValue::InternalLinkage);
        global2->setAlignment(MaybeAlign(1));
        global2->setInitializer(insertableCallgraph2);
        passToRuntimeComponent(M, global2);


        for (auto &F: M) {
            for (auto &B: F) {
                for (auto &I: B) {
                    if (isa<CallInst>(I)) {
                        if (auto *origin = getFunctionOriginStruct(cast<CallInst>(I))) {
                            outs() << "In function:" << demangle(F.getName().str()) << "\n";
                            outs() << "we are calling function from: " << demangle(origin->getName().str()) << "\n";
                        }
                    }
                }
            }
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
