#pragma once
// SQLite functions exported by the Windows system library winsqlite3.lib.
// Windows SDK installations may provide the import library but omit winsqlite3.h.
// Keep declarations aligned with the already-working engine's Windows path.
#ifdef _WIN32
extern "C" {
struct sqlite3;
struct sqlite3_stmt;
int sqlite3_open_v2(const char*, sqlite3**, int, const char*);
int sqlite3_close(sqlite3*);
int sqlite3_busy_timeout(sqlite3*, int);
int sqlite3_prepare_v2(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
int sqlite3_bind_text(sqlite3_stmt*, int, const char*, int, void(*)(void*));
int sqlite3_bind_int(sqlite3_stmt*, int, int);
int sqlite3_bind_int64(sqlite3_stmt*, int, long long);
int sqlite3_exec(sqlite3*, const char*, int(*)(void*,int,char**,char**), void*, char**);
int sqlite3_column_count(sqlite3_stmt*);
const char* sqlite3_column_name(sqlite3_stmt*, int);
const unsigned char* sqlite3_column_text(sqlite3_stmt*, int);
int sqlite3_column_int(sqlite3_stmt*, int);
double sqlite3_column_double(sqlite3_stmt*, int);
int sqlite3_step(sqlite3_stmt*);
int sqlite3_finalize(sqlite3_stmt*);
const char* sqlite3_errmsg(sqlite3*);
}
// These SQLite C API result codes are stable across SQLite versions.
inline constexpr int SQLITE_OK = 0;
inline constexpr int SQLITE_ROW = 100;
inline constexpr int SQLITE_DONE = 101;
inline constexpr int SQLITE_OPEN_READONLY = 0x00000001;
inline constexpr int SQLITE_OPEN_READWRITE = 0x00000002;
// sqlite3_bind_text() is used only while the source string is still alive.
// SQLITE_STATIC (nullptr) is safe because query() finalizes the statement
// before its argument's lifetime ends.
#endif
