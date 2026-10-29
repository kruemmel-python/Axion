#pragma once

#include "common.hpp"

namespace mre {

struct DeclParam {
    std::string name;
    std::string type;
    std::string binder; // default, implicit, strictImplicit, instImplicit
    bool is_prop = false;
};

struct SemanticDeclaration {
    std::string name;
    std::string kind; // theorem, lemma, def, abbrev, instance, etc.
    std::string module;
    std::string file;
    int64_t line = 0;
    std::string type_raw;
    std::string conclusion;
    bool is_prop = false;
    int param_count = 0;
    std::string domain;
    std::vector<DeclParam> params;
    std::vector<std::string> premises;
    std::vector<std::string> universe_params;
    std::vector<std::string> constants;
    std::string json_meta;

    bool isFunction() const;
    std::string getDomainType() const; // First explicit parameter type
    std::string getCodomainType() const; // Return/conclusion type
};

// Domain classification from module/file path
std::string extractDomain(const std::string& path);

// Identify certificate / high-volume generated files
bool isCertificateFile(const std::string& path, const std::string& name);

// Lightweight JSON parser for LeanExtractor JSON outputs
SemanticDeclaration parseSemanticDeclarationJson(const std::string& json_str, const std::string& file = "", int64_t line = 0);

// Mathematical type compatibility check
bool areTypesCompatibleForComposition(const SemanticDeclaration& f, const SemanticDeclaration& g);

// Heuristic extractor for declarations from raw database statement text
SemanticDeclaration extractSemanticFromRawStatement(const std::string& file, int64_t line, const std::string& kind, const std::string& name, const std::string& statement);

} // namespace mre
