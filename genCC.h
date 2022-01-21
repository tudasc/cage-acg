#ifndef LLVM_TRANSFORMS_HELLONEW_HELLOWORLD_H
#define LLVM_TRANSFORMS_HELLONEW_HELLOWORLD_H

#include "llvm/IR/PassManager.h"

namespace llvm {

    class HelloWorldPass2 : public PassInfoMixin<HelloWorldPass2> {
    public:
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    };

} // namespace llvm

#endif // LLVM_TRANSFORMS_HELLONEW_HELLOWORLD_H