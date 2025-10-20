#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>

#include <cage/generator.hxx>

llvm::PassPluginLibraryInfo
plugin_info ()
{
  return {
    LLVM_PLUGIN_API_VERSION,
    "CaGe",
    "0.3",
    [] (llvm::PassBuilder& b)
    {
      // Allow registration via optlevel (non-lto)
      b.registerOptimizerLastEPCallback ([] (llvm::PassManager<llvm::Module>& pm, llvm::OptimizationLevel, llvm::ThinOrFullLTOPhase)
        {
          llvm::outs () << "Registering CaGe to run during opt\n";
          pm.addPass (cage::cage {});
        });

      // Registering via optlevel during lto appears to still be broken
      b.registerFullLinkTimeOptimizationLastEPCallback ([] (llvm::ModulePassManager& pm, llvm::OptimizationLevel)
        {
          llvm::outs () << "Registering CaGe to run in during full-lto\n";
          pm.addPass(cage::cage {});
        });

      // Allow registration via pipeline parser
      b.registerPipelineParsingCallback ([](llvm::StringRef const name,
                                            llvm::ModulePassManager& pm,
                                            llvm::ArrayRef<llvm::PassBuilder::PipelineElement>)
        {
          if (name == "CaGe")
          {
            llvm::outs () << "Registering CaGe to run as pipeline described\n";
            pm.addPass(cage::cage {});
            return true;
          }

          llvm::outs () << "Did not register CaGe\n";
          return false;
        });
    }
  };
}

#ifndef LLVM_GENCC_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo ()
{
#ifndef NDEBUG
  llvm::outs () << "Loading debug version of CaGe...\n";
#else
  llvm::outs () <<"Loading release version of CaGe...\n";
#endif

  return plugin_info ();
}
#endif
