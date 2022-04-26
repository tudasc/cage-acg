#include "Runtime/library.h"

#include "MCGReader.h"

#include <iostream>

int a=0;

void getGCC(void* data) {
    printf("Controlflow diverted to runtime component: %d\n",a);
    a++;
    auto JsonString=std::string( (char*) data);
    nlohmann::basic_json json=nlohmann::json::parse( JsonString);
    metacg::io::JsonSource jsonSource(json);
    metacg::io::VersionTwoMetaCGReader metaCgReader(jsonSource);
    auto& mcgManager= metacg::graph::MCGManager::get();
    mcgManager.resetManager();
    mcgManager.addToManagedGraphs("emptyGraph",std::make_unique<metacg::Callgraph>());
    printf("%s\n",json.dump(2,' ').c_str());
    metaCgReader.read(mcgManager);
    printf("Runtimecomponent finished, resuming program\n");
}
