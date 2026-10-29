#include "k4d.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>
namespace mre::k4d {
namespace {
using Grid=std::vector<std::uint8_t>;
struct World {
 int n; Grid state; std::mt19937_64 rng;
 explicit World(int side,std::uint64_t seed):n(side),state(static_cast<std::size_t>(side)*side*side*side),rng(seed){
  for(auto& v:state)v=static_cast<std::uint8_t>(rng()&1U);
 }
 std::size_t index(int x,int y,int z,int w) const {
  const auto wrap=[this](int a){return (a+n)%n;};
  return static_cast<std::size_t>(((wrap(x)*n+wrap(y))*n+wrap(z))*n+wrap(w));
 }
 Grid advance(bool coupled) const {
  Grid next(state.size());
  for(int x=0;x<n;++x)for(int y=0;y<n;++y)for(int z=0;z<n;++z)for(int w=0;w<n;++w){
   auto p=index(x,y,z,w); unsigned neighbors=0;
   neighbors+=state[index(x+1,y,z,w)]; neighbors+=state[index(x-1,y,z,w)];
   neighbors+=state[index(x,y+1,z,w)]; neighbors+=state[index(x,y-1,z,w)];
   neighbors+=state[index(x,y,z+1,w)]; neighbors+=state[index(x,y,z-1,w)];
   neighbors+=state[index(x,y,z,w+1)]; neighbors+=state[index(x,y,z,w-1)];
   // Fixed microphysics, no encoded search operators or mutation templates.
   next[p]=coupled?static_cast<std::uint8_t>(state[p]^(neighbors==3U || neighbors==5U)):state[index(x-1,y,z,w)];
  }
  return next;
 }
};
std::uint64_t fingerprint(const Grid& g){std::uint64_t h=14695981039346656037ULL;for(auto b:g){h^=b;h*=1099511628211ULL;}return h;}
struct Result{std::size_t unique=0, revisits=0;double activity=0, effect=0;};
Result simulate(int n,int steps,std::uint64_t seed,bool coupled){
 World world(n,seed); World perturbed=world; std::map<std::uint64_t,int> seen;
 Result r; for(int t=0;t<steps;++t){
  auto key=fingerprint(world.state);if(++seen[key]>1)++r.revisits;
  if(t==steps/2)perturbed.state[perturbed.index(n/2,n/2,n/2,n/2)]^=1U;
  world.state=world.advance(coupled);perturbed.state=perturbed.advance(coupled);
 }
 r.unique=seen.size();
 r.activity=static_cast<double>(std::count(world.state.begin(),world.state.end(),static_cast<std::uint8_t>(1)))/world.state.size();
 std::size_t diff=0;for(std::size_t i=0;i<world.state.size();++i)diff+=world.state[i]!=perturbed.state[i];
 r.effect=static_cast<double>(diff)/world.state.size();return r;
}
}
Observation observe(int side,int steps,std::uint64_t seed,bool coupled){
 if(side<3||side>32||steps<2||steps>100000)throw std::invalid_argument("invalid K4D observation budget");
 const auto r=simulate(side,steps,seed,coupled);
 World w(side,seed);
 for(int t=0;t<steps;++t)w.state=w.advance(coupled);
 return {fingerprint(w.state),r.activity,r.effect,r.unique,r.revisits};
}

std::vector<DynamicsStep> traceDynamics(int side, int steps, std::uint64_t seed, bool coupled) {
    if (side < 3 || side > 32 || steps < 2 || steps > 100000) {
        throw std::invalid_argument("invalid K4D trace budget");
    }
    World world(side, seed);
    World perturbed = world;
    std::map<std::uint64_t, int> seen;
    std::vector<DynamicsStep> trace;
    trace.reserve(steps);

    const int mid = side / 2;
    const std::size_t total_sec = (static_cast<std::size_t>(side) * side * side * side / 2 == 0)
        ? 1 : (static_cast<std::size_t>(side) * side * side * side / 2);

    for (int t = 0; t < steps; ++t) {
        auto key = fingerprint(world.state);
        bool revisit = (++seen[key] > 1);

        if (t == steps / 2) {
            perturbed.state[perturbed.index(mid, mid, mid, mid)] ^= 1U;
        }

        DynamicsStep step;
        step.fingerprint = key;
        step.is_revisit = revisit;

        std::size_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
        for (int x = 0; x < side; ++x) {
            for (int y = 0; y < side; ++y) {
                for (int z = 0; z < side; ++z) {
                    for (int w = 0; w < side; ++w) {
                        if (world.state[world.index(x, y, z, w)]) {
                            if (x >= mid) ++c0;
                            if (y >= mid) ++c1;
                            if (z >= mid) ++c2;
                            if (w >= mid) ++c3;
                        }
                    }
                }
            }
        }
        step.sector_activity[0] = static_cast<double>(c0) / total_sec;
        step.sector_activity[1] = static_cast<double>(c1) / total_sec;
        step.sector_activity[2] = static_cast<double>(c2) / total_sec;
        step.sector_activity[3] = static_cast<double>(c3) / total_sec;

        std::size_t diff = 0;
        for (std::size_t i = 0; i < world.state.size(); ++i) {
            diff += (world.state[i] != perturbed.state[i]);
        }
        step.perturbation_distance = static_cast<double>(diff) / world.state.size();

        trace.push_back(step);

        world.state = world.advance(coupled);
        perturbed.state = perturbed.advance(coupled);
    }
    return trace;
}

int selftest(){
 World w(4,17);if(w.state.size()!=256U)throw std::runtime_error("4D volume invalid");
 const auto a=w.advance(true);if(a.size()!=w.state.size())throw std::runtime_error("step invalid");
 if(simulate(4,20,12,true).unique!=simulate(4,20,12,true).unique)throw std::runtime_error("nondeterministic");
 const auto control=simulate(4,20,12,false);if(control.activity<0||control.activity>1)throw std::runtime_error("activity invalid");
 std::cout<<"K4D selftest PASS\n";return 0;
}
int run(const std::string& output,int side,int steps,std::uint64_t seed){
 if(side<3||side>32||steps<2||steps>100000)throw std::invalid_argument("side must be 3..32 and steps 2..100000");
 const auto coupled=simulate(side,steps,seed,true),control=simulate(side,steps,seed,false);
 std::filesystem::path p(output);if(p.has_parent_path())std::filesystem::create_directories(p.parent_path());
 std::ofstream f(p);if(!f)throw std::runtime_error("cannot write output");
 f<<"{\n  \"status\": \"EXPLORATORY_NOT_PROVEN\",\n  \"dimensions\": 4,\n  \"side\": "<<side<<",\n  \"steps\": "<<steps<<",\n  \"seed\": "<<seed<<",\n  \"coupled\": {\"unique\": "<<coupled.unique<<", \"revisits\": "<<coupled.revisits<<", \"activity\": "<<coupled.activity<<", \"intervention_effect\": "<<coupled.effect<<"},\n  \"control\": {\"unique\": "<<control.unique<<", \"revisits\": "<<control.revisits<<", \"activity\": "<<control.activity<<", \"intervention_effect\": "<<control.effect<<"},\n  \"new_strategy_proven\": false\n}\n";
 if(!f)throw std::runtime_error("write failed");
 std::cout<<"K4D result: "<<output<<"\n";
 return 0;
}
}
