
#include <memory>

#include "common/utils/main_helper.hpp"
#include "managers/controller/manager.hpp"



// this function never actually called. 
// it just tricks the linking into doing its job
void force_include()
{
    std::string n,t; // dummy vars
    std::make_shared<managers::controller::Manager>(n, t);
}

int main(int argc, char* argv[])
{
    return common::utils::main_helper(argc, argv);
}
