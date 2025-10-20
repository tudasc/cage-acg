#ifndef CAGE_OMP_HXX
#define CAGE_OMP_HXX

#include <llvm/Analysis/CallGraph.h>

#include <Callgraph.h>

#include <optional>
#include <cstdlib>
#include <cstring>

namespace cage
{
  enum struct omp_mode
  {
    merge,
    transparent,
    split,
  };

  inline std::optional<omp_mode>
  parse_omp_mode ()
  {
    auto const* val = getenv ("CAGE_OMP_MODE");
    if (!val)
      return {};

    if (!strcmp (val, "merge"))
      return omp_mode::merge;
    if (!strcmp (val, "split"))
      return omp_mode::split;
    if (!strcmp (val, "transparent"))
      return omp_mode::transparent;

    return {};
  }

  struct omp
  {
    omp (metacg::Callgraph* mcg, llvm::CallGraph const* lcg)
      : mode { parse_omp_mode () },
        mcg { mcg },
        lcg { lcg }
    {}

    void
    add (llvm::CallBase const& call) const
    {
      auto const* parent = &mcg->getOrInsertNode (call.getParent ()->getParent ()->getName ().str ());
      auto const* kmpc_fork = call.getCalledFunction ();

      metacg::CgNode const* child{};
      if (mode == omp_mode::merge)
        return add_merged (parent, call);

      if (mode == omp_mode::split)
      {
        child = &mcg->getOrInsertNode (kmpc_fork->getName ().str ());
        mcg->addEdge (*parent, *child);
        parent = child;
      }

      if (auto const* region = dyn_cast<llvm::Function> (call.getArgOperand (2)); region)
        child = &mcg->getOrInsertNode (region->getName ().str ());

      mcg->addEdge (*parent, *child);
    }

    [[nodiscard]] auto
    has_enabled_mode (omp_mode const m) const { return mode.has_value () && *mode == m; }

    [[nodiscard]] auto
    has_not_enabled_mode (omp_mode const m) const { return mode.has_value () && *mode != m; }

    [[nodiscard]] auto
    enabled () const { return mode.has_value (); }

  private:
    void
    add_merged (metacg::CgNode const* parent, llvm::CallBase const& call) const
    {
      for (auto const& [key, elem] : *lcg->operator[] (dyn_cast<llvm::Function> (call.getArgOperand (2))))
      {
        if (!key.has_value ())
          continue;
        if (!elem->getFunction ())
          continue;
        if (elem->getFunction ()->isIntrinsic ())
          continue;

        auto const* child = elem->getFunction ();
        assert (child->hasName ());

        auto const* node = &mcg->getOrInsertNode (child->getName ().str ());
        mcg->addEdge (*parent, *node);
      }
    }

    std::optional<omp_mode> mode;
    metacg::Callgraph* mcg;
    llvm::CallGraph const* lcg;
  };
}

#endif // CAGE_OMP_HXX
