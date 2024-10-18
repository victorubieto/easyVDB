#pragma once

//#include <vector>
#include <string>
#include <assert.h>
//#include <iostream>

// Paralelization
//#include <omp.h>
//#pragma omp parallel for //private(wordIndex, threadId)
//threadId = omp_get_thread_num();

// Profiling
#include <chrono>
//const auto t1 = std::chrono::high_resolution_clock::now();
//const auto t2 = std::chrono::high_resolution_clock::now();
//// floating-point duration: no duration_cast needed
//const std::chrono::duration<double, std::milli> fp_ms = t2 - t1;
//const std::chrono::duration<double, std::milli> fp_ms2 = t3 - t1;
//const std::chrono::duration<double, std::milli> fp_ms3 = t3 - t2;

namespace easyVDB
{

    inline unsigned int countChars(std::string s, char c)
    {
	    unsigned int count = 0;

	    for (int i = 0; i < s.size(); i++)
		    if (s[i] == c) count++;

	    return count;
    }

    inline long long hexToDec(std::string hex)
    {
        long long result = 0;
        for (int i = 0; i < hex.length(); i++) {
            if (hex[i] >= 48 && hex[i] <= 57)
            {
                result += (hex[i] - 48) * pow(16, hex.length() - i - 1);
            }
            else if (hex[i] >= 65 && hex[i] <= 70) {
                result += (hex[i] - 55) * pow(16, hex.length() - i - 1);
            }
            else if (hex[i] >= 97 && hex[i] <= 102) {
                result += (hex[i] - 87) * pow(16, hex.length() - i - 1);
            }
        }
        return result;
    }

}