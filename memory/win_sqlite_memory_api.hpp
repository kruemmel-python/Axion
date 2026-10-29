#pragma once
// Minimal declarations for the Windows SDK winsqlite3 import library.
// Windows SDK commonly provides winsqlite3.lib without sqlite3.h.
// Signatures and constants follow the SQLite C API; no external SQLite install needed.
#ifdef _WIN32
extern "C" {
struct sqlite3;
struct sqlite3_stmt;
int sqlite3_open(const char*, sqlite3**);
int sqlite3_open_v2(const char*, sqlite3**, int, const char*);
const char* sqlite3_errmsg(sqlite3*);
int sqlite3_close(sqlite3*);
int sqlite3_exec(sqlite3*, const char*, int (*)(void*,int,char**,char**), void*, char**);
void sqlite3_free(void*);
int sqlite3_prepare_v2(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
int sqlite3_finalize(sqlite3_stmt*);
int sqlite3_step(sqlite3_stmt*);
int sqlite3_bind_text(sqlite3_stmt*, int, const char*, int, void (*)(void*));
int sqlite3_bind_int(sqlite3_stmt*, int, int);
int sqlite3_bind_double(sqlite3_stmt*, int, double);
int sqlite3_column_int(sqlite3_stmt*, int);
double sqlite3_column_double(sqlite3_stmt*, int);
const unsigned char* sqlite3_column_text(sqlite3_stmt*, int);
}
inline constexpr int SQLITE_OK = 0;
inline constexpr int SQLITE_OPEN_READONLY = 0x00000001;
inline constexpr int SQLITE_ROW = 100;
inline constexpr int SQLITE_DONE = 101;
// The canonical SQLITE_TRANSIENT special destructor pointer, as documented by SQLite.
// Cast via intptr_t so it is valid on both 32-bit and 64-bit Windows.
#define SQLITE_TRANSIENT reinterpret_cast<void(*)(void*)>(static_cast<intptr_t>(-1))
#endif
