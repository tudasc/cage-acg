#include "dataflow.h"
#include "omp.h"
#include "resolver.h"

#include <cxxabi.h>
#include <fstream>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/InstVisitor.h>
#include <metavirt/support/Logger.h>

#include "cage/interface/CaGePlugin.h"

template <typename T, typename R>
auto collect(R&& range) {
    T container{};

    for (auto&& it : std::forward<R>(range))
        container.push_back(std::forward<decltype(it)>(it));

    return container;
}

struct TheACGPlugin : cage::Plugin {

    struct call_base_visitor : llvm::InstVisitor<call_base_visitor> {
        explicit call_base_visitor(metacg::Callgraph& mcg, llvm::CallGraph& lcg) :
            omp_handler{mcg, lcg}, resolv{lcg.getModule(), mcg}, mcg(mcg), lcg(lcg) {};

        void visitCallBase(llvm::CallBase const& call) const {
            // Skip calls to intrinsics
            if (call.getCalledFunction()==nullptr)
                return;

            if (call.getCalledFunction()->isIntrinsic())
                return;

            if (call.getCalledFunction()->getName() == "__kmpc_fork_call" && omp_handler.enabled())
                omp_handler.add(call);
        }

        void visitFunction(llvm::Function const& f) const {
            if (f.isIntrinsic())
                return;

            auto const name = f.getName();
            LOG_DEBUG("Visiting function: " << name);

            // Skip certain OpenMP related functions depending on the selected OpenMP mode
            if (name == "__kmpc_fork_call" && omp_handler.has_not_enabled_mode(ACGPlugin::omp_mode::split))
                return;
            if (name.contains("omp_outlined") && omp_handler.has_enabled_mode(ACGPlugin::omp_mode::merge))
                return;

            metacg::CgNode* current = mcg.getFirstNode(f.getName().str());
            handle_args(f, current);
            handle_locals(f, current);
        }

    private:
        void handle_args(llvm::Function const& f, metacg::CgNode* node) const {
            LOG_DEBUG("> processing arguments...");

            auto const published = published_values(
                map_range(f.args(), [&f](auto const& arg) { return std::make_pair(&arg, &arg - f.arg_begin()); }),
                resolv, false);
            auto const args = make_filter_range(published, [](auto const& it) { return it.has_value(); });

            auto md = std::make_unique<ACGPlugin::mcg::md_arg_flow>();
            for (auto const& arg : args) {
                auto const outs = collect<std::vector<ACGPlugin::mcg::md_arg_output>>(
                    llvm::map_range(arg->uses, [](ACGPlugin::use_in_call const& use) {
                        auto const callees = map_range(use.callees, [](metacg::CgNode const* c) { return c->getId(); });

                        return ACGPlugin::mcg::md_arg_output{
                            .idx = use.idx,
                            .callees = collect<std::vector<size_t>>(callees),
                            .by_ref = use.by_ref,
                            .loc = use.loc,
                        };
                    }));

                md->arg(ACGPlugin::mcg::md_arg{
                    .idx = arg->idx,
                    .outs = outs,
                });
            }

            if (!md->args.empty())
                node->addMetaData(std::move(md));
        }

        void handle_locals(llvm::Function const& f, metacg::CgNode* node) const {
            LOG_DEBUG("> processing locals...");

            auto const insts = instructions(f);
            auto const published = published_values(
                map_range(make_filter_range(insts,
                                            [](auto const& inst) {
                                                if (!isa<llvm::AllocaInst>(&inst))
                                                    return false;

                                                // Ignore allocas that are just stack copies of function parameters
                                                for (auto const* alloca = cast<llvm::AllocaInst>(&inst);
                                                     auto const* user : alloca->users())
                                                    if (auto const* instr = dyn_cast<llvm::Instruction>(user); instr) {
                                                        switch (instr->getOpcode()) {
                                                        case llvm::Instruction::Store:
                                                            if (isa<llvm::Argument>(instr->getOperand(0)))
                                                                return false;
                                                            break;

                                                        default:
                                                            break;
                                                        }
                                                    }

                                                return true;
                                            }),
                          // Map this to a dummy pair to keep the `published_values` interface consistent
                          [](auto const& inst) { return std::make_pair(&inst, 0); }),
                resolv, true);

            // Collect local variables that escape the current function
            auto const locals = llvm::make_filter_range(published, [](auto const& it) { return it.has_value(); });
            LOG_DEBUG("-> No intrinsic metadata for local " << published.empty())
            for (auto const& local : locals)
                if (auto const intrin = ACGPlugin::di::find_intrinsic(dyn_cast<llvm::Instruction>(local->val));
                    intrin) {
                    if ((*intrin)->getVariable()) {
                        LOG_DEBUG("-> local [" << (*intrin)->getVariable()->getName() << "] is published.");
                    } else {
                    }
                    LOG_DEBUG("-> No intrinsic metadata for local " << *local->val)
                }

            auto md = std::make_unique<ACGPlugin::mcg::md_locals>();
            for (auto const& local : locals)
                for (auto const& use : local->uses) {
                    auto const callees = map_range(use.callees, [](metacg::CgNode const* c) { return c->getId(); });
                    md->local(ACGPlugin::mcg::md_local{
                        .callees = collect<std::vector<size_t>>(callees),
                        .loc = use.loc,
                    });
                }

            if (!md->locals.empty())
                node->addMetaData(std::move(md));
        }

        ACGPlugin::omp omp_handler;
        ACGPlugin::resolver resolv;
        metacg::Callgraph& mcg;
        llvm::CallGraph& lcg;
    };

    void augmentCallGraph(const llvm::Module& m, metacg::Callgraph& cg) final {
        metacg::MCGLogger::logInfo("Got called");
        llvm::CallGraph lcg(const_cast<llvm::Module&>(m));
        call_base_visitor visitor{cg, lcg};
        visitor.visit(const_cast<llvm::Module&>(m));
    }

    [[nodiscard]] std::string getPluginName() const override { return "Argument Call Graph"; }
};


// This is the function which is used to get the FileInformationMetadataPlugin struct
extern "C" {
cage::Plugin* getPlugin() { return new TheACGPlugin(); }
}