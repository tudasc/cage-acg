#ifndef CAGE_DATAFLOW_HXX
#define CAGE_DATAFLOW_HXX

#include <llvm/IR/InstIterator.h>

#include "resolver.hxx"
#include "difinder.hxx"
#include "metacg.hxx"

namespace cage
{
  struct use_in_call
  {
    llvm::SmallVector<metacg::CgNode*, 4> callees {};
    size_t idx {};
    bool by_ref {};
    mcg::src_loc loc {};
  };

  struct published_value
  {
    size_t idx {};
    llvm::Value const* val {};
    llvm::SmallVector<use_in_call> uses {};
  };

  struct workq_item { llvm::Value const* last {}, *current {}; };

  template <typename R>
  auto
  published_values (R&& inputs, resolver const& resolver, bool is_local_flow)
  {
    llvm::outs () << "> Published value called\n";
    for (auto const& it: inputs)
      llvm::outs () << "-> element: [" << *std::get<0> (it) << "]\n";

    return map_range (inputs, [&resolver, is_local_flow] (std::pair<llvm::Value const*, size_t> input)
    {
      llvm::outs () << "-> input is [" << *std::get<0> (input) << "].\n";

      llvm::SmallVector<workq_item, 64> workq { workq_item { .last = nullptr, .current = std::get<0> (input) } };
      llvm::SmallPtrSet<llvm::Value const*, 32> seen {};

      std::optional<published_value> var {};

      auto const enqueue = [&workq, &seen] (llvm::Value const* last, llvm::Value const* val)
      {
        if (auto const [_, inserted] = seen.insert (val); inserted)
        {
          llvm::outs () << "!--> enqueueing [" << *val << "]...\n";
          workq.emplace_back (last, val);
        }
      };

      while (!workq.empty ())
      {
        auto const [last, current] = workq.pop_back_val ();

        if (last)
          llvm::outs () << "--> last is [" << *last << "].\n";
        llvm::outs () << "--> current is [" << *current << "].\n";

        // If we reach a call, try to resolve the call to the set of potential callees and move on to the next
        // item in the work queue.
        if (auto const* inst = dyn_cast<llvm::Instruction> (current);
            inst && inst->getOpcode () == llvm::Instruction::Call)
        {
          auto const& call = *cast<llvm::CallBase> (inst);
          auto const* loc = di::location (inst);

          if (is_local_flow && call.getCalledFunction ())
            continue;

          if (!var.has_value ())
            var.emplace (std::get<1> (input), std::get<0> (input));

          llvm::outs () << "--> input index is [" << var->idx << "].\n";

          auto const callees = resolver.potential_targets (call, call.getFunction ()->getName ());
          for (auto const& callee: callees)
            llvm::outs () << "--> callee: " << callee->getFunctionName () << "\n";

          var->uses.push_back (use_in_call {
            .callees = callees,
            .idx = last ? static_cast<size_t> (std::distance (call.args ().begin (), find (call.args (), last))) : 0,
            .by_ref = last ? last->getType ()->isPointerTy () : false,
            .loc = mcg::src_loc { .line = loc ? loc->getLine () : 0, .col = loc ? loc->getColumn () : 0, },
          });
          continue;
        }

        if (current->users ().empty ())
        {
          llvm::outs () << "--> users empty, skipping.\n";
          continue;
        }

        // Handle users of the current value
        for (auto const* user: current->users ())
          if (auto const* inst = dyn_cast<llvm::Instruction> (user); inst)
          {
            switch (inst->getOpcode ())
            {
            case llvm::Instruction::Store:
              if (auto const* alloc = dyn_cast<llvm::Instruction> (getPointerOperand (inst));
                  alloc && alloc->getOpcode () == llvm::Instruction::Alloca)
              {
                llvm::outs () << "--> reached store [" << *inst << "].\n";
                enqueue (current, alloc);
              }
              break;

            default:
              llvm::outs () << "--> found user [" << *inst << "].\n";
              enqueue (current, inst);
            }
          }
      }

      return var;
    });
  }
} // namespace cage

#endif // CAGE_DATAFLOW_HXX
