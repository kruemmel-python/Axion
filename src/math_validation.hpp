#pragma once
#include "common.hpp"
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

namespace mre::math_validation {
struct Case { const char* domain; int a; int b; int c; double expected; double tolerance; };
inline double cubicCount(int n, int power, int target) {
    std::vector<int> counts(static_cast<size_t>(n), 0);
    for (int x=0; x<n; ++x) { int residue=1; for (int i=0;i<power;++i) residue=(residue*x)%n; ++counts[static_cast<size_t>(residue)]; }
    long long total=0;
    for(int x=0;x<n;++x) for(int y=0;y<n;++y) {
        int rx=1, ry=1;
        for(int i=0;i<power;++i) {rx=(rx*x)%n;ry=(ry*y)%n;}
        const int need=(target-rx-ry+2*n)%n;
        total+=counts[static_cast<size_t>(need)];
    }
    return static_cast<double>(total);
}
inline double cycleGap(int n) { return 2.0-2.0*std::cos(2.0*std::acos(-1.0)/static_cast<double>(n)); }
inline double logistic(double initial, int steps) { double x=initial; for(int i=0;i<steps;++i) x=4.0*x*(1.0-x); return x; }
inline std::vector<Case> cases() {
    return {{"cubic",7,3,2,cubicCount(7,3,2),0.0}, {"cubic",11,3,0,cubicCount(11,3,0),0.0},
        {"cubic",13,3,5,cubicCount(13,3,5),0.0},
        {"cyclegap",5,0,0,cycleGap(5),1e-7},{"cyclegap",8,0,0,cycleGap(8),1e-7},
        {"cyclegap",11,0,0,cycleGap(11),1e-7},
        {"logistic",25,1,0,logistic(0.25,1),1e-8},
        {"logistic",20,2,0,logistic(0.20,2),1e-8},
        {"logistic",30,3,0,logistic(0.30,3),1e-8}};
}
inline bool parseValue(const std::string& output, double& value) {
    const std::string key="MRE_VERIFY value=";
    const auto pos=output.find(key);
    if(pos==std::string::npos) return false;
    const std::string number=output.substr(pos+key.size());
    std::istringstream in(number);
    in >> value;
    return static_cast<bool>(in) && std::isfinite(value);
}
inline bool approx(double actual,double expected,double tolerance){return std::abs(actual-expected)<=tolerance;}
} // namespace mre::math_validation
