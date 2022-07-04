#include "Procedures.h"
#include<algorithm>
#include <random>

namespace jrRPC
{
    namespace RegistedProc
    {
        namespace NonMember
        {
            std::vector<int> intSort(std::vector<int> vec)
            {
                std::sort(vec.begin(), vec.end());
                return vec;
            }

            int noparamFunc()
            {
                return ::rand() % 100;
            }
        }
    }
}
