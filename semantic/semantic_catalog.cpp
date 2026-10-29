#ifdef _WIN32
#include "../memory/win_sqlite_memory_api.hpp"
#else
#include <sqlite3.h>
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
int main(int argc,char** argv){try{
 if(argc!=3){std::cerr<<"Usage: math_semantic_catalog source.sqlite report.csv\n";return 2;}
 const fs::path source=argv[1],dest=argv[2];if(!fs::exists(source))throw std::runtime_error("Source DB missing");
 sqlite3* db=nullptr;const std::string filename=source.string();
 const int openResult=sqlite3_open_v2(filename.c_str(),&db,SQLITE_OPEN_READONLY,nullptr);
 if(openResult!=SQLITE_OK){
  const std::string detail=db ? sqlite3_errmsg(db) : "SQLite handle unavailable";
  if(db)sqlite3_close(db);
  throw std::runtime_error("Read-only SQLite open failed: "+detail+" [path="+filename+"]");
 }
 sqlite3_exec(db,"PRAGMA query_only=ON",nullptr,nullptr,nullptr);
 sqlite3_stmt* st=nullptr;
 if(sqlite3_prepare_v2(db,"SELECT name,sql FROM sqlite_master WHERE type='table' ORDER BY name",-1,&st,nullptr)!=SQLITE_OK){sqlite3_close(db);throw std::runtime_error("Schema inspection failed");}
 if(!dest.parent_path().empty()){fs::create_directories(dest.parent_path());}
 std::ofstream out(dest,std::ios::binary);if(!out)throw std::runtime_error("Cannot create report");out<<"table,ddl\n";unsigned n=0;
 while(sqlite3_step(st)==SQLITE_ROW){const unsigned char* name=sqlite3_column_text(st,0);const unsigned char* ddl=sqlite3_column_text(st,1);auto csv=[](const unsigned char* s){std::string v="\"";if(s)for(const char* p=reinterpret_cast<const char*>(s);*p;++p){if(*p=='\"')v+='\"';v+=*p;}return v+'\"';};out<<csv(name)<<','<<csv(ddl)<<'\n';++n;}
 sqlite3_finalize(st);sqlite3_close(db);if(!out)throw std::runtime_error("Report write failed");std::cout<<"SEMANTIC_CATALOG tables="<<n<<" mode=READ_ONLY no_semantic_claim=true\n";return 0;
 }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
