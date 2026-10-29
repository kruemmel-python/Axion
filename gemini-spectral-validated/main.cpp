// Gemini candidate, corrected and independently validated Fourier analysis.
// No API calls and no generated code execution in this program.
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace signal42 {
using Complex = std::complex<double>;
constexpr double pi = 3.141592653589793238462643383279502884;

long long mod_pow(long long base, int exp, int mod) {
    if (mod < 2 || exp < 1) throw std::invalid_argument("mod >= 2 and power >= 1 required");
    long long r = 1, b = ((base % mod) + mod) % mod;
    while (exp > 0) {
        if (exp & 1) r = (r * b) % mod;
        b = (b * b) % mod;
        exp >>= 1;
    }
    return r;
}
std::vector<int> generators(int n, int power) {
    std::set<int> s;
    for (int x = 1; x < n; ++x) {
        const int r = static_cast<int>(mod_pow(x, power, n));
        if (r != 0) { s.insert(r); s.insert((n-r)%n); }
    }
    return {s.begin(), s.end()};
}
std::vector<double> representations(int n, int power, int depth) {
    const auto gen = generators(n, power);
    std::vector<double> a(static_cast<std::size_t>(n), 0.0);
    a[0] = 1.0;
    for (int d = 0; d < depth; ++d) {
        std::vector<double> b(static_cast<std::size_t>(n), 0.0);
        for (int u = 0; u < n; ++u)
            for (int g : gen) b[static_cast<std::size_t>((u+g)%n)] += a[static_cast<std::size_t>(u)];
        a.swap(b);
    }
    return a;
}
// Fourier characters diagonalize every Cayley graph of the cyclic group.
// L = degree*I - adjacency. Return exact-formula eigenvalues, not Jacobi bases.
std::vector<double> eigenvalues(int n, const std::vector<int>& gen) {
    std::vector<double> v(static_cast<std::size_t>(n));
    for (int k=0;k<n;++k) {
        double adjacency_eigenvalue=0.0;
        for(int g:gen) adjacency_eigenvalue += std::cos(2.0*pi*k*g/n);
        v[static_cast<std::size_t>(k)] = static_cast<double>(gen.size())-adjacency_eigenvalue;
    }
    return v;
}
std::vector<Complex> dft(const std::vector<double>& signal) {
    const int n = static_cast<int>(signal.size());
    std::vector<Complex> out(static_cast<std::size_t>(n), Complex{});
    const double scale=1.0/std::sqrt(static_cast<double>(n));
    for(int k=0;k<n;++k)
        for(int x=0;x<n;++x) {
            const double angle=-2.0*pi*static_cast<double>(k)*x/n;
            out[static_cast<std::size_t>(k)] += signal[static_cast<std::size_t>(x)]*Complex(std::cos(angle),std::sin(angle))*scale;
        }
    return out;
}
struct Spectral {
    double entropy=0.0;
    double norm_sq=0.0;
    std::size_t distinct_eigenvalues=0;
};
// Group degeneracies by eigenvalue and sum squared Fourier amplitudes.
// The result is energy of orthogonal spectral projectors: basis invariant.
Spectral spectrum(const std::vector<double>& signal, const std::vector<double>& lambda) {
    const auto hat=dft(signal);
    const std::size_t n=hat.size();
    if(lambda.size()!=n) throw std::invalid_argument("eigenvalues mismatch");
    std::vector<std::size_t> sorted(n);
    std::iota(sorted.begin(),sorted.end(),0);
    std::sort(sorted.begin(),sorted.end(),[&](std::size_t a,std::size_t b){return lambda[a]<lambda[b];});
    const double norm=std::inner_product(signal.begin(),signal.end(),signal.begin(),0.0);
    std::vector<double> grouped;
    constexpr double rel_tol=1e-9;
    double previous=0.0;
    bool first=true;
    for(std::size_t ix:sorted) {
        const double e=std::norm(hat[ix]);
        if(first || std::abs(lambda[ix]-previous) > rel_tol*std::max({1.0,std::abs(lambda[ix]),std::abs(previous)})) {
            grouped.push_back(e);
            previous=lambda[ix];
            first=false;
        } else grouped.back() += e;
    }
    Spectral result;
    result.norm_sq=norm;
    result.distinct_eigenvalues=grouped.size();
    if(norm>0.0)
        for(double e:grouped) {
            const double p=e/norm;
            if(p>1e-15) result.entropy-=p*std::log2(p);
        }
    return result;
}
// Return |<initial|exp(-i L t)|initial>|^2 / ||initial||^4.
// In Fourier coordinates this depends only on the projector weights.
double survival(const std::vector<double>& signal, const std::vector<double>& lambda, double time) {
    const auto hat=dft(signal);
    double norm=0.0;
    Complex amp{};
    for(std::size_t k=0;k<hat.size();++k) {
        const double weight=std::norm(hat[k]);
        norm+=weight;
        const double a=-time*lambda[k];
        amp+=weight*Complex(std::cos(a),std::sin(a));
    }
    return norm>0.0?std::norm(amp)/(norm*norm):0.0;
}
struct Result { int target; double count; double entropy; double survival; double legacy_score; };
Result evaluate(int target,const std::vector<double>& rep,const std::vector<double>& lambda) {
    const int n=static_cast<int>(rep.size());
    std::vector<double> delta(static_cast<std::size_t>(n),0.0);
    delta[static_cast<std::size_t>(target)]=rep[static_cast<std::size_t>(target)];
    const auto s=spectrum(delta,lambda);
    const double q=survival(delta,lambda,1.0);
    const double count=rep[static_cast<std::size_t>(target)];
    // For cyclic Cayley graphs, delta_target spectral features are translation invariant.
    // This score therefore adds no independent target-specific information beyond count.
    return {target,count,s.entropy,q,count>0.0?count*(1.0+q)/(1.0+s.entropy):0.0};
}
void require(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
void close(double a,double b,double tol,const char* message) {
    require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol*std::max({1.0,std::abs(a),std::abs(b)}),message);
}
void selftest() {
    require(mod_pow(2,10,1000)==24,"modular exponentiation");
    for(int n : {7,11,17,29}) {
        const auto gen=generators(n,3);
        const auto lambda=eigenvalues(n,gen);
        auto rep=representations(n,3,3);
        close(std::accumulate(rep.begin(),rep.end(),0.0),std::pow(static_cast<double>(gen.size()),3),1e-12,"representation mass");
        // Independently verify Laplacian * Fourier character = lambda * character.
        for(int k=0;k<n;++k) for(int x=0;x<n;++x) {
            const Complex u=std::polar(1.0,2.0*pi*k*x/n);
            Complex lu=static_cast<double>(gen.size())*u;
            for(int g:gen) lu-=std::polar(1.0,2.0*pi*k*((x+g)%n)/n);
            require(std::abs(lu-lambda[static_cast<std::size_t>(k)]*u)<1e-9,"spectral eigenvalue residual");
        }
        std::vector<double> input(static_cast<std::size_t>(n));
        for(int x=0;x<n;++x) input[static_cast<std::size_t>(x)]=std::sin(x*0.6)+x*0.12;
        auto shifted=input;
        std::rotate(shifted.begin(),shifted.begin()+1,shifted.end());
        const auto sa=spectrum(input,lambda), sb=spectrum(shifted,lambda);
        close(sa.entropy,sb.entropy,1e-10,"translation invariance of grouped spectral entropy");
        close(sa.norm_sq,sb.norm_sq,1e-10,"Parseval norm");
        close(survival(input,lambda,1.0),survival(shifted,lambda,1.0),1e-10,"translation invariance of survival");
        // Delta at every vertex must have exactly equal basis-invariant spectral data.
        const auto baseline=evaluate(0,rep,lambda);
        for(int t=1;t<n;++t) {
            const auto r=evaluate(t,rep,lambda);
            if(baseline.count>0.0&&r.count>0.0) {
                close(baseline.entropy,r.entropy,1e-10,"delta entropy invariant under shift");
                close(baseline.survival,r.survival,1e-10,"delta survival invariant under shift");
            }
        }
    }
    // Explicit repeated eigenvalue: complete Cayley graph on Z_101.
    const int n=101;
    const auto gen=generators(n,3);
    require(gen.size()==100,"expected complete graph at modulus 101");
    const auto lambda=eigenvalues(n,gen);
    const auto rep=representations(n,3,3);
    for(int t=0;t<n;++t) {
        const auto r=evaluate(t,rep,lambda);
        close(r.count,t==0?9900.0:9901.0,1e-12,"expected complete-graph representation counts");
        const auto r0=evaluate(0,rep,lambda);
        close(r.entropy,r0.entropy,1e-10,"degenerate eigenspace entropy should be target invariant");
        close(r.survival,r0.survival,1e-10,"degenerate eigenspace survival should be target invariant");
        if(t!=0) close(r.legacy_score,evaluate(1,rep,lambda).legacy_score,1e-10,"equal counts imply equal scores");
    }
    std::cout<<"SELFTEST PASS: eigen-residual, Parseval, translation, degeneracy, count invariants\n";
}
int parse_int(const char* s,const char* name) {
    const std::string v=s;
    std::size_t p=0;
    const long long x=std::stoll(v,&p);
    if(p!=v.size() || x<std::numeric_limits<int>::min()||x>std::numeric_limits<int>::max()) throw std::invalid_argument(name);
    return static_cast<int>(x);
}
} // namespace signal42
int main(int argc,char** argv) {
    try {
        int n=101,power=3,target=42,depth=3;
        bool scan=false,test=false;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--selftest") test=true;
            else if(arg=="--scan") scan=true;
            else if((arg=="--modulus"||arg=="--power"||arg=="--target"||arg=="--depth")&&i+1<argc) {
                const int x=signal42::parse_int(argv[++i],arg.c_str());
                if(arg=="--modulus") n=x;
                else if(arg=="--power") power=x;
                else if(arg=="--target") target=x;
                else depth=x;
            } else throw std::invalid_argument("unknown or incomplete option: "+arg);
        }
        if(test) {signal42::selftest();return 0;}
        if(n<2 || n>512 || power<1 || power>63 || depth<0 || depth>8)
            throw std::invalid_argument("allowed: modulus 2..512, power 1..63, depth 0..8");
        target=(target%n+n)%n;
        const auto gen=signal42::generators(n,power);
        const auto lambda=signal42::eigenvalues(n,gen);
        const auto rep=signal42::representations(n,power,depth);
        const auto r=signal42::evaluate(target,rep,lambda);
        const auto zero=signal42::spectrum(std::vector<double>(static_cast<std::size_t>(n),1.0),lambda);
        std::cout<<std::setprecision(12)
                 <<"SIGNAL 42 - BASIS-INVARIANT SPECTRAL ANALYSIS\n"
                 <<"modulus="<<n<<" power="<<power<<" target="<<target<<" depth="<<depth<<"\n"
                 <<"generators="<<gen.size()<<" distinct_eigenvalues="<<zero.distinct_eigenvalues<<"\n"
                 <<"count="<<r.count<<" grouped_spectral_entropy="<<r.entropy
                 <<" quantum_survival="<<r.survival<<" legacy_score="<<r.legacy_score<<"\n"
                 <<"NOTE: for delta targets of a cyclic Cayley graph entropy and survival are translation invariant.\n"
                 <<"NOTE: legacy_score is a scaled count; it is NOT an independently validated resonance.\n";
        if(scan) {
            std::vector<signal42::Result> result;
            for(int t=0;t<n;++t) result.push_back(signal42::evaluate(t,rep,lambda));
            std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){
                if(a.count!=b.count) return a.count>b.count;
                return a.target<b.target; // stable, transparent tie break
            });
            std::cout<<"SCAN (ranked by representation count; ties by target ID):\n";
            for(std::size_t i=0;i<std::min<std::size_t>(10,result.size());++i)
                std::cout<<i+1<<": target="<<result[i].target<<" count="<<result[i].count<<"\n";
        }
    } catch(const std::exception& ex) { std::cerr<<"ERROR: "<<ex.what()<<'\n';return 1; }
    return 0;
}
