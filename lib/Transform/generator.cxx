#include <cage/generator.hxx>

#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/InstVisitor.h>

#include <io/VersionFourMCGWriter.h>
#include <MCGManager.h>

#include <cxxabi.h>
#include <fstream>

#include "dataflow.hxx"
#include "resolver.hxx"
#include "omp.hxx"

namespace cage
{
  template <typename T, typename R>
  auto
  collect (R&& range)
  {
    T container {};

    for (auto&& it: std::forward<R> (range))
      container.push_back (std::forward<decltype (it)> (it));

    return container;
  }

  struct call_base_visitor: llvm::InstVisitor<call_base_visitor>
  {
    explicit
    call_base_visitor (llvm::CallGraph const* lcg)
      : mcg {metacg::graph::MCGManager::get ().getOrCreateCallgraph ("LTOGraph", true) },
        lcg { lcg },
        omp_handler { mcg, lcg },
        resolv { lcg->getModule (), mcg }
    {}

    void
    visitCallBase (llvm::CallBase const& call) const
    {
      // Skip calls to intrinsics
      if (call.getCalledFunction () && call.getCalledFunction ()->isIntrinsic ())
        return;

      auto const* current = mcg->getFirstNode (call.getParent ()->getParent ()->getName ().str ());

      if (!call.getCalledFunction ())
      {
        // If we're looking at an indirect call, add edges to all potential call targets we can resolve
        for (auto const possible = resolv.potential_targets (call, call.getFunction ()->getName ());
             auto const* target: possible)
        {
          llvm::outs () << "[DBG] -> target = " << target->getFunctionName () << '\n';
          mcg->addEdge (*current, *target);
        }
      }
      else if (call.getCalledFunction ()->getName () == "__kmpc_fork_call" && omp_handler.enabled ())
        omp_handler.add (call);
    }

    void
    visitFunction (llvm::Function const& f) const
    {
      if (f.isIntrinsic ())
        return;

      auto const name = f.getName ();
      llvm::outs() << "Visiting function: " << name << '\n';

      // Skip certain OpenMP related functions depending on the selected OpenMP mode
      if (name == "__kmpc_fork_call" && omp_handler.has_not_enabled_mode (omp_mode::split))
        return;
      if (name.contains ("omp_outlined") && omp_handler.has_enabled_mode (omp_mode::merge))
        return;

      // Create a node for the current function
      metacg::CgNode* current;
      if (resolv.map ().contains (&f))
      {
        auto const& origin = resolv.map ().at (&f)->getFilename ().str ();
        current = &mcg->getOrInsertNode (name.str (), origin);
      } else
        current = &mcg->getOrInsertNode (name.str ());

      current->setHasBody (f.getInstructionCount() != 0);

      handle_args (f, current);
      handle_locals (f, current);

      // Process all callees of the current function
      for (auto const& [key, elem]: *lcg->operator[] (&f))
      {
        if (!key.has_value ())
          continue;
        if (!elem->getFunction ())
          continue;
        if (elem->getFunction ()->isIntrinsic ())
          continue;
        if (omp_handler.enabled () && elem->getFunction ()->getName () == "__kmpc_fork_call")
          continue;

        auto const* child_f = elem->getFunction ();
        assert (child_f->hasName () && "Callee has no name");

        // Create a node for the callee
        metacg::CgNode* child;
        if (resolv.map ().contains (child_f))
        {
          auto const& origin = resolv.map ().at (child_f)->getFilename ().str ();
          child = &mcg->getOrInsertNode (child_f->getName ().str (), origin);
        } else
          child = &mcg->getOrInsertNode (child_f->getName ().str ());

        child->setHasBody (f.getInstructionCount() != 0);

        if (!mcg->existsEdge (current->getId (), child->getId ()))
          mcg->addEdge (*current, *child);
      }
    }

