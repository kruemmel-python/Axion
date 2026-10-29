#include "semantic_types.hpp"
#include <regex>

namespace mre {

std::string extractDomain(const std::string& path) {
    std::string s = path;
    for (char& c : s) if (c == '\\') c = '/';

    const std::vector<std::string> domains = {
        "AlgebraicGeometry", "AlgebraicTopology", "CategoryTheory", "LinearAlgebra",
        "GroupTheory", "NumberTheory", "MeasureTheory", "ModelTheory",
        "Probability", "Computability", "InformationTheory", "Optimization",
        "SetTheory", "RingTheory", "Dynamics", "GameTheory",
        "Algebra", "Analysis", "Combinatorics", "Geometry", "Topology"
    };

    for (const auto& d : domains) {
        if (s.find("OAI/" + d) != std::string::npos || s.find("/" + d + "/") != std::string::npos) {
            return d;
        }
    }
    if (s.find("Mathlib/") != std::string::npos) return "Mathlib";
    if (s.find("Init/") != std::string::npos || s.find("Std/") != std::string::npos) return "Core";
    return "General";
}

bool isCertificateFile(const std::string& path, const std::string& name) {
    std::string s = path + "/" + name;
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (s.find("certificate") != std::string::npos) return true;
    if (s.find("ramsey") != std::string::npos && s.find("cycleclique") != std::string::npos) return true;
    if (s.find("nuclearultrapower") != std::string::npos) return true;
    if (s.find("planarpacking/certificate") != std::string::npos) return true;
    if (name.find("_decode_entry_") != std::string::npos) return true;
    if (name.find("_templates_item_") != std::string::npos) return true;
    return false;
}

bool SemanticDeclaration::isFunction() const {
    if (kind == "def" || kind == "abbrev") return true;
    for (const auto& p : params) {
        if (p.binder == "default" && !p.is_prop) return true;
    }
    return type_raw.find("→") != std::string::npos || type_raw.find("->") != std::string::npos;
}

std::string SemanticDeclaration::getDomainType() const {
    for (const auto& p : params) {
        if (p.binder == "default" && !p.is_prop) {
            return trim(p.type);
        }
    }
    return "";
}

std::string SemanticDeclaration::getCodomainType() const {
    if (!conclusion.empty()) {
        return trim(conclusion);
    }
    return "";
}

static std::string normalizeType(std::string s) {
    s = trim(s);
    // remove parentheses if outer
    if (s.size() >= 2 && s.front() == '(' && s.back() == ')') {
        s = s.substr(1, s.size() - 2);
    }
    // collapse spaces
    std::string res;
    bool in_space = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!in_space) { res += ' '; in_space = true; }
        } else {
            res += c;
            in_space = false;
        }
    }
    return trim(res);
}

bool areTypesCompatibleForComposition(const SemanticDeclaration& f, const SemanticDeclaration& g) {
    std::string f_out = normalizeType(f.getCodomainType());
    std::string g_in = normalizeType(g.getDomainType());

    if (f_out.empty() || g_in.empty()) return false;

    // Direct match
    if (f_out == g_in) return true;

    // Polymorphic type variables match (e.g. α, β, T, R, Type)
    if (g_in == "α" || g_in == "β" || g_in == "T" || g_in == "Type" || g_in == "Type*" || g_in == "R") {
        return true;
    }
    if (f_out == "α" || f_out == "β" || f_out == "T") {
        return true;
    }

    // Number types compatibility (e.g. Nat -> Nat, Int -> Int, Real -> Real)
    if ((f_out == "Nat" || f_out == "ℕ") && (g_in == "Nat" || g_in == "ℕ")) return true;
    if ((f_out == "Int" || f_out == "ℤ") && (g_in == "Int" || g_in == "ℤ")) return true;
    if ((f_out == "Real" || f_out == "ℝ") && (g_in == "Real" || g_in == "ℝ")) return true;

    return false;
}

// Simple JSON field extractor for LeanExtractor output
static std::string extractJsonString(const std::string& json, const std::string& field) {
    std::string key = "\"" + field + "\":\"";
    auto pos = json.find(key);
    if (pos == std::string::npos) return "";
    pos += key.size();
    std::string val;
    bool esc = false;
    while (pos < json.size()) {
        char c = json[pos++];
        if (esc) {
            if (c == 'n') val += '\n';
            else if (c == 't') val += '\t';
            else val += c;
            esc = false;
        } else if (c == '\\') {
            esc = true;
        } else if (c == '"') {
            break;
        } else {
            val += c;
        }
    }
    return val;
}

static bool extractJsonBool(const std::string& json, const std::string& field) {
    std::string key = "\"" + field + "\":";
    auto pos = json.find(key);
    if (pos == std::string::npos) return false;
    pos += key.size();
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    return json.compare(pos, 4, "true") == 0;
}

