#include "memory.hpp"

#include <boost/program_options.hpp>

using namespace Cache;

int main(int argc, char* argv[]) {
    boost::program_options::options_description desc("Single Cache Model Options");
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
        std::cout << desc << "\n";
        return 0;
    }

    TraceLevel trace = static_cast<TraceLevel>(vm["trace"].as<int>());
    MemoryInitMode init = static_cast<MemoryInitMode>(vm["init"].as<int>());

    auto l1_cache = std::make_shared<Cache::Cache>(
        16 * 1024, 32, 4, 32,
        WritePolicy::WRITE_BACK,
        AllocationPolicy::BOTH,
        ReplacementPolicy::MRU
    );

    auto l2_cache = std::make_shared<Cache::Cache>(
        256, 32, 256/32, 32,
        WritePolicy::WRITE_THROUGH,
        AllocationPolicy::WRITE_ALLOCATE,
        ReplacementPolicy::LRU
    );
    
    auto memory = std::make_shared<MemoryModel>(trace);
    memory->initialize(init);
    
    std::vector<std::shared_ptr<Cache::Cache>> caches;
    caches.push_back(l1_cache);
    caches.push_back(l2_cache);
    auto hierarchy = std::make_shared<MemoryHierarchy>(caches, memory, trace);

    std::cout << "L1: 16KB, 32B blocks, 4-way, BOTH-Allocate, Write-Back, MRU\n"      
    << "L2: 256B, 32B blocks, Fully-Assoc, Write-Allocate, Write-Through, LRU\n";
    
    if (vm.count("test")) {
        run_tests(vm["test"].as<std::string>(), hierarchy);
    } else {
        process_commands(hierarchy);
    }
    
    return 0;
}

// int main() {
//     auto l1_cache = std::make_shared<Cache::Cache>(
//         16 * 1024, 32, 4, 32,
//         WritePolicy::WRITE_BACK,
//         AllocationPolicy::BOTH,
//         ReplacementPolicy::MRU
//     );

//     auto l2_cache = std::make_shared<Cache::Cache>(
//         256, 32, 256/32, 32,
//         WritePolicy::WRITE_THROUGH,
//         AllocationPolicy::WRITE_ALLOCATE,
//         ReplacementPolicy::LRU
//     );

//     auto memory = std::make_shared<MemoryModel>();
//     std::vector<std::shared_ptr<Cache::Cache>> caches;
//     caches.push_back(l1_cache);
//     caches.push_back(l2_cache);
//     auto hierarchy = std::make_shared<MemoryHierarchy>(caches, memory);

//     std::cout << "L1: 16KB, 32B blocks, 4-way, BOTH-Allocate, Write-Back, MRU" << std::endl
//     << "L2: 256B, 32B blocks, Fully-Assoc, Write-Allocate, Write-Through, LRU" << std::endl;
    
//     process_commands(hierarchy);
//     return 0;
// }