#include "memory.hpp"
#include <boost/program_options.hpp>

using namespace Cache;

int main(int argc, char* argv[]) {
    auto desc = create_options_description();
    auto vm = parse_command_line_args(argc, argv, desc);
    
    if (handle_help_option(vm, desc)) {
        return 0;
    }

    TraceLevel trace = get_trace_level(vm);
    MemoryInitMode init = get_memory_init_mode(vm);

    auto cache = std::make_shared<Cache::Cache>(
        4 * 1024, 64, 4, 32,
        WritePolicy::WRITE_BACK,
        AllocationPolicy::READ_ALLOCATE,
        ReplacementPolicy::LRU
    );

    auto memory = std::make_shared<MemoryModel>(trace);
    memory->initialize(init);
    
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
