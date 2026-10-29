#pragma once

#include "common.hpp"

#ifdef _WIN32
extern "C" {
struct sqlite3;
struct sqlite3_stmt;
int sqlite3_open16(const void*, sqlite3**);
int sqlite3_open_v2(const char*, sqlite3**, int, const char*);
int sqlite3_close(sqlite3*);
int sqlite3_exec(sqlite3*, const char*, int(*)(void*, int, char**, char**), void*, char**);
int sqlite3_prepare_v2(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
int sqlite3_bind_text(sqlite3_stmt*, int, const char*, int, void(*)(void*));
int sqlite3_bind_int64(sqlite3_stmt*, int, long long);
int sqlite3_bind_double(sqlite3_stmt*, int, double);
int sqlite3_bind_null(sqlite3_stmt*, int);
int sqlite3_step(sqlite3_stmt*);
int sqlite3_reset(sqlite3_stmt*);
int sqlite3_clear_bindings(sqlite3_stmt*);
int sqlite3_finalize(sqlite3_stmt*);
const unsigned char* sqlite3_column_text(sqlite3_stmt*, int);
long long sqlite3_column_int64(sqlite3_stmt*, int);
double sqlite3_column_double(sqlite3_stmt*, int);
int sqlite3_column_type(sqlite3_stmt*, int);
const char* sqlite3_errmsg(sqlite3*);
int sqlite3_busy_timeout(sqlite3*, int);
int sqlite3_changes(sqlite3*);
}
#else
#include <sqlite3.h>
#endif

namespace mre {

constexpr int SQLITE_OK_ = 0;
constexpr int SQLITE_ROW_ = 100;
constexpr int SQLITE_DONE_ = 101;
constexpr int SQLITE_NULL_ = 5;

class Db {
    sqlite3* handle_ = nullptr;
    bool readonly_ = false;

public:
    Db(const fs::path& path, bool readonly = false);
    ~Db();
    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    sqlite3* get() const { return handle_; }
    bool isReadOnly() const { return readonly_; }

    void exec(const std::string& sql);
    void beginTransaction();
    void commit();
    void rollback();
    int64_t count(const std::string& sql);
    int changes() const;
};

class Stmt {
    sqlite3_stmt* stmt_ = nullptr;
    sqlite3* db_ = nullptr;

public:
    Stmt(Db& db, const std::string& sql);
    ~Stmt();
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    void bind(int col, const std::string& val);
    void bind(int col, int64_t val);
    void bind(int col, int val) { bind(col, static_cast<int64_t>(val)); }
    void bind(int col, uint32_t val) { bind(col, static_cast<int64_t>(val)); }
    void bind(int col, uint64_t val) { bind(col, static_cast<int64_t>(val)); }
    void bind(int col, double val);
    void bindNull(int col);

    bool step(); // returns true if SQLITE_ROW
    void reset();

    std::string getText(int col);
    int64_t getInt64(int col);
    double getDouble(int col);
    bool isNull(int col);

    sqlite3_stmt* get() { return stmt_; }
};

// Database schema migration
void initializeOrMigrateDiscoveryDb(Db& db);

} // namespace mre
