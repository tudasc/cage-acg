#ifndef CAGE_DIFINDER_HXX
#define CAGE_DIFINDER_HXX

#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/Casting.h>
#include <ranges>

namespace ACGPlugin::di {
    namespace compat {
        template <typename DbgVar>
        llvm::Value* get_alloca_for(DbgVar const* dbg_var) {
#if LLVM_VERSION_MAJOR < 13
            return dbg_var->getVariableLocation();
#else
            return dbg_var->getVariableLocationOp(0);
#endif
        }
    } // namespace compat

#if LLVM_VERSION_MAJOR < 19

    inline std::optional<llvm::DbgVariableIntrinsic const*> find_intrinsic(llvm::Instruction const* inst) {
        if (!inst)
            return {};

        for (auto const& func = *inst->getFunction(); auto const& instr : instructions(func))
            if (auto const* dbg = dyn_cast<llvm::DbgVariableIntrinsic>(&instr))
                if (compat::get_alloca_for(dbg) == inst)
                    return dbg;

        return {};
    }

#else

    inline std::optional<llvm::DbgVariableRecord const*> find_intrinsic(llvm::Instruction const* root) {
        if (!root)
            return {};

        for (auto const& inst : *root->getParent())
            for (llvm::DbgVariableRecord const& var : filterDbgVars(inst.getDbgRecordRange()))
                if (compat::get_alloca_for(&var) == root)
                    return &var;

        return {};
    }

#endif // LLVM_VERSION_MAJOR < 19

    inline llvm::DILocation const* location(llvm::Instruction const* inst) {
        llvm::SmallVector<std::pair<unsigned, llvm::MDNode*>> mds{};
        inst->getAllMetadata(mds);

        for (auto const& md : mds | std::views::values)
            if (auto const* loc = dyn_cast<llvm::DILocation>(md); loc)
                return loc;

        return nullptr;
    }
} // namespace ACGPlugin::di

#endif // CAGE_DIFINDER_HXX
