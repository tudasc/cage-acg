#ifndef OMPOUTLINE_H
#define OMPOUTLINE_H

#include <optional>
#include <cstdlib>
#include <cstring>

namespace CallgraphGeneration {
    enum struct OmpOutlineMode {
        MERGE,
        TRANSPARENT,
        SPLIT,
    };

    inline std::optional<OmpOutlineMode> parseOmpOutlineMode() {
        const auto* val = getenv("CAGE_OMP_OUTLINE_MODE");

        if (!val)
            return {};

        if (!strcmp(val, "merge"))
            return OmpOutlineMode::MERGE;
        if (!strcmp(val, "split"))
            return OmpOutlineMode::SPLIT;
        if (!strcmp(val, "transparent"))
            return OmpOutlineMode::TRANSPARENT;

        return {};
    }

    struct OmpOutline {
        OmpOutline(metacg::Callgraph *mcg, CallGraph *lcg)
            : mode{parseOmpOutlineMode()}, mcg{mcg}, lcg{lcg}
        {}

        void add(CallBase const& I) const {
            const auto *parent = mcg->getOrInsertNode(I.getParent()->getParent()->getName().str());
            const auto *kmpc_fork = I.getCalledFunction();

            const metacg::CgNode *child{};

            if (mode == OmpOutlineMode::MERGE)
                return addMerged(parent, I);

            if (mode == OmpOutlineMode::SPLIT) {
                child = mcg->getOrInsertNode(kmpc_fork->getName().str());
                mcg->addEdge(parent, child);
                parent = child;
            }

            if (const auto *region = dyn_cast<Function>(I.getArgOperand(2)); region)
                child = mcg->getOrInsertNode(region->getName().str());

            mcg->addEdge(parent, child);
        }

        void addMerged(const metacg::CgNode *parent, const CallBase &I) const {
            auto *lcgNode = lcg->operator[](dyn_cast<Function>(I.getArgOperand(2)));

            for (const auto &[key, elem] : *lcgNode) {
                if (!key.has_value()) continue;
                if (elem->getFunction() == nullptr) continue;
                if (elem->getFunction()->isIntrinsic()) continue;

                const Function *childFunc = elem->getFunction();
                assert(childFunc->hasName());

                const auto *childNode = mcg->getOrInsertNode(childFunc->getName().str());
                mcg->addEdge(parent, childNode);
            }
        }

        [[nodiscard]] auto enabled() const { return mode.has_value(); }

    private:
        std::optional<OmpOutlineMode> mode;
        metacg::Callgraph *mcg;
        CallGraph *lcg;
    };
}

#endif // OMPOUTLINE_H
