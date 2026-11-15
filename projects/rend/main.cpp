
#include <memory>

#include "common/utils/main_helper.hpp"
#include "managers/rend/manager.hpp"


void force_include()
{
    // dummy vars
    std::string name{};
    std::string type{};

    // trick linker into thinking these types get used
    std::make_shared<managers::rend::Manager>(name, type);
}

int main(int argc, char* argv[])
{
    return common::utils::main_helper(argc, argv);
}
