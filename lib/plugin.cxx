#include <cage/Logger.h>
#include <cage/generator.hxx>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

llvm::PassPluginLibraryInfo plugin_info() {
  return {LLVM_PLUGIN_API_VERSION, "CaGe", "0.3", [](llvm::PassBuilder& b) {
            // Allow registration via optlevel (non-lto)
            b.registerOptimizerLastEPCallback(
                [](llvm::PassManager<llvm::Module>& pm, llvm::OptimizationLevel, llvm::ThinOrFullLTOPhase) {
                  LOG_DEBUG("Registering CaGe to run during opt");
                  pm.addPass(cage::cage{});
                });

            // Registering via optlevel during lto appears to still be broken
            b.registerFullLinkTimeOptimizationLastEPCallback([](llvm::ModulePassManager& pm, llvm::OptimizationLevel) {
              LOG_DEBUG("Registering CaGe to run in during full-lto");
              pm.addPass(cage::cage{});
            });

            // Allow registration via pipeline parser
            b.registerPipelineParsingCallback([](llvm::StringRef const name, llvm::ModulePassManager& pm,
                                                 llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
              if (name == "CaGe") {
                LOG_DEBUG("Registering CaGe to run as pipeline described");
                pm.addPass(cage::cage{});
                return true;
              }

              LOG_DEBUG("Did not register CaGe");
              return false;
            });
          }};
}

#ifndef LLVM_GENCC_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
#ifndef NDEBUG
  LOG_DEBUG("Loading debug version of CaGe...");
#else
  LOG_DEBUG("Loading release version of CaGe...");
#endif

  return plugin_info();
}
#endif
