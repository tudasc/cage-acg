#ifndef CAGE_DATAFLOW_HXX
#define CAGE_DATAFLOW_HXX

#include "difinder.hxx"
#include "metacg.hxx"
#include "resolver.hxx"

#include <cage/Logger.h>
#include <llvm/IR/InstIterator.h>

namespace cage {
struct use_in_call {
  llvm::SmallVector<metacg::CgNode*, 4> callees{};
  size_t idx{};
  bool by_ref{};
  mcg::src_loc loc{};
};

struct published_value {
  size_t idx{};
  llvm::Value const* val{};
  llvm::SmallVector<use_in_call> uses{};
};

struct workq_item {
  llvm::Value const *last{}, *current{};
};

template <typename R>
auto published_values(R&& inputs, resolver const& resolver, bool is_local_flow) {
  LOG_DEBUG("> Published value called");
  for (auto const& it : inputs) {
    LOG_DEBUG("-> element: [" << *std::get<0>(it) << "]");
  }

  return map_range(inputs, [&resolver, is_local_flow](std::pair<llvm::Value const*, size_t> input) {
    LOG_DEBUG("-> input is [" << *std::get<0>(input) << "].");

    llvm::SmallVector<workq_item, 64> workq{workq_item{.last = nullptr, .current = std::get<0>(input)}};
    llvm::SmallPtrSet<llvm::Value const*, 32> seen{};

    std::optional<published_value> var{};

    auto const enqueue = [&workq, &seen](llvm::Value const* last, llvm::Value const* val) {
      if (auto const [_, inserted] = seen.insert(val); inserted) {
        LOG_DEBUG("!--> enqueueing [" << *val << "]...");
        workq.emplace_back(last, val);
      }
    };

    while (!workq.empty()) {
      auto const [last, current] = workq.pop_back_val();

      if (last) {
        LOG_DEBUG("--> last is [" << *last << "].");
      }
      LOG_DEBUG("--> current is [" << *current << "].");

      // If we reach a call, try to resolve the call to the set of potential callees and move on to the next
      // item in the work queue.
      if (auto const* inst = dyn_cast<llvm::Instruction>(current);
          inst && inst->getOpcode() == llvm::Instruction::Call) {
        auto const& call = *cast<llvm::CallBase>(inst);
        auto const* loc  = di::location(inst);

        if (is_local_flow && call.getCalledFunction()) {
          continue;
        }

        if (!var.has_value()) {
          var.emplace(std::get<1>(input), std::get<0>(input));
        }

        LOG_DEBUG("--> input index is [" << var->idx << "].");

        auto const callees = resolver.potential_targets(call, call.getFunction()->getName());
        for (auto const& callee : callees) {
          LOG_DEBUG("--> callee: " << callee->getFunctionName());
        }

        var->uses.push_back(use_in_call{
            .callees = callees,
            .idx     = last ? static_cast<size_t>(std::distance(call.args().begin(), find(call.args(), last))) : 0,
            .by_ref  = last ? last->getType()->isPointerTy() : false,
            .loc =
                mcg::src_loc{
                    .line = loc ? loc->getLine() : 0,
                    .col  = loc ? loc->getColumn() : 0,
                },
        });
        continue;
      }

      if (current->users().empty()) {
        LOG_DEBUG("--> users empty, skipping.");
        continue;
      }

      // Handle users of the current value
      for (auto const* user : current->users())
        if (auto const* inst = dyn_cast<llvm::Instruction>(user); inst) {
          switch (inst->getOpcode()) {
            case llvm::Instruction::Store:
              if (auto const* alloc = dyn_cast<llvm::Instruction>(getPointerOperand(inst));
                  alloc && alloc->getOpcode() == llvm::Instruction::Alloca) {
                LOG_DEBUG("--> reached store [" << *inst << "].");
                enqueue(current, alloc);
              }
              break;

            default: {
              LOG_DEBUG("--> found user [" << *inst << "].");
              enqueue(current, inst);
            }
          }
        }
    }

    return var;
  });
}
}  // namespace cage

#endif  // CAGE_DATAFLOW_HXX
