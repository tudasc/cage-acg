#ifndef CAGE_GENERATOR_HXX
#define CAGE_GENERATOR_HXX

#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

namespace cage {
bool work(llvm::Module&, llvm::ModuleAnalysisManager*);

struct cage : llvm::PassInfoMixin<cage> {
  llvm::PreservedAnalyses run(llvm::Module& m, llvm::ModuleAnalysisManager& ma) {
    if (!work(m, &ma))
      return llvm::PreservedAnalyses::all();

    return llvm::PreservedAnalyses::none();
  }

  static llvm::StringRef name() {
    return "CaGe";
  }
};
}  // namespace cage

#endif  // CAGE_GENERATOR_HXX
