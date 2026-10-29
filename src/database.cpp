#include "database.hpp"

namespace mre {

Db::Db(const fs::path& path, bool readonly) : readonly_(readonly) {
    if (!readonly) {
        if (path.has_parent_path()) {
            fs::create_directories(path.parent_path());
        }
    }

#ifdef _WIN32
    if (readonly) {
        std::string p = utf8(fs::absolute(path));
        // Use read-only flags
        constexpr int SQLITE_OPEN_READONLY_FLAG = 0x00000001;
        check(sqlite3_open_v2(p.c_str(), &handle_, SQLITE_OPEN_READONLY_FLAG, nullptr) == SQLITE_OK_,
              "Cannot open database in read-only mode: " + p);
    } else {
        auto wide = path.wstring();
        check(sqlite3_open16(wide.c_str(), &handle_) == SQLITE_OK_,
              "Cannot open/create database: " + utf8(path));
    }
#else
    if (readonly) {
        std::string uri = "file:" + utf8(fs::absolute(path)) + "?mode=ro";
        check(sqlite3_open_v2(uri.c_str(), &handle_, 0x00000001 | 0x00000040, nullptr) == SQLITE_OK_,
              "Cannot open database in read-only mode");
    } else {
        check(sqlite3_open(utf8(path).c_str(), &handle_) == SQLITE_OK_,
              "Cannot open/create database");
    }
#endif

    sqlite3_busy_timeout(handle_, 10000);

    if (readonly) {
        exec("PRAGMA query_only = ON;");
    } else {
        exec("PRAGMA journal_mode = WAL;");
        exec("PRAGMA synchronous = NORMAL;");
        exec("PRAGMA busy_timeout = 10000;");
    }
}

Db::~Db() {
    if (handle_) {
        sqlite3_close(handle_);
        handle_ = nullptr;
    }
}

void Db::exec(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK_) {
        std::string msg = err ? err : sqlite3_errmsg(handle_);
        throw std::runtime_error("SQLite exec error: " + msg + " in query: " + sql);
    }
}

void Db::beginTransaction() {
    exec("BEGIN IMMEDIATE TRANSACTION;");
}

void Db::commit() {
    exec("COMMIT;");
}

void Db::rollback() {
    exec("ROLLBACK;");
}

int64_t Db::count(const std::string& sql) {
    Stmt s(*this, sql);
    if (s.step()) {
        return s.getInt64(0);
    }
    return 0;
}

int Db::changes() const {
    return sqlite3_changes(handle_);
}

Stmt::Stmt(Db& db, const std::string& sql) : db_(db.get()) {
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr);
    if (rc != SQLITE_OK_) {
        throw std::runtime_error("SQLite prepare error: " + std::string(sqlite3_errmsg(db_)) + " in: " + sql);
    }
}

Stmt::~Stmt() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}

void Stmt::bind(int col, const std::string& val) {
    check(sqlite3_bind_text(stmt_, col, val.c_str(), static_cast<int>(val.size()), nullptr) == SQLITE_OK_,
          "Failed to bind string parameter");
}

void Stmt::bind(int col, int64_t val) {
    check(sqlite3_bind_int64(stmt_, col, val) == SQLITE_OK_,
          "Failed to bind int64 parameter");
}

void Stmt::bind(int col, double val) {
    check(sqlite3_bind_double(stmt_, col, val) == SQLITE_OK_,
          "Failed to bind double parameter");
}

void Stmt::bindNull(int col) {
    check(sqlite3_bind_null(stmt_, col) == SQLITE_OK_,
          "Failed to bind NULL parameter");
}

bool Stmt::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW_) return true;
    if (rc == SQLITE_DONE_) return false;
    throw std::runtime_error("SQLite step error: " + std::string(sqlite3_errmsg(db_)));
}

void Stmt::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

std::string Stmt::getText(int col) {
    const unsigned char* p = sqlite3_column_text(stmt_, col);
    return p ? reinterpret_cast<const char*>(p) : "";
}

int64_t Stmt::getInt64(int col) {
    return sqlite3_column_int64(stmt_, col);
}

double Stmt::getDouble(int col) {
    return sqlite3_column_double(stmt_, col);
}

bool Stmt::isNull(int col) {
    return sqlite3_column_type(stmt_, col) == SQLITE_NULL_;
}

static void ensureColumn(Db& db, const std::string& table, const std::string& column, const std::string& colDef) {
    std::string sql = "PRAGMA table_info(" + table + ");";
    Stmt s(db, sql);
    bool exists = false;
    while (s.step()) {
        if (s.getText(1) == column) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        db.exec("ALTER TABLE " + table + " ADD COLUMN " + column + " " + colDef + ";");
    }
}

