#ifdef _WIN32
#include <cstdint>
#include "win_sqlite_memory_api.hpp"
#else
#include <sqlite3.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace {
void require(bool ok,const std::string& msg){if(!ok)throw std::runtime_error(msg);}
struct Database{
 sqlite3* p=nullptr;
 explicit Database(const fs::path& path){fs::create_directories(path.parent_path());require(sqlite3_open(path.string().c_str(),&p)==SQLITE_OK,"sqlite open failed");exec("PRAGMA journal_mode=WAL;");exec("PRAGMA foreign_keys=ON;");exec("PRAGMA busy_timeout=5000;");exec("CREATE TABLE IF NOT EXISTS experiments(id INTEGER PRIMARY KEY,namespace TEXT NOT NULL,fingerprint TEXT NOT NULL,parents TEXT NOT NULL DEFAULT '',task TEXT NOT NULL,operator_id INTEGER NOT NULL,selection REAL NOT NULL,holdout REAL NOT NULL,baseline REAL NOT NULL,verdict TEXT NOT NULL,run_tag TEXT NOT NULL,created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),UNIQUE(namespace,fingerprint,task));");exec("CREATE INDEX IF NOT EXISTS experiments_task ON experiments(namespace,task,operator_id);");exec("CREATE TABLE IF NOT EXISTS runs(run_tag TEXT PRIMARY KEY,namespace TEXT NOT NULL,task TEXT NOT NULL,mode TEXT NOT NULL,budget INTEGER NOT NULL,trials INTEGER NOT NULL,best REAL NOT NULL,baseline REAL NOT NULL,verdict TEXT NOT NULL,created_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')));");}
 ~Database(){if(p)sqlite3_close(p);}
 void exec(const char* sql){char* err=nullptr;int rc=sqlite3_exec(p,sql,nullptr,nullptr,&err);if(rc!=SQLITE_OK){std::string message=err?err:"sqlite error";sqlite3_free(err);throw std::runtime_error(message);}}
 Database(const Database&)=delete;Database& operator=(const Database&)=delete;
};
struct Statement{sqlite3_stmt* p=nullptr;Statement(sqlite3* db,const char* sql){require(sqlite3_prepare_v2(db,sql,-1,&p,nullptr)==SQLITE_OK,"sqlite prepare");}~Statement(){sqlite3_finalize(p);}Statement(const Statement&)=delete;Statement& operator=(const Statement&)=delete;};
void bindText(sqlite3_stmt* st,int n,const std::string& s){require(sqlite3_bind_text(st,n,s.c_str(),-1,SQLITE_TRANSIENT)==SQLITE_OK,"bind text");}
std::string id(unsigned op,unsigned param){return "operator="+std::to_string(op)+";parameter="+std::to_string(param)+";protocol=1";}
struct Candidate{unsigned op=0,param=0;double selection=0,holdout=0;std::string fingerprint;};
// Public benchmark: training/validation are deterministic and independent of the final holdout.
// The objective is prediction of a synthetic target; NOT proof of novelty or human usefulness.
double operation(unsigned op,unsigned p,double a,double b){const double k=static_cast<double>(p+1)/12.0;switch(op){case 0:return a;case 1:return b;case 2:return (a+b)*0.5;case 3:return std::abs(a-b);case 4:return std::max(a,b);case 5:return std::min(a,b);case 6:return a*k+b*(1.0-k);case 7:return std::sin(a*3.14159265358979323846)*k+b*(1.0-k);default:return (a*a+b*b)*0.5;}}
double evaluate(unsigned op,unsigned p,unsigned seed){std::mt19937 rng(seed);std::uniform_real_distribution<double> dist(0.0,1.0);double mse=0;constexpr unsigned count=384;for(unsigned i=0;i<count;++i){double a=dist(rng),b=dist(rng);double truth=(0.7*a+0.3*b);double value=operation(op,p,a,b);mse+=(value-truth)*(value-truth);}return -mse/static_cast<double>(count);}
constexpr unsigned nops=9,nparams=12;
struct Result{std::string tag;unsigned trials=0,skipped=0;double best=-1e9,baseline=0;Candidate champion;bool gain=false;};
std::string ancestors(Database& db,const std::string& ns,const std::string& task){Statement st(db.p,"SELECT fingerprint FROM experiments WHERE namespace=?1 AND task=?2 ORDER BY holdout DESC LIMIT 1");bindText(st.p,1,ns);bindText(st.p,2,task);if(sqlite3_step(st.p)==SQLITE_ROW){const auto* raw=sqlite3_column_text(st.p,0);return raw?reinterpret_cast<const char*>(raw):"";}return "";}
std::array<double,nops> prior(Database& db,const std::string& ns,const std::string& task){std::array<double,nops> values{};values.fill(-1e9);Statement st(db.p,"SELECT operator_id,MAX(selection) FROM experiments WHERE namespace=?1 AND task=?2 GROUP BY operator_id");bindText(st.p,1,ns);bindText(st.p,2,task);while(sqlite3_step(st.p)==SQLITE_ROW){int op=sqlite3_column_int(st.p,0);if(op>=0&&op<static_cast<int>(nops))values[static_cast<unsigned>(op)]=sqlite3_column_double(st.p,1);}return values;}
void writeTrial(Database& db,const std::string& ns,const std::string& task,const Candidate& c,double baseline,const std::string& parent,const std::string& tag){Statement st(db.p,"INSERT OR IGNORE INTO experiments(namespace,fingerprint,parents,task,operator_id,selection,holdout,baseline,verdict,run_tag) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)");bindText(st.p,1,ns);bindText(st.p,2,c.fingerprint);bindText(st.p,3,parent);bindText(st.p,4,task);require(sqlite3_bind_int(st.p,5,static_cast<int>(c.op))==SQLITE_OK,"bind op");require(sqlite3_bind_double(st.p,6,c.selection)==SQLITE_OK,"bind score");require(sqlite3_bind_double(st.p,7,c.holdout)==SQLITE_OK,"bind holdout");require(sqlite3_bind_double(st.p,8,baseline)==SQLITE_OK,"bind baseline");bindText(st.p,9,c.holdout>baseline+0.0001?"GAIN_ON_SYNTHETIC_HOLDOUT":"NO_GAIN");bindText(st.p,10,tag);require(sqlite3_step(st.p)==SQLITE_DONE,"write experiment");}
Result run(Database& db,const std::string& ns,const std::string& task,unsigned budget,bool memory,const std::string& tag){require(budget>0&&budget<=nops*nparams,"budget outside 1..108");double baseline=evaluate(0,0,730001);auto ratings=prior(db,ns,task);std::vector<unsigned> operators(nops);std::iota(operators.begin(),operators.end(),0);if(memory){std::stable_sort(operators.begin(),operators.end(),[&](unsigned a,unsigned b){return ratings[a]>ratings[b];});}
 std::vector<std::pair<unsigned,unsigned>> proposals;for(unsigned op:operators){for(unsigned p=0;p<nparams;++p)proposals.emplace_back(op,p);}if(!memory){std::mt19937 rng(717);std::shuffle(proposals.begin(),proposals.end(),rng);}else{std::stable_sort(proposals.begin(),proposals.end(),[&](const auto& a,const auto& b){return ratings[a.first]>ratings[b.first];});}
 Result result;result.tag=tag;result.baseline=baseline;const std::string parent=memory?ancestors(db,ns,task):"";
 db.exec("BEGIN IMMEDIATE;");try{for(unsigned i=0;i<budget;++i){const auto [op,p]=proposals[i];Candidate c{op,p,evaluate(op,p,130001),0,id(op,p)};if(c.selection>result.best){result.best=c.selection;result.champion=c;}++result.trials; // Holdout evaluated ONLY for the selected winner after search.
 // Persist selection observations; holdout for each trial is intentionally NULL-equivalent marker.
 Candidate stored=c;stored.holdout=-1e9;writeTrial(db,ns,task,stored,baseline,parent,tag);
 }result.champion.holdout=evaluate(result.champion.op,result.champion.param,730001);result.gain=result.champion.holdout>baseline+0.0001;
 Statement st(db.p,"INSERT INTO runs(run_tag,namespace,task,mode,budget,trials,best,baseline,verdict) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)");bindText(st.p,1,tag);bindText(st.p,2,ns);bindText(st.p,3,task);bindText(st.p,4,memory?"memory":"cold");sqlite3_bind_int(st.p,5,static_cast<int>(budget));sqlite3_bind_int(st.p,6,static_cast<int>(result.trials));sqlite3_bind_double(st.p,7,result.champion.holdout);sqlite3_bind_double(st.p,8,baseline);bindText(st.p,9,result.gain?"GAIN_ON_SYNTHETIC_HOLDOUT":"NO_GAIN");require(sqlite3_step(st.p)==SQLITE_DONE,"run insert");db.exec("COMMIT;");}catch(...){db.exec("ROLLBACK;");throw;}return result;}
