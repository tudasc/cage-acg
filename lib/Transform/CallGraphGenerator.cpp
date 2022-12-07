#include <Transform/CallGraphGenerator.h>
//#include <MetaDataHandler.h>
#include "MetaCGMetadata/VTableMetadata.h"

using namespace llvm;

// static cl::opt<bool> enableGenCC("genCC", cl::init(false),
//                                  cl::desc("generates call-graph component"));

namespace CallGraphGeneration {

    void generateLibraryFunction(Module &M) {
        assert(M.getFunction("getGCC") == nullptr &&
               "could not add getGCC runtime component call");

        FunctionType *getCallGraphFT =
                FunctionType::get(Type::getVoidTy(M.getContext()),
                                  {Type::getInt8PtrTy(M.getContext())}, false);

        M.getOrInsertFunction("getGCC", getCallGraphFT);
    }

    void generateInitFunction(Module &M, size_t id) {

        std::string functionName =
                std::string("genCCInit").append(std::to_string(id));

        assert(M.getFunction(functionName) == nullptr &&
               "genCCInit function already exists");

        FunctionType *InitFT =
                FunctionType::get(Type::getVoidTy(M.getContext()), {}, false);

        M.getOrInsertFunction(functionName, InitFT);
        auto &InitFunctionBBList = M.getFunction(functionName)->getBasicBlockList();
        InitFunctionBBList.insert(InitFunctionBBList.begin(),
                                  BasicBlock::Create(M.getContext()));
        InitFunctionBBList.front().getInstList().insert(
                InitFunctionBBList.front().getInstList().begin(),
                ReturnInst::Create(M.getContext()));
        appendToGlobalCtors(M, M.getFunction(functionName), 101);
    }

    void passToRuntimeComponent(Module &M, Value *Arg, size_t id) {
        std::string functionName =
                std::string("genCCInit").append(std::to_string(id));
        auto &FIlist = M.getFunction(functionName)->front().getInstList();
        auto bitCast = BitCastInst::CreateBitOrPointerCast(
                Arg, Type::getInt8PtrTy(M.getContext()));
        FIlist.insert((--FIlist.end()), bitCast);
        FIlist.insert((--FIlist.end()),
                      CallInst::Create(M.getFunction("getGCC"), bitCast));
    }


    metacg::graph::MCGManager &
    llvmCallGraphToMetaCG(CallGraphAnalysis::Result &llvmCG,
                          const RecordAnalysis::RecordMap &rm) {
        metacg::graph::MCGManager &mcgManager = metacg::graph::MCGManager::get();
        assert(mcgManager.graphs_size() == 0);
        mcgManager.addToManagedGraphs("graph", std::make_unique<metacg::Callgraph>());
        mcgManager.addMetaHandler<GenCCVtableMetadatahandler>();
        for (auto &llvmNode: llvmCG) {
            if (llvmNode.first == NULL) {
                // This is LLVM's way of encoding Reachable Functions, we don't care
                continue;
            }
            if (!llvmNode.first->hasName()) {
                //outs() << "Found function without name!\n";
                continue;
            }

            //outs() << "Adding: " << llvmNode.first->getName() << "\n";
            auto n1 = mcgManager.getCallgraph()->getOrInsertNode(
                    llvmNode.first->getName().str());

            auto &function = llvmNode.first->getFunction();
            n1->setHasBody(function.getInstructionCount() != 0);

            for (auto node: *llvmNode.second) {
                if (!node.first.hasValue()) {
                    outs() << "Function:" << function.getName() << " calls external node\n";
                    continue;
                }

                auto callBase = cast<CallBase>(node.first.getValue());
                auto *calledFunction = callBase->getCalledFunction();

                if (calledFunction != nullptr) {
                    // This is a direct call, we add an edge
                    assert(calledFunction->hasName());
                    //outs() << "Direct call:" << n1->getFunctionName() << "->" << calledFunction->getName() << "\n";

                    auto n2 = mcgManager.getCallgraph()->getOrInsertNode(calledFunction->getName().str());
                    n2->setHasBody(function.getInstructionCount() != 0);

                    if (!mcgManager.getCallgraph()->existEdgeFromTo(n1->getId(), n2->getId())) {
                        mcgManager.getCallgraph()->addEdge(n1, n2);
                    }
                    continue;
                }

                // It was impossible to determine the function,
                // It is a call via pointer
                if (isa<LoadInst>(callBase->getCalledOperand())) {
                    auto loadInst = cast<LoadInst>(callBase->getCalledOperand());
                    handlePointerCallFromLoad(n1, mcgManager.getCallgraph(), callBase, loadInst, rm);

                } else if (isa<BitCastInst>(callBase->getCalledOperand())) {
                    //outs() << "FunctionPointer was generated via bitcast [UNIMPLEMENTED]\n";
                } else if (isa<IntToPtrInst>(callBase->getCalledOperand())) {
                    //outs() << "FunctionPointer was generated via Int to ptr (legacy getelemptr) [UNIMPLEMENTED]\n";
                } else if (isa<PHINode>(callBase->getCalledOperand())) {
                    //outs() << "FunctionPointer was generated via phi node resolve [UNIMPLEMENTED]\n";
                } else {
                    //outs() << "Unknown Instruction:\n";
                    //callBase->getCalledOperand()->dump();
                    //outs() << "Via Call:\n";
                    //callBase->dump();
                    continue;
                }
            }
            // outs() << "Finished creating call Edges!\n";
        }

        llvm::outs() << "Completed insertion of all nodes and edges\n";

        return mcgManager;
    }

