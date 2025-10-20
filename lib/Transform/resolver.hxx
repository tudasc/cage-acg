#ifndef CAGE_RESOLVER_HXX
#define CAGE_RESOLVER_HXX

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Module.h>

#include <Callgraph.h>
#include <metavirt/VirtCall.h>

#include <unordered_map>

namespace cage
{
  struct resolver
  {
    explicit
    resolver (llvm::Module const& m, metacg::Callgraph* mcg) : mcg { mcg }
    {
      llvm::DebugInfoFinder dbg_finder {};
      dbg_finder.processModule (m);

      has_md = dbg_finder.subprogram_count () != 0;

      for (auto const& sub: dbg_finder.subprograms ())
        if (auto const* f = m.getFunction (sub->getName ()); f)
          fn_map[f] = sub;
        else if (auto const lf = m.getFunction (sub->getLinkageName ()); lf)
          fn_map[lf] = sub;

      for (auto const& f: m.getFunctionList ())
        sig_map[f.getFunctionType ()].push_back (&f.getFunction ());
    }

    llvm::SmallVector<metacg::CgNode*, 4>
    potential_targets (llvm::CallBase const& call) const
    {
      llvm::SmallVector<metacg::CgNode*, 4> r {};

      // If we're looking at a direct call, just return the function directly, supported here for convenience
      if (auto const f = call.getCalledFunction (); f)
      {
        metacg::CgNode* child = &mcg->getOrInsertNode (f->getName ().str ());
        child->setHasBody (f->getInstructionCount() != 0);
        r.push_back (child);
        return r;
      }

      // Attempt to resolve the call base as a virtual call
      if (auto const vcall = metavirt::vcall_data_for (&call); vcall && !vcall->call_targets.empty ())
      {
        for (auto const& [name, origin]: fn_names_and_origins (*vcall))
          r.push_back (&mcg->getOrInsertNode (name.str (), origin.str ()));

        return r;
      }

      // Resolve all other function pointers via function type approximation
      for (auto const& possible = sig_map.at (call.getFunctionType ()); auto const& f: possible)
      {
        metacg::CgNode* child;
        if (has_md && is_contained (fn_map, f))
          child = &mcg->getOrInsertNode (fn_map.at (f)->getLinkageName ().str (), fn_map.at (f)->getFilename ().str ());
        else
          child = &mcg->getOrInsertNode (f->getName ().str ());

        child->setHasBody (f->getInstructionCount() != 0);
        r.push_back (child);
      }

      return r;
    }

    const auto&
    map () const { return fn_map; }

  private:
    bool has_md;
    metacg::Callgraph* mcg {};
    std::unordered_map<llvm::Function const*, llvm::DISubprogram const*> fn_map {};
    std::unordered_map<llvm::FunctionType*, std::vector<llvm::Function const*>> sig_map {};
  };
} // namespace cage

#endif // CAGE_RESOLVER_HXX
