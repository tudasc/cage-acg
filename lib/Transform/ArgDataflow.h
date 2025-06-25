#ifndef ARGDATAFLOW_H
#define ARGDATAFLOW_H

/*
 * - Support extension points / callbacks for instructions
 */

#include <unordered_map>
#include <unordered_set>
#include <deque>

#include "FunctionResolver.h"

namespace CallgraphGeneration {
    struct Path {
        Path() = default;
        explicit Path(const Value *val) : values{val} {}

        [[nodiscard]] const Value *last() const { return values[values.size() - 1]; }

        SmallVector<const Value *, 16> values{};
    };

    inline raw_ostream &operator<<(raw_ostream &OS, const Path &path) {
        for (const auto [i, v] : enumerate(path.values)) {
            OS << '[' << *v << ']';
            if (i != path.values.size() - 1)
                OS << " -> ";
        }

        return OS;
    }

    struct PathStack {
        explicit PathStack(const Path &initial = {}) : seens{{}}, paths{initial} {}

        auto &current() { return paths.back(); }

        auto &current_seen() { return seens.back(); }

        Path& fork(const Path& path) {
            paths.push_back(path);
            seens.push_back(current_seen());
            return current();
        }

        SmallVector<std::unordered_set<const Value *>> seens{};
        SmallVector<Path> paths{};
    };

    inline void computeArgPathRec(
        PathStack& paths,
        std::deque<const Value *>& workq
    ) {
        static auto handle_user = [&workq, &paths](Path& path, const User *user) {
            if (const auto *inst = dyn_cast<Instruction>(user); inst) {
                if (is_contained(paths.current_seen(), inst) && !isa<CallInst>(inst))
                    return;

                path.values.push_back(inst);
                paths.current_seen().insert(inst);

                switch (inst->getOpcode()) {
                    case Instruction::Store:
                        // `alloca`s are not uses of the related `store`s
                        if (const auto alloca = dyn_cast<Instruction>(getPointerOperand(inst));
                            alloca && alloca->getOpcode() == Instruction::Alloca)
                        {
                            path.values.push_back(alloca);
                            workq.push_back(alloca);
                        }
                    break;

                    default:
                        workq.push_back(inst);
                }
            }
        };

        while (!workq.empty()) {
            const auto val = workq.back();
            workq.pop_back();

            Path old = paths.current();

            if (val->users().empty())
                return;

            // We don't need to look through calls
            if (const auto *inst = dyn_cast<Instruction>(val); inst && inst->getOpcode() == Instruction::Call)
                return;

            for (const auto &[i, user] : enumerate(val->users())) {
                handle_user(i == 0 ? paths.current() : paths.fork(old), user);
                computeArgPathRec(paths, workq);
            }
        }
    }

    inline PathStack computeArgPath(const Value *arg) {
        PathStack paths{Path{arg}};
        std::deque workq{arg};

        computeArgPathRec(paths, workq);
        erase_if(paths.paths, [](const Path &path) {
            const auto *inst = dyn_cast<Instruction>(path.last());
            return !inst || inst->getOpcode() != Instruction::Call;
        });

        return paths;
    }

    struct SrcLoc {
        size_t line, col;
    };

    inline auto operator ==(const SrcLoc &lhs, const SrcLoc &rhs) {
        return lhs.line == rhs.line && lhs.col == rhs.col;
    }

    inline void to_json(nlohmann::json &j, const SrcLoc &md) {
        j = {
            {"line", md.line},
            {"col", md.col},
        };
    }

    inline void from_json(const nlohmann::json &j, SrcLoc &md) {
        j.at("line").get_to(md.line);
        j.at("col").get_to(md.col);
    }

    struct ArgMd {
        size_t idx;
        std::vector<size_t> callees;
        bool by_ref;
        SrcLoc loc;
        /* enum kind { indirect, direct, virtual } */
    };

    inline void to_json(nlohmann::json &j, const ArgMd &md) {
        j = {
            {"callees", md.callees},
            {"idx", md.idx},
            {"by_ref", md.by_ref},
            {"loc", md.loc},
        };
    }