    void handlePointerCallFromLoad(metacg::CgNode *sourceNode, metacg::Callgraph *cg, const CallBase *callBase,
                                   const LoadInst *loadInst,
                                   const RecordAnalysis::RecordMap &recordMap) {
        outs() << "Virtual call:" << sourceNode->getFunctionName() << "->";
        loadInst->print(outs());
        outs() << "\n";
        assert(loadInst->getType()->getPointerElementType()->isFunctionTy());
        auto functionType =
                cast<FunctionType>(loadInst->getType()->getPointerElementType());

        if (functionType->getNumParams() < 1) {
            outs() << "Found assumed virtual function without passed object reference\n";
            functionType->dump();
            return;
        }

        assert(functionType->getNumParams() >= 1 &&
               "Call to Virtual Function must at least have *this* as param");
        if (!functionType->getParamType(0)->isPointerTy()) {
            outs() << "Function does not get a pointer as first argument \n Skipping Function [UNIMPLEMENTED]";
            functionType->getParamType(0)->dump();
            return;
        }

        if (!functionType->getParamType(0)->getPointerElementType()->isStructTy()) {
            outs()
                    << "Found call via pointer that was loaded, but is not a virtual call, Skipping Function [UNIMPLEMENTED]\n";
            callBase->dump();
            callBase->getCalledOperand()->dump();
            return;
        }

        assert(functionType->getParamType(0)->getPointerElementType()->isStructTy() &&
               "Pointer to *this* must be struct/class");
        //outs()<<"We are Indexing into:";
        auto indexedStructName = std::string("");
        if (functionType->getParamType(0)->getPointerElementType()->getStructName().startswith(
                RecordAnalysis::StructPrefix)) {
            outs() << "Removing struct prefix\n";
            indexedStructName = RecordAnalysis::removeStructPrefix(functionType->getParamType(0)
                                                                           ->getPointerElementType()
                                                                           ->getStructName()
                                                                           .str());
        } else if (functionType->getParamType(0)->getPointerElementType()->getStructName().startswith(
                RecordAnalysis::ClassPrefix)) {
            outs() << "Removing class prefix\n";
            indexedStructName = RecordAnalysis::removeClassPrefix(functionType->getParamType(0)
                                                                          ->getPointerElementType()
                                                                          ->getStructName()
                                                                          .str());
        } else {
            assert(false && "Indexing into pointer to something that is neither a struct nor a class");
        }


        auto functionPointer = loadInst->getPointerOperand();
        if (isa<GetElementPtrInst>(functionPointer)) {
            outs() << "The pointer operand was calculated via a GEP, Vtable contains more than one function\n";

            auto gep = cast<GetElementPtrInst>(functionPointer);

            if (gep->getNumOperands() != 2) {
                outs() << "Strange GEP maybe nested vtables ?\n";
                gep->dump();
                return;
            }
            assert(loadInst->getType()->isPointerTy());
            assert(isa<ConstantInt>(gep->getOperand(1)));
            long vtableIndex = cast<ConstantInt>(gep->getOperand(1))
                    ->getUniqueInteger()
                    .getSExtValue();
            //outs() << "Creating MN\n";
            {//Remove possible numerical suffixes, which appear when running with opt instead of lld
                int count = 0;
                for (auto byte = std::prev(llvm::StringRef(indexedStructName).bytes().end());
                     (*byte == '.' || (*byte >= '0' && *byte <= '9')) &&
                     byte != llvm::StringRef(indexedStructName).bytes_begin(); byte--, count++) {}
                indexedStructName = llvm::StringRef(indexedStructName).drop_back(count);
            }

            outs() << "Struct: " << indexedStructName << " will call the: " << vtableIndex << "th function of "
                   << recordMap.at(indexedStructName)->callSet.size() << " different vtables \n";

            CgNodeRawPtrUSet callSet;
            for (const auto &elem: recordMap.at(indexedStructName)->callSet) {
                outs() << elem->name << "::" << elem->vtable.functions.at(vtableIndex)->getName() << "\n";
                auto targetNode = cg->getOrInsertNode(elem->vtable.functions.at(vtableIndex)->getName().str());
                callSet.insert(targetNode);
                cg->addEdge(sourceNode->getId(), targetNode->getId());
            }
            sourceNode->getOrCreateMD<GenCCVtableMetadata>()->addToCallSet(callSet);

        } else {

            outs() << "Vtable contains only one function we load index 0\n";
            outs() << "Creating Trivial MN\n";

            outs() << "Struct: " << indexedStructName << " will call the: " << 0 << "th function of "
                   << recordMap.at(indexedStructName)->callSet.size() << " different vtables \n";

            CgNodeRawPtrUSet callSet;
            for (const auto &elem: recordMap.at(indexedStructName)->callSet) {
                outs() << elem->name << "::" << elem->vtable.functions.at(0)->getName() << "\n";
                auto targetNode = cg->getNode(elem->vtable.functions.at(0)->getName().str());
                callSet.insert(targetNode);
                cg->addEdge(sourceNode->getId(), targetNode->getId());

            }
            sourceNode->getOrCreateMD<GenCCVtableMetadata>()->addToCallSet(callSet);
        }
    }

