//
// Created by tim on 24.10.22.
//

#ifndef CALLGRAPHGENERATION_VTABLEMETADATA_H
#define CALLGRAPHGENERATION_VTABLEMETADATA_H
/**
 * File: VTableMetadata.h
 */


#include <optional>
#include <CgNode.h>
#include <Callgraph.h>
#include <set>

#include "metadata/MetaData.h"
#include "nlohmann/json.hpp"


/**
 * Class to hold data which can be annotated to a node
 */
    class GenCCVtableMetadata : public metacg::MetaData::Registrar<GenCCVtableMetadata> {
    public:
        static constexpr const char *key = "GenCC_VTableMetadata";
        explicit GenCCVtableMetadata(const nlohmann::json &j){
            outs()<<"Recreating VTables from JSON is not yet implemented\n";
        };

        explicit GenCCVtableMetadata()=default  ;

        nlohmann::json to_json() const final {
            std::vector<std::set<std::string>> retVal;
            for(const auto& elem : callingSet){
                std::set<std::string> tempSet;
                for(const auto& func : elem){
                    tempSet.insert(func->getFunctionName());
                }
                retVal.push_back(tempSet);
            }
            return retVal;
        }


        virtual const char* getKey() const final {return key;}

        std::vector<CgNodeRawPtrUSet> getCallSet(){
            return callingSet;
        };

        void addToCallSet(const CgNodeRawPtrUSet& set){
            callingSet.push_back(set);
        };

    private:
        std::vector<CgNodeRawPtrUSet> callingSet; //vector( {a::foo}, {a::foo, b::foo}, {a::foo,c::foo})
    };

#endif //CALLGRAPHGENERATION_VTABLEMETADATA_H