  private:
    void
    handle_args (llvm::Function const& f, metacg::CgNode* node) const
    {
      llvm::outs () << "> processing arguments...\n";

      auto const published = published_values (
        map_range (f.args (), [&f] (auto const& arg)
        {
          return std::make_pair (&arg, &arg - f.arg_begin ());
        }),
        resolv,
        false
      );
      auto const args = make_filter_range (published, [] (auto const& it) { return it.has_value (); });

      auto md = std::make_unique<mcg::md_arg_flow> ();
      for (auto const& arg: args)
      {
        auto const outs = collect<std::vector<mcg::md_arg_output>> (map_range (arg->uses, [] (use_in_call const& use)
          {
            auto const callees = map_range (use.callees, [] (metacg::CgNode const* c) { return c->getId (); });

            return mcg::md_arg_output {
              .idx = use.idx,
              .callees = collect<std::vector<size_t>> (callees),
              .by_ref = use.by_ref,
              .loc = use.loc,
            };
          }));

        md->arg (mcg::md_arg { .idx = arg->idx, .outs = outs, });
      }

      if (!md->args.empty ())
        node->addMetaData (std::move (md));
    }

    void
    handle_locals (llvm::Function const& f, metacg::CgNode* node) const
    {
      llvm::outs () << "> processing locals...\n";

      auto const insts = instructions (f);
      auto const published = published_values (
        map_range (
          make_filter_range (insts, [] (auto const& inst)
          {
            if (!isa<llvm::AllocaInst> (&inst))
              return false;

            // Ignore allocas that are just stack copies of function parameters
            for (auto const* alloca = cast<llvm::AllocaInst> (&inst); auto const* user: alloca->users ())
              if (auto const* instr = dyn_cast<llvm::Instruction> (user); instr)
              {
                switch (instr->getOpcode ())
                {
                case llvm::Instruction::Store:
                  if (isa<llvm::Argument> (instr->getOperand (0)))
                    return false;
                    break;

                  default:
                    break;
                }
              }

            return true;
          }),
          // Map this to a dummy pair to keep the `published_values` interface consistent
          [] (auto const& inst) { return std::make_pair (&inst, 0); }
        ),
        resolv,
        true
      );

      // Collect local variables that escape the current function
      auto const locals = make_filter_range (published, [] (auto const& it) { return it.has_value (); });

      for (auto const& local: locals)
        if (auto const intrin = di::find_intrinsic (dyn_cast<llvm::Instruction> (local->val)); intrin)
        {
          if ((*intrin)->getVariable ())
            llvm::outs () << "-> local [" << (*intrin)->getVariable ()->getName () << "] is published.\n";
        }
  
      auto md = std::make_unique<mcg::md_locals> ();
      for (auto const& local: locals)
        for (auto const& use: local->uses)
        {
          auto const callees = map_range (use.callees, [] (metacg::CgNode const* c) { return c->getId (); });
          md->local (mcg::md_local { .callees = collect<std::vector<size_t>> (callees), .loc = use.loc, });
        }

      if (!md->locals.empty ())
        node->addMetaData (std::move (md));
    }

    metacg::Callgraph* mcg;
    llvm::CallGraph const* lcg;
    omp omp_handler;
    resolver resolv;
  };

  bool
  work (llvm::Module& m, llvm::ModuleAnalysisManager* ma)
  {
    auto const& lcg = ma->getResult<llvm::CallGraphAnalysis> (m);
    call_base_visitor visitor { &lcg };

    visitor.visit (m);

    // Serialize the generated callgraph to disk
    metacg::io::JsonSink sink {};
    metacg::io::VersionFourMCGWriter writer { { { 4, 0 }, { "GenCC", 0, 1, "NO_GIT_SHA_AVAILABLE" } }, false, true };
    writer.write (metacg::graph::MCGManager::get ().getCallgraph (), sink);

    char const* name = std::getenv ("GENCC_CG_NAME");
    std::ofstream file {name ? name : "LTO_callgraph.mcg"};
    file << sink.getJson ().dump (4);
    file.flush ();

    return false;
  }
} // namespace cage