    bool work(Module &M, ModuleAnalysisManager *MA) {
        generateLibraryFunction(M);

        size_t t = std::hash<std::string>()(getUniqueModuleId(&M));

        outs() << "T:" << getUniqueModuleId(&M) << ":" << t << "\n";

        generateInitFunction(M, t);
        /** Use callgraph information provided by CGA Pass
         */
        auto &cgResult = MA->getResult<CallGraphAnalysis>(M);
        // cgResult.print(outs());

        std::string callGraph2;
        llvm::raw_string_ostream llvmso(callGraph2);

        cgResult.print(llvmso);

        auto anaRes = MA->getResult<RecordAnalysis::RecordAnalyzer>(M);


        auto &mcg = llvmCallGraphToMetaCG(cgResult, anaRes);
        // printRecordAnalyzerResults(outs(), anaRes);

        metacg::io::JsonSink jsSink;
        // TODO: M.getName does not return a "nice" name during LTO, maybe allow for
        //  passing a parameter to name the control flow graph
        std::string s("GenCC");
        metacg::MCGGeneratorVersionInfo mcgVI = {s, 0, 1, "NO_GIT_SHA_AVAILABLE"};
        metacg::io::MCGWriter mcgw(mcg, genCCInfo(M.getName()));

        mcgw.write(jsSink);
        std::stringstream jsStream;
        jsSink.output(jsStream);

        outs() << "Callgraph:\n";
        outs() << jsSink.getJson().dump(2, ' ').c_str() << "\n";
        outs() << "-----------------------------\n";
        auto insertableCallgraph =
                ConstantDataArray::getString(M.getContext(), jsStream.str(), true);

        std::string globalName = std::string("CallGraph").append(std::to_string(t));

        M.getOrInsertGlobal(globalName, insertableCallgraph->getType());
        auto global2 = M.getNamedGlobal(globalName);
        global2->setLinkage(llvm::GlobalValue::InternalLinkage);
        global2->setAlignment(MaybeAlign(1));
        global2->setInitializer(insertableCallgraph);
        passToRuntimeComponent(M, global2, t);


        outs() << "Finishing up \n";
        mcg.resetManager();
        outs() << "Finished up\n";
        return false;
    }

    metacg::MCGFileInfo genCCInfo(StringRef ModuleName) {
        metacg::MCGFileFormatInfo ffi(3, 0);
        ffi.cgFieldName = "_CG";
        ffi.metaInfoFieldName = "_MetaCG";
        std::string name = "GenCC";
        metacg::MCGGeneratorVersionInfo gvi = {name, 0, 1, "NO_GIT_SHA_AVAILABLE"};
        return {ffi, gvi};
    }

} // namespace CallGraphGeneration