SemanticDeclaration parseSemanticDeclarationJson(const std::string& json_str, const std::string& file, int64_t line) {
    SemanticDeclaration d;
    d.json_meta = json_str;
    d.file = file;
    d.line = line;
    d.name = extractJsonString(json_str, "name");
    d.kind = extractJsonString(json_str, "kind");
    d.type_raw = extractJsonString(json_str, "type_raw");
    d.conclusion = extractJsonString(json_str, "conclusion");
    d.is_prop = extractJsonBool(json_str, "is_prop");
    d.domain = extractDomain(file);

    // Parse params array if present
    auto pPos = json_str.find("\"params\":[");
    if (pPos != std::string::npos) {
        pPos += 10;
        auto endPos = json_str.find("],", pPos);
        if (endPos != std::string::npos) {
            std::string paramsSub = json_str.substr(pPos, endPos - pPos);
            size_t cur = 0;
            while (cur < paramsSub.size()) {
                auto objStart = paramsSub.find('{', cur);
                if (objStart == std::string::npos) break;
                auto objEnd = paramsSub.find('}', objStart);
                if (objEnd == std::string::npos) break;
                std::string item = paramsSub.substr(objStart, objEnd - objStart + 1);
                DeclParam p;
                p.name = extractJsonString(item, "name");
                p.type = extractJsonString(item, "type");
                p.binder = extractJsonString(item, "binder");
                p.is_prop = extractJsonBool(item, "is_prop");
                d.params.push_back(p);
                if (p.is_prop && p.binder == "default") {
                    d.premises.push_back(p.type);
                }
                cur = objEnd + 1;
            }
        }
    }
    d.param_count = static_cast<int>(d.params.size());
    return d;
}

SemanticDeclaration extractSemanticFromRawStatement(const std::string& file, int64_t line, const std::string& kind, const std::string& name, const std::string& statement) {
    SemanticDeclaration d;
    d.file = file;
    d.line = line;
    d.kind = kind;
    d.name = name;
    d.domain = extractDomain(file);
    d.type_raw = statement;

    std::string s = statement;
    // Strip trailing ":=" or "by" or "where"
    auto assignPos = s.find(":=");
    if (assignPos != std::string::npos) s = s.substr(0, assignPos);
    auto byPos = s.find(" by");
    if (byPos != std::string::npos) s = s.substr(0, byPos);
    s = trim(s);

    // Find return type colon: skip colons inside binders (...), [...], {...}
    int depthP = 0, depthB = 0, depthC = 0;
    size_t colonPos = std::string::npos;
    for (size_t i = 0; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '(') depthP++;
        else if (ch == ')') { if (depthP > 0) depthP--; }
        else if (ch == '[') depthB++;
        else if (ch == ']') { if (depthB > 0) depthB--; }
        else if (ch == '{') depthC++;
        else if (ch == '}') { if (depthC > 0) depthC--; }
        else if (ch == ':' && depthP == 0 && depthB == 0 && depthC == 0) {
            // Check it's not ":="
            if (i + 1 < s.size() && s[i + 1] != '=') {
                colonPos = i;
            }
        }
    }

    if (colonPos != std::string::npos && colonPos + 1 < s.size()) {
        d.conclusion = trim(s.substr(colonPos + 1));
        s = s.substr(0, colonPos);
    } else {
        d.conclusion = "";
    }

    if (kind == "theorem" || kind == "lemma") {
        d.is_prop = true;
    } else {
        d.is_prop = (d.conclusion == "Prop" || d.conclusion.find("Prop") != std::string::npos);
    }

    // Parse parameters across (), {}, []
    std::regex paramRegex(R"(([\(\{\[])([^:\]\}\)]+)\s*:\s*([^\]\}\)]+)([\)\}\]]))");
    auto it = std::sregex_iterator(s.begin(), s.end(), paramRegex);
    auto end = std::sregex_iterator();
    while (it != end) {
        std::smatch match = *it;
        char openB = match[1].str()[0];
        std::string pNames = trim(match[2].str());
        std::string pType = trim(match[3].str());
        std::string binder = (openB == '{') ? "implicit" : ((openB == '[') ? "instImplicit" : "default");

        // Split multiple variable names in single binder: e.g. (x y : Nat)
        std::istringstream varStream(pNames);
        std::string singleName;
        while (varStream >> singleName) {
            DeclParam p;
            p.name = singleName;
            p.type = pType;
            p.binder = binder;
            p.is_prop = (pType.find("≤") != std::string::npos ||
                         pType.find("=") != std::string::npos ||
                         pType.find("<") != std::string::npos ||
                         pType.find("Prop") != std::string::npos ||
                         pType.find("∈") != std::string::npos ||
                         singleName.rfind("h", 0) == 0);
            d.params.push_back(p);
            if (p.is_prop && p.binder == "default") {
                d.premises.push_back(p.type);
            }
        }
        ++it;
    }

    d.param_count = static_cast<int>(d.params.size());
    return d;
}

} // namespace mre
