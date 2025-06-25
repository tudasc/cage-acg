#ifndef FUNCTIONRESOLVER_H
#define FUNCTIONRESOLVER_H

namespace CallgraphGeneration {
    struct FunctionResolver {
        explicit FunctionResolver(const Module &m, metacg::Callgraph *mcg) : mcg{mcg} {
            DebugInfoFinder dbgFinder;
            dbgFinder.processModule(m);

            hasMetadata = dbgFinder.subprogram_count() != 0;

            for (const auto &i : dbgFinder.subprograms()) {
                if (const auto f = m.getFunction(i->getName()); f)
                    FunctionInfoMap[f] = i;
                else if (const auto lf = m.getFunction(i->getLinkageName()); lf)
                    FunctionInfoMap[lf] = i;
            }

            for (const auto &f : m.getFunctionList())
                SignatureFunctionMap[f.getFunctionType()].push_back(&f.getFunction());
        }

        auto potentialFuncs(const CallBase &I) {
            assert(!I.getCalledFunction());
            SmallVector<metacg::CgNode *> r{};

            // Attempt to resolve the call base as a virtual call
            if (const auto vcall = metavirt::vcall_data_for(&I); vcall && !vcall->call_targets.empty()) {
                for (const auto &[name, origin] : fn_names_and_origins(*vcall))
                    r.push_back(mcg->getOrInsertNode(name.str(), origin.str()));

                return r;
            }

            // Resolve all other function pointers via function type approximation
            for (const auto &possibleFuncs = SignatureFunctionMap[I.getFunctionType()]; const auto &f : possibleFuncs) {
                metacg::CgNode *childNode;
                if (hasMetadata && is_contained(FunctionInfoMap, f))
                    childNode = mcg->getOrInsertNode(
                        FunctionInfoMap[f]->getLinkageName().str(),
                        FunctionInfoMap[f]->getFilename().str()
                    );
                else
                    childNode = mcg->getOrInsertNode(f->getName().str());

                r.push_back(childNode);
            }

            return r;
        }

        const auto &functionInfoMap() { return FunctionInfoMap; }

    private:
        bool hasMetadata;
        metacg::Callgraph *mcg{};
        std::unordered_map<const Function *, const DISubprogram *> FunctionInfoMap{};
        std::unordered_map<FunctionType *, std::vector<const Function *>> SignatureFunctionMap{};
    };
}

#endif //FUNCTIONRESOLVER_H
