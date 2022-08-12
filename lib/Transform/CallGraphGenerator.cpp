#include <Transform/CallGraphGenerator.h>

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
  mcgManager.addToManagedGraphs("graph", std::make_unique<metacg::Callgraph>());

  for (auto &llvmNode : llvmCG) {
    if (llvmNode.first == NULL) {
      // This is LLVM's way of encoding Reachable Functions, we dont care
      continue;
    }
    if (!llvmNode.first->hasName()) {
      // outs() << "Found function without name!\n";
      continue;
    }

    auto n1 = mcgManager.getCallgraph()->getOrInsertNode(
        llvmNode.first->getName().str());
    auto &function = llvmNode.first->getFunction();

    n1->setFilename(function.getParent()->getSourceFileName());
    n1->setHasBody(function.getInstructionCount() != 0);

    for (auto node : *llvmNode.second) {
      if (!node.first.hasValue()) {
        // outs() << "Function:" << n1->getFunctionName() << " calls external
        // node\n";
        continue;
      }
      assert(isa<CallBase>(node.first.getValue()));
      auto callBase = cast<CallBase>(node.first.getValue());
      auto *calledFunction = callBase->getCalledFunction();
      if (calledFunction != nullptr) {
        // This is a direct call, we add an edge
        assert(calledFunction->hasName());
        // is possible insertion really necessary?
        CgNodePtr n2 = mcgManager.getCallgraph()->getOrInsertNode(
            calledFunction->getName().str());
        n2->setFilename(function.getParent()->getSourceFileName());
        n2->setHasBody(function.getInstructionCount() != 0);
        mcgManager.getCallgraph()->addEdge(n1, n2);
        continue;
      }
       // It was impossible to determine the function,
      // It is a call via pointer

      if (isa<LoadInst>(callBase->getCalledOperand())) {
        auto loadInst = cast<LoadInst>(callBase->getCalledOperand());
        //loadInst->dump();
        assert(loadInst->getType()->getPointerElementType()->isFunctionTy());
        //outs()<<__LINE__<<"\n";
        auto functionType =
            cast<FunctionType>(loadInst->getType()->getPointerElementType());
        //outs()<<__LINE__<<"\n";
        assert(functionType->getNumParams() >= 1 &&
               "Call to Virtual Function must at least have *this* as param");
        //outs()<<__LINE__<<"\n";

        if(!functionType->getParamType(0)->isPointerTy()){
          //outs()<<"Function does not get a pointer as first argument \n Skipping Function [UNIMPLEMENTED]";
          //functionType->getParamType(0)->dump();
          continue;
        }

        if(!functionType->getParamType(0)->getPointerElementType()->isStructTy()){
          //outs()<<"Found call via pointer that was loaded, but is not a virtual call, Skipping Function [UNIMPLEMENTED]\n";
          //callBase->dump();
          //callBase->getCalledOperand()->dump();
          continue;
        }
        //outs()<<__LINE__<<"\n";

        assert(functionType->getParamType(0)
                   ->getPointerElementType()
                   ->isStructTy() &&
               "Pointer to *this* must be struct/class");
        // outs()<<"We are Indexing into:";
        auto indexedStructName =
            RecordAnalysis::removeStructPrefix(functionType->getParamType(0)
                                                   ->getPointerElementType()
                                                   ->getStructName()
                                                   .str());
        auto functionPointer = loadInst->getPointerOperand();
        if (isa<GetElementPtrInst>(functionPointer)) {
          // outs()<<"The pointer operand was calculated via a GEP, Vtable
          // contains more then one function\n";

          auto gep = cast<GetElementPtrInst>(functionPointer);

          if(gep->getNumOperands() != 2){
            outs()<<"Strange GEP\n";
            continue;
          }

          assert(gep->getNumOperands() == 2);
          assert(loadInst->getType()->isPointerTy());
          assert(isa<ConstantInt>(gep->getOperand(1)));
          long vtableIndex = cast<ConstantInt>(gep->getOperand(1))
                                 ->getUniqueInteger()
                                 .getSExtValue();
          //outs() << "Creating MN\n";
          CgMultiNodePtr cgmnp =
              createMultiNode(rm, indexedStructName, vtableIndex);
          //outs() << "MultiNode:\n";
          //std::cout << cgmnp << "\n";
          mcgManager.getCallgraph()->insertMultiNode(cgmnp);
          // outs()<<"------------\n";

        } else {
          // outs() << "Vtable contains only one function we load index 0\n";
          //outs() << "Creating Trivial MN\n";
          CgMultiNodePtr cgmnp = createMultiNode(rm, indexedStructName, 0);
          //outs() << "MultiNode:\n";
          //std::cout << cgmnp << "\n";
          mcgManager.getCallgraph()->insertMultiNode(cgmnp);
        }
      } else if (isa<BitCastInst>(callBase->getCalledOperand())) {
        //outs() << "FunctionPointer was generated via bitcast [UNIMPLEMENTED]\n";
      } else if (isa<IntToPtrInst>(callBase->getCalledOperand())) {
        //outs() << "FunctionPointer was generated via Int to ptr (legacy "
                  "getelemptr) [UNIMPLEMENTED]\n";
      } else if (isa<PHINode>(callBase->getCalledOperand())) {
        //outs() << "FunctionPointer was generated via phi node resolve "
                  "[UNIMPLEMENTED]\n";
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

CgMultiNodePtr createMultiNode(RecordAnalysis::RecordMap rm,
                               const std::string &indexedStructName,
                               long vtableIndex) {
  outs() << "Searching:" << indexedStructName << "\n";
  std::shared_ptr<RecordAnalysis::RecordInformation> recordInformation;
  if (rm.count(indexedStructName) != 0) {
    outs() << "Done\n";
    recordInformation = rm.at(indexedStructName);

    CgMultiNode cnp;
    for (const auto &possibleStruct : recordInformation->parents) {
      if(rm.count(possibleStruct->name)==0){
        outs()<<"Struct "<<possibleStruct->name<<" was not in analyzed set\n";
        break;
      }
      auto baseFunction = rm.at(possibleStruct->name);
      auto virtualFunction = baseFunction->vtable.functions[vtableIndex];
      cnp.push_back(
          std::make_shared<metacg::CgNode>(virtualFunction->getName().str()));
      cnp.back()->setFilename(
          virtualFunction->getParent()->getSourceFileName());
      cnp.back()->setHasBody(virtualFunction->getInstructionCount() != 0);
    }
    return std::make_shared<CgMultiNode>(cnp);
  } else {
    outs() << "Could not find\n";
    CgMultiNode cnp;
    cnp.push_back(std::make_shared<metacg::CgNode>("CouldNotFindNode"));
    return std::make_shared<CgMultiNode>(cnp);
  }
}

bool work(Module &M, ModuleAnalysisManager *MA) {
  generateLibraryFunction(M);

  size_t t = std::hash<std::string>()(getUniqueModuleId(&M));

  outs() << "T:" << getUniqueModuleId(&M) << ":" << t << "\n";

  generateInitFunction(M, t);

  /** Use callgraph information provided by CGA Pass
   *  and dump that to module as well
   */
  auto &cgResult = MA->getResult<CallGraphAnalysis>(M);
  // cgResult.print(outs());

  std::string callGraph2;
  llvm::raw_string_ostream llvmso(callGraph2);

  cgResult.print(llvmso);

  auto anaRes = MA->getResult<RecordAnalysis::RecordAnalyzer>(M);

  auto &mcg = llvmCallGraphToMetaCG(cgResult, anaRes);

  for (const auto &elem : *mcg.getCallgraph()) {
    std::cout << elem << "\n";
  }

  // printRecordAnalyzerResults(outs(), anaRes);

  metacg::io::JsonSink jsSink;
  // TODO: M.getName does not return a "nice" name during LTO, maybe allow for
  // passing a parameter to name the control flow graph
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