void initializeOrMigrateDiscoveryDb(Db& db) {
    db.exec("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY, description TEXT, applied_at TEXT);");

    // Version 1 / 2 baseline candidates table
    db.exec("CREATE TABLE IF NOT EXISTS candidates ("
            "id INTEGER PRIMARY KEY, "
            "seed INTEGER NOT NULL, "
            "ordinal INTEGER NOT NULL, "
            "mode TEXT NOT NULL, "
            "source_a TEXT NOT NULL, "
            "source_b TEXT NOT NULL, "
            "file_a TEXT NOT NULL, "
            "file_b TEXT NOT NULL, "
            "proposal TEXT NOT NULL, "
            "status TEXT NOT NULL, "
            "details TEXT NOT NULL DEFAULT '', "
            "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
            ");");

    // Upgrade schema to Version 3 columns
    ensureColumn(db, "candidates", "strategy", "TEXT NOT NULL DEFAULT 'random'");
    ensureColumn(db, "candidates", "type_sig_a", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(db, "candidates", "type_sig_b", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(db, "candidates", "hypothesis_code", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(db, "candidates", "proof_code", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(db, "candidates", "premises_status", "TEXT NOT NULL DEFAULT 'NONE'");
    ensureColumn(db, "candidates", "unresolved_premises", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(db, "candidates", "counterexample", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(db, "candidates", "error_log", "TEXT NOT NULL DEFAULT ''");
    ensureColumn(db, "candidates", "verification_exit_code", "INTEGER DEFAULT -1");
    ensureColumn(db, "candidates", "verification_duration_ms", "INTEGER DEFAULT 0");
    ensureColumn(db, "candidates", "parent_id", "INTEGER DEFAULT 0");
    ensureColumn(db, "candidates", "chain_depth", "INTEGER DEFAULT 0");
    ensureColumn(db, "candidates", "updated_at", "TEXT DEFAULT ''");

    // Migrate any rows where hypothesis_code is empty but proposal is set
    db.exec("UPDATE candidates SET hypothesis_code = proposal WHERE hypothesis_code = '' AND proposal != '';");

    db.exec("CREATE INDEX IF NOT EXISTS idx_candidates_status ON candidates(status);");
    db.exec("CREATE INDEX IF NOT EXISTS idx_candidates_strategy ON candidates(strategy);");

    // Graph edges table
    db.exec("CREATE TABLE IF NOT EXISTS graph_edges ("
            "source TEXT NOT NULL, "
            "target TEXT NOT NULL, "
            "weight INTEGER NOT NULL, "
            "PRIMARY KEY(source, target)"
            ");");

    // Mathematical type graph edges
    db.exec("CREATE TABLE IF NOT EXISTS type_graph_edges ("
            "source TEXT NOT NULL, "
            "target TEXT NOT NULL, "
            "kind TEXT NOT NULL, "
            "weight INTEGER NOT NULL, "
            "metadata TEXT NOT NULL DEFAULT '', "
            "PRIMARY KEY(source, target, kind)"
            ");");
    db.exec("CREATE INDEX IF NOT EXISTS idx_tge_source ON type_graph_edges(source);");

    // Semantic declarations
    db.exec("CREATE TABLE IF NOT EXISTS semantic_declarations ("
            "name TEXT PRIMARY KEY, "
            "kind TEXT NOT NULL, "
            "module TEXT NOT NULL, "
            "file TEXT NOT NULL, "
            "line INTEGER NOT NULL, "
            "type_raw TEXT NOT NULL, "
            "conclusion TEXT NOT NULL, "
            "is_prop INTEGER NOT NULL, "
            "param_count INTEGER NOT NULL, "
            "domain TEXT NOT NULL, "
            "json_meta TEXT NOT NULL, "
            "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
            ");");
    db.exec("CREATE INDEX IF NOT EXISTS idx_sem_domain ON semantic_declarations(domain);");
    db.exec("CREATE INDEX IF NOT EXISTS idx_sem_kind ON semantic_declarations(kind);");

    // Experiments session table
    db.exec("CREATE TABLE IF NOT EXISTS experiments ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "session_name TEXT NOT NULL, "
            "strategy TEXT NOT NULL, "
            "seed INTEGER NOT NULL, "
            "total_candidates INTEGER NOT NULL DEFAULT 0, "
            "proven_count INTEGER NOT NULL DEFAULT 0, "
            "counterexample_count INTEGER NOT NULL DEFAULT 0, "
            "status TEXT NOT NULL, "
            "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
            ");");

    // Detailed verification logs
    db.exec("CREATE TABLE IF NOT EXISTS verifications ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "candidate_id INTEGER NOT NULL, "
            "status TEXT NOT NULL, "
            "exit_code INTEGER NOT NULL, "
            "log_path TEXT NOT NULL, "
            "duration_ms INTEGER NOT NULL, "
            "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
            ");");
    db.exec("CREATE INDEX IF NOT EXISTS idx_verif_cand ON verifications(candidate_id);");

    // Numeric evaluations
    db.exec("CREATE TABLE IF NOT EXISTS numeric_evaluations ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "candidate_id INTEGER NOT NULL, "
            "strategy TEXT NOT NULL, "
            "status TEXT NOT NULL, "
            "counterexample_val TEXT NOT NULL DEFAULT '', "
            "test_points_count INTEGER NOT NULL, "
            "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
            ");");

    // Record schema version 3
    db.exec("INSERT INTO schema_version (version, description, applied_at) "
            "VALUES (3, 'Math Research Engine v0.3 Full Discovery Architecture', CURRENT_TIMESTAMP) "
            "ON CONFLICT(version) DO UPDATE SET applied_at = CURRENT_TIMESTAMP;");
}

} // namespace mre
