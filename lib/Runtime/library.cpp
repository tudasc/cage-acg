#include "Runtime/library.h"

#include "MCGReader.h"
#include "MCGWriter.h"

#include <iostream>
#include <MCGBaseInfo.h>

//Fixme: These Global names can not alias with any other Global name
// we might be loaded into
auto &genCCRT_mcgManager = metacg::graph::MCGManager::get();
int genCCRT_NumOfReferences = 0;

void print(char *data) {
    auto JsonString = std::string(data);
    nlohmann::basic_json json = nlohmann::json::parse(JsonString);
    metacg::io::JsonSource jsonSource(json);
    printf("%s\n", json.dump(2, ' ').c_str());
}

void help() {
    printf("Type H for Help\n");
    printf("Type C to continue\n");
    printf("Type P to Print incoming data\n");
    printf("Type A to Add incoming data\n");
    printf("Type M to Merge incoming data\n");
    printf("Type D to Dump all available data\n");
    printf("Type f to Dump active Graph to File\n");
    printf("Type F to Dump all Graphs to File\n");
}


void dump() {
    for(int i=0;i<genCCRT_NumOfReferences;i++){
        std::string currentGraph=std::to_string(i).append("emptyGraph");
        printf("Callgraph: %s \n",currentGraph.c_str());
        std::cout<<genCCRT_mcgManager.getCallgraph(currentGraph);
        printf("--------------------------------------\n\n");
    }
}

void add(char *data) {
    printf("Adding data to manager %s\n",std::to_string((genCCRT_NumOfReferences)).append("emptyGraph").c_str());
    auto JsonString = std::string(data);
    nlohmann::basic_json json = nlohmann::json::parse(JsonString);
    metacg::io::JsonSource jsonSource(json);
    metacg::io::VersionThreeMetaCGReader metaCgReader(jsonSource);

    genCCRT_mcgManager.addToManagedGraphs(std::to_string((genCCRT_NumOfReferences)).append("emptyGraph"),
                                          std::make_unique<metacg::Callgraph>());
    metaCgReader.read(genCCRT_mcgManager);
    genCCRT_NumOfReferences++;
}

void merge(char *data, int source = 0, int target = 0) {
    if (target == source && source == 0) {
        add(data);
        printf("Merging Data into %s\n",(std::to_string(0).append("emptyGraph").c_str()));
        auto targetGraph = std::to_string(0).append("emptyGraph");
        auto sourceGraph = std::to_string((genCCRT_NumOfReferences-1)).append("emptyGraph");
        genCCRT_mcgManager.mergeGraphs(targetGraph, sourceGraph);
    } else {
        printf("Merging Data from Graph %d into Data from Graph %d\n", source, target);
        printf("This is currently unimplemented \n");
    }
}

void toFile(){
    genCCRT_mcgManager.dumpToFile("",true);
}

void allToFile(){
    genCCRT_mcgManager.dumpAllGraphsToFile();
}

void getGCC(void *data) {
    printf("Controlflow diverted to runtime component step: %d \n", genCCRT_NumOfReferences);
    printf("H for help\n");
    char inp;
    while (true) {
        std::cin >> inp;
        switch (inp) {
            case 'H':
                help();
                continue;
            case 'P':
                print((char *) data);
                continue;
            case 'M':
                merge((char *) data);
                continue;
            case 'A':
                add((char *) data);
                continue;
            case 'D':
                dump();
                continue;
            case 'f':
                toFile();
                continue;
            case 'F':
                allToFile();
                continue;
            default :
                break;
        }
        break;
    }
    printf("Runtimecomponent finished, resuming program\n");
}

//Nodes und Format refactor schreiben

int main() {
    std::cout << "Execution!!" << "\n";
}