void print(const Result& r){std::cout<<std::setprecision(9)<<"RUN "<<r.tag<<" trials="<<r.trials<<" selected="<<r.champion.fingerprint<<" validation="<<r.best<<" holdout="<<r.champion.holdout<<" baseline="<<r.baseline<<" verdict="<<(r.gain?"GAIN_ON_SYNTHETIC_HOLDOUT":"NO_GAIN")<<'\n';}
void selftest(const fs::path& path){fs::remove(path);Database db(path);const auto a=run(db,"selftest","blend",12,false,"cold_test");const auto b=run(db,"selftest","blend",12,true,"memory_test");require(a.trials==12&&b.trials==12,"trial counts");require(std::isfinite(a.champion.holdout)&&std::isfinite(b.champion.holdout),"scores");const double score=b.champion.holdout;{Database reopened(path);Statement s(reopened.p,"SELECT COUNT(*) FROM experiments");require(sqlite3_step(s.p)==SQLITE_ROW&&sqlite3_column_int(s.p,0)>0,"persistence");require(!ancestors(reopened,"selftest","blend").empty(),"ancestry persistence");}require(std::abs(score-b.champion.holdout)<1e-15,"deterministic");print(a);print(b);std::cout<<"RESEARCH_MEMORY_SELFTEST PASS\n";}
}
int main(int argc,char** argv){try{if(argc==3&&std::string(argv[1])=="selftest"){selftest(argv[2]);return 0;}if(argc==7&&std::string(argv[1])=="run"){Database db(argv[2]);const std::string mode=argv[4];require(mode=="cold"||mode=="memory","mode cold or memory");unsigned budget=static_cast<unsigned>(std::stoul(argv[5]));auto r=run(db,"research_v7",argv[3],budget,mode=="memory",argv[6]);print(r);return 0;}std::cerr<<"Usage: math_research_memory selftest path.sqlite | run path.sqlite task cold|memory budget unique_run_tag\n";return 2;}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
