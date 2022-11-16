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

#include "MetaData.h"
#include "nlohmann/json.hpp"


/**
 * Class to hold data which can be annotated to a node
 */
    class GenCCVtableMetadata : public metacg::MetaData {
    public:
        static constexpr const char *key() { return "GenCC_VTableMetadata"; }

        std::vector<CgNodeRawPtrUSet> getCallSet(){
            return callingSet;
        };

        void addToCallSet(const CgNodeRawPtrUSet& set){
            callingSet.push_back(set);
        };

        std::string testString =std::string("TestString");

    private:
        std::vector<CgNodeRawPtrUSet> callingSet; //vector( {a::foo}, {a::foo, b::foo}, {a::foo,c::foo})
    };


    class  GenCCVtableMetadatahandler final:  public metacg::io::retriever::MetaDataHandler{
    public:
            /** Invoked to decide if meta data should be output into the json file for the node */
            [[nodiscard]] bool handles(const metacg::CgNode* const n) const final{
                return n->has<GenCCVtableMetadata>();
            };

            /** Invoked to find or create meta data entry in json */
            [[nodiscard]] const std::string toolName() const final{

                return "GenCCVtables";
            };

            /** Creates or returns the object to attach as meta information */
            nlohmann::json value(const metacg::CgNode* const n)  const final {
                std::vector<std::set<std::string>> retVal;
                for(const auto& elem :n->get<GenCCVtableMetadata>()->getCallSet()){
                    std::set<std::string> tempSet;
                    for(const auto& func : elem){
                        tempSet.insert(func->getFunctionName());
                    }
                    retVal.push_back(tempSet);
                }
                return retVal;
                //return n->get<GenCCVtableMetadata>()->testString;
            }

            /** Reads the meta data from the json file and attaches it to the graph nodes */
            void read([[maybe_unused]] const nlohmann::json &j, const std::string &functionName) final {
                return;
            };

             ~GenCCVtableMetadatahandler() final = default;

        };
    void to_json(nlohmann::json &j, const GenCCVtableMetadata &md);


#endif

#endif //CALLGRAPHGENERATION_VTABLEMETADATA_H
