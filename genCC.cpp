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

using namespace llvm;

static cl::opt<bool> enableGenCC("genCC", cl::init(false),
                                 cl::desc("generates call-graph component"));

namespace genCC {

    void generateLibraryFunction(Module &M, GlobalVariable *gv) {
        FunctionType *getCallGraph = FunctionType::get(Type::getVoidTy(M.getContext()),
                                                       {gv->getType()},
                                                       false);

        auto f = M.getOrInsertFunction("getGCC", getCallGraph);

    }

    bool work(Module &M, ModuleAnalysisManager *MA) {

        //errs() << "Running with new pass manager \n";
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



            //Fixme:dont insert into main, but insert into function with __attribute__((constructor)) to handle shared libraries
            generateLibraryFunction(M, global1);
            auto &FIlist = M.getFunction("main")->front().getInstList();
            FIlist.insert(FIlist.begin(), CallInst::Create(M.getFunction("getGCC"), {global1}));
        }

        /** Use callgraph information provided by CGA Pass
         *  and dump that to module aswell
         */
        {
            auto &cgResult = MA->getResult<CallGraphAnalysis>(M);
            cgResult.print(outs());


            std::string callGraph2;
            llvm::raw_string_ostream llvmso(callGraph2);

            cgResult.print(llvmso);
            llvm::outs() << "end of cgResult\n";


            metacg::Callgraph mcgCallGraph;
            llvm::outs() << "generated call graph object\n";
            for (auto &llvmNode: cgResult) {
                if (llvmNode.first == NULL) {
                    outs() << "Null?\n";
                } else {

                    if (llvmNode.first->hasName()) {
                        llvm::outs() << "Inserting: " << llvmNode.first->getName() << "\n";
                        metacg::CgNode cgn(llvmNode.first->getName().str());
                        outs()<<"Created node\n";
                        //mcgCallGraph.insert(std::make_shared<metacg::CgNode>(cgn));
                    } else {
                        llvm::outs() << "Found function without name!\n";
                    }
                }
            }

            llvm::outs() << "Completed insertion of all nodes\n";
            //maybe steal phasar aproach
            //maybe use clang to pre generate virtual metadata in more readable format
            //focus on more common cases and stay in llvm ir

/*
        SmallVector<std::pair<unsigned, MDNode *>> sv= SmallVector<std::pair<unsigned, MDNode *>>();
        M.getNamedGlobal("_ZTV7Derived")->getAllMetadata(sv);

        for(auto elem: sv){
            elem.second->print(outs());
            outs()<<"\n";
        }
        outs()<<"\n";

        outs().flush();
*/

            auto insertableCallgraph2 = ConstantDataArray::getString(M.getContext(), callGraph2, true);
            M.getOrInsertGlobal("CallGraph2", insertableCallgraph2->getType());
            auto global2 = M.getNamedGlobal("CallGraph2");
            global2->setLinkage(llvm::GlobalValue::ExternalLinkage);
            global2->setAlignment(MaybeAlign(1));
            global2->setInitializer(insertableCallgraph2);
        }

        //Format of global information is not relevant, as long as its serializable and deserializable
        //Need graph structur in runtime component
        //Keep simple, edges nodes,
        //Must be mergeable
        //Representation must be serializable to MetaCG

        for (auto &g: M.globals()) {
            if (g.hasName() && llvm::StringRef(demangle(g.getName().str())).startswith("vtable")) {
                outs() << "The Vtable for: " << g.getName() << " (" << demangle(g.getName().str()) << ")\n";
                outs() << "Contains:\n";
                SmallVector<std::pair<unsigned int, MDNode *>> allMetadata = {};
                g.getAllMetadata(allMetadata);
                for (auto m: allMetadata) {
                    std::string metadata;
                    llvm::raw_string_ostream llvmso(metadata);
                    m.second->getOperand(1)->print(llvmso);
                    outs() << demangle(StringRef(metadata).substr(2, metadata.size() - 3).str()) << "\n";
                }

                /*if(g.getType()->isStructTy()){
                    outs()<<"was struct\n";
                }
                if(g.getType()->isPointerTy()){
                    cast<PointerType>(g.getType())->getElementType()->print(outs());
                    outs()<<"\n";
                }*/
            }
        }


        return false;
    }

    struct LegacyGenCC : public ModulePass {
        static char ID;

        LegacyGenCC() : ModulePass(ID) {}

        bool runOnModule(Module &M) override { return work(M, nullptr); }
    };

    struct genCC : PassInfoMixin<genCC> {
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MA) {
            if (!work(M, &MA))
                return PreservedAnalyses::all();
            return PreservedAnalyses::none();
        }
    };

} // namespace

char genCC::LegacyGenCC::ID = 0;

static RegisterPass<genCC::LegacyGenCC> X("legacy-genCC", "generates Call Grap Components (legacy)",
                                          false /* Only looks at CFG */,
                                          false /* Analysis Pass */);

/* Legacy PM Registration */
static llvm::RegisterStandardPasses RegisterGenCC(
        llvm::PassManagerBuilder::EP_OptimizerLast,
        [](const llvm::PassManagerBuilder &Builder,
           llvm::legacy::PassManagerBase &PM) { PM.add(new genCC::LegacyGenCC()); }
);

/* New PM Registration */
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
            }};
}

#ifndef LLVM_GENCC_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getGenCCPluginInfo();
}
#endif