    inline void from_json(const nlohmann::json &j, ArgMd &md) {
        j.at("callees").get_to(md.callees);
        j.at("idx").get_to(md.idx);
        j.at("by_ref").get_to(md.by_ref);
        j.at("loc").get_to(md.loc);
    }

    struct ArgsMd {
        size_t idx;
        std::vector<ArgMd> outs;
    };

    inline void to_json(nlohmann::json &j, const ArgsMd &md) {
        j = {
            {"idx", md.idx},
            {"outs", md.outs},
        };
    }

    inline void from_json(const nlohmann::json &j, ArgsMd &md) {
        j.at("idx").get_to(md.idx);
        j.at("outs").get_to(md.outs);
    }

    struct ArgFlowMd : metacg::MetaData::Registrar<ArgFlowMd> {
        static constexpr auto key = "argFlow";

        ArgFlowMd() = default;
        ArgFlowMd(const ArgFlowMd &) = default;

        explicit ArgFlowMd(const nlohmann::json& j) {
            j.at("args").get_to(args);
        }

        [[nodiscard]] nlohmann::json to_json() const override {
            return {
                {"args", args}
            };
        }

        [[nodiscard]] const char *getKey() const override { return key; }

        [[nodiscard]] MetaData *clone() const override { return new ArgFlowMd{*this}; }

        // TODO(laurin): idk
        void merge(const MetaData&) final {}

        void addArg(ArgsMd &&arg) { args.push_back(arg); }

        std::vector<ArgsMd> args{};
    };

    struct EdgeCallsMd : metacg::MetaData::Registrar<EdgeCallsMd> {
        static constexpr auto key = "calls";

        EdgeCallsMd() = default;
        EdgeCallsMd(const EdgeCallsMd &) = default;

        explicit EdgeCallsMd(const nlohmann::json& j) {
            j.at("locs").get_to(locs);
        }

        [[nodiscard]] nlohmann::json to_json() const override {
            return {
                {"locs", locs},
            };
        }

        [[nodiscard]] const char *getKey() const override { return key; }

        [[nodiscard]] MetaData *clone() const override { return new EdgeCallsMd{*this}; }

        // TODO(laurin): idk
        void merge(const MetaData&) final {}

        void addLoc(const SrcLoc &loc) { locs.push_back(loc); }

        std::vector<SrcLoc> locs;
    };

    inline const DILocation *getLoc(const Instruction *inst) {
        SmallVector<std::pair<unsigned, MDNode *>> mds;
        inst->getAllMetadata(mds);

        for (const auto &[_, md] : mds) {
            if (const auto *loc = dyn_cast<DILocation>(md); loc)
                return loc;
        }

        return nullptr;
    }

    inline auto argPathsForFn(metacg::Callgraph *mcg, FunctionResolver& resolver, const Function &fn) {
        std::unordered_map<const Function *, ArgFlowMd *> md{};

        for (const auto &[in, arg] : enumerate(fn.args())) {
            const auto stack = computeArgPath(&arg);

            auto argsMd = ArgsMd{in};
            const Function *called_f{};

            for (const auto &path : stack.paths) {
                const auto *call = dyn_cast<CallBase>(path.values.back());
                std::vector<size_t> callees{};
                if (!call->getCalledFunction()) {
                    called_f = nullptr;
                    for (const auto funcs = resolver.potentialFuncs(*call); const auto *node : funcs)
                        callees.push_back(node->getId());
                } else {
                    called_f = call->getCalledFunction();
                    callees.push_back(mcg->getOrInsertNode(call->getCalledFunction()->getName().str())->getId());
                }

                const auto *loc = getLoc(call);

                for (const auto [i, arg_]: enumerate(call->args())) {
                    if (arg_.get() == path.values[path.values.size() - 2])
                        argsMd.outs.push_back(ArgMd{
                            .idx = i,
                            .callees = std::move(callees),
                            .by_ref = arg_->getType()->isPointerTy(),
                            .loc = SrcLoc { loc ? loc->getLine() : 0, loc ? loc->getColumn() : 0 },
                        });
                }
            }

            if (called_f) {
                if (md.contains(called_f))
                    md[called_f] = new ArgFlowMd{};

                md[called_f]->addArg(std::move(argsMd));
            }
        }

        return md;
    }
}

#endif // ARGDATAFLOW_H
