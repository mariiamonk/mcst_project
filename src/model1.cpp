#include "memory.hpp"
#include <boost/program_options.hpp>

using namespace Cache;

int main(int argc, char* argv[]) {
    boost::program_options::options_description desc("Two-Level Cache Model Options");
    desc.add_options()
        ("help,h", "Show help message")
        ("trace,t", boost::program_options::value<int>()->default_value(0), 
         "Trace level (0=none, 1=basic, 2=full)")
        ("init,i", boost::program_options::value<int>()->default_value(0), 
         "Memory init mode (0=zeros, 1=addresses)")
        ("test", boost::program_options::value<std::string>(), 
         "Run test from file");
    
    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    TraceLevel trace = static_cast<TraceLevel>(vm["trace"].as<int>());

    auto cache = std::make_shared<Cache::Cache>(
        4 * 1024, 64, 4, 32,
        WritePolicy::WRITE_BACK,
        AllocationPolicy::READ_ALLOCATE,
        ReplacementPolicy::LRU
    );

    auto memory = std::make_shared<MemoryModel>(trace);
    memory->initialize(MemoryInitMode::ADDRESSES);
    
    std::vector<std::shared_ptr<Cache::Cache>> caches;
    caches.push_back(cache);
    auto hierarchy = std::make_shared<MemoryHierarchy>(caches, memory, trace);

    std::cout << "L1: 4KB, 64B blocks, 4-way, Read-Allocate, Write-Back, LRU\n";
    
    if (vm.count("test")) {
        run_tests(vm["test"].as<std::string>(), hierarchy);
    } else {
        process_commands(hierarchy);
    }
    
    return 0;
}


// using namespace Cache;

// int main() {
//     auto cache = std::make_shared<Cache::Cache>(
//         4 * 1024, 64, 4, 32,
//         WritePolicy::WRITE_BACK,
//         AllocationPolicy::READ_ALLOCATE,
//         ReplacementPolicy::LRU
//     );
    
//     auto memory = std::make_shared<MemoryModel>();
//     std::vector<std::shared_ptr<Cache::Cache>> caches;
//     caches.push_back(cache);
//     auto hierarchy = std::make_shared<MemoryHierarchy>(caches, memory);

//     std::cout << "Cache: 4KB, 64B blocks, 4-way, Read-Allocate, Write-Back, LRU\n";
    
//     process_commands(hierarchy);
//     return 0;
// }