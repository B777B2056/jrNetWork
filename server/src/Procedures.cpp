#include "Procedures.h"
#include<algorithm>

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
        }
    }
}
