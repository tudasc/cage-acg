#include <llvm/Analysis/CallGraph.h>
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
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <clang/Analysis/CallGraph.h>

#include "Callgraph.h"
#include "MCGManager.h"
#include "RecordAnalyzer.h"
#include "MCGWriter.h"


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

    metacg::graph::MCGManager& llvmCallGraphToMetaCG(CallGraphAnalysis::Result &llvmCG, RecordMap rm) {

        metacg::graph::MCGManager &mcgManager = metacg::graph::MCGManager::get();
        mcgManager.addToManagedGraphs("graph", std::make_unique<metacg::Callgraph>());

        for (auto &llvmNode: llvmCG) {
            if (llvmNode.first == NULL) {
            } else {
                if (llvmNode.first->hasName()) {
                    mcgManager.findOrCreateNode(llvmNode.first->getName().str());
                    auto n1 = mcgManager.findOrCreateNode(llvmNode.first->getName().str());
                    auto &function = llvmNode.first->getFunction();
                    n1->setFilename(function.getParent()->getSourceFileName());
                    n1->setHasBody(function.getInstructionCount() != 0);

                    for (auto node: *llvmNode.second) {
                        if (node.first.hasValue()) {
                            //Todo: probably should use call base ?
                            assert(isa<CallInst>(node.first.getValue()));
                            auto callInst = cast<CallInst>(node.first.getValue());
                            auto* calledFunction = callInst->getCalledFunction();
                            if(calledFunction == nullptr){
                                outs()<<"Call was to:\n";
                                callInst->dump();
                                outs()<<"This is a function Pointer that was just loaded\n";
                                callInst->getCalledOperand()->dump();
                                assert(isa<LoadInst>(callInst->getCalledOperand()));
                                outs()<<"The gep instruction gives us the function that was loaded from the vtable\n";
                                auto functionPointer=cast<LoadInst>(callInst->getCalledOperand())->getPointerOperand();
                                functionPointer->dump();
                                if(isa<GetElementPtrInst>(functionPointer)){
                                    outs()<<"Vtable contains more then one function, we need the one of em\n";
                                    auto gep=cast<GetElementPtrInst>(functionPointer);
                                    assert(gep->getNumOperands()==2);
                                    outs()<<gep->getPointerOperandIndex()<<"\n";
                                    assert(isa<ConstantInt>(gep->getOperand(1)));
                                    rm.at(llvmNode.first->getName().str())->vtable.functions[0];
                                }else{
                                    outs()<<"Vtable contains only one function we load index 0\n";
                                }

                            }else{
                                //Fixme: this should never need to create a node
                                //implement the corresponding functionality in mcgManager
                                assert(calledFunction->hasName());
                                auto n2 = mcgManager.findOrCreateNode(calledFunction->getName().str());
                                mcgManager.addEdge(n1, n2);
                            }

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

        return mcgManager;
    }

    bool work(Module &M, ModuleAnalysisManager *MA) {
        generateLibraryFunction(M);
        generateInitFunction(M);

        /** Use callgraph information provided by CGA Pass
         *  and dump that to module as well
         */
        auto &cgResult = MA->getResult<CallGraphAnalysis>(M);
        //cgResult.print(outs());

        std::string callGraph2;
        llvm::raw_string_ostream llvmso(callGraph2);

        cgResult.print(llvmso);


        auto anaRes = MA->getResult<RecordAnalyzer>(M);
        auto& mcg = llvmCallGraphToMetaCG(cgResult,anaRes);
        //printRecordAnalyzerResults(outs(), anaRes);

        metacg::io::JsonSink jsSink;
        metacg::io::MCGWriter mcgw(mcg);
        mcgw.write(jsSink);
        std::stringstream jsStream;
        jsSink.output(jsStream);

        auto insertableCallgraph = ConstantDataArray::getString(M.getContext(), jsStream.str(), true);
        M.getOrInsertGlobal("CallGraph", insertableCallgraph->getType());
        auto global2 = M.getNamedGlobal("CallGraph");
        global2->setLinkage(llvm::GlobalValue::InternalLinkage);
        global2->setAlignment(MaybeAlign(1));
        global2->setInitializer(insertableCallgraph);
        passToRuntimeComponent(M, global2);

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
                            if (Name == "print<record-analysis>") {
                                MPM.addPass(RecordAnalyzerPrinter(llvm::errs()));
                                return true;
                            }
                            return false;
                        });
                // #2 REGISTRATION FOR "MAM.getResult<RecordAnalyzer>(Module)"
                PB.registerAnalysisRegistrationCallback(
                        [](ModuleAnalysisManager &MAM) {
                            MAM.registerPass([&] { return RecordAnalyzer(); });
                        });
            }};
}

#ifndef LLVM_GENCC_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getGenCCPluginInfo();
}
#endif
