#include <Transform/GenCC.h>

using namespace llvm;

//static cl::opt<bool> enableGenCC("genCC", cl::init(false),
//                                 cl::desc("generates call-graph component"));

namespace GenCC {

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

    metacg::graph::MCGManager& llvmCallGraphToMetaCG(CallGraphAnalysis::Result &llvmCG, RecordAnalysis::RecordMap rm) {

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
                                    //rm.at(llvmNode.first->getName().str())->vtable.functions[0];
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


        auto anaRes = MA->getResult<RecordAnalysis::RecordAnalyzer>(M);
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

} // namespace


