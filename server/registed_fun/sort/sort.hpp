#ifndef SORT_H
#define SORT_H

#include<vector>
#include<algorithm>

std::vector<int> int_sort(std::vector<int> vec) {
    std::sort(vec.begin(), vec.end());
    return vec;
}

#endif
