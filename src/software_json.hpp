#pragma once
#include <cctype>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace mre::software_json {
struct Value {
    enum class Kind { null_value, string_value, object_value, array_value, object = object_value, array = array_value, other_value } kind = Kind::null_value;
    std::string text;
    std::map<std::string, Value> object;
    std::vector<Value> array;
    const Value& at(const std::string& key) const {
        if (kind != Kind::object_value) throw std::runtime_error("JSON: expected object for " + key);
        auto it=object.find(key);
        if(it==object.end()) throw std::runtime_error("JSON: missing key " + key);
        return it->second;
    }
    std::string str() const { if(kind!=Kind::string_value) throw std::runtime_error("JSON: expected string"); return text; }
};
class Parser {
    const std::string& data_; size_t p_=0;
    void ws(){while(p_<data_.size()&&std::isspace(static_cast<unsigned char>(data_[p_])))++p_;}
    char get(){if(p_>=data_.size())throw std::runtime_error("JSON: truncated");return data_[p_++];}
    void expect(char c){ws();if(get()!=c)throw std::runtime_error("JSON: expected delimiter");}
    static void utf8(std::string& out, unsigned x){
        if(x<0x80)out.push_back(static_cast<char>(x));
        else if(x<0x800){out.push_back(static_cast<char>(0xc0|(x>>6)));out.push_back(static_cast<char>(0x80|(x&63)));}
        else if(x<0x10000){out.push_back(static_cast<char>(0xe0|(x>>12)));out.push_back(static_cast<char>(0x80|((x>>6)&63)));out.push_back(static_cast<char>(0x80|(x&63)));}
        else {out.push_back(static_cast<char>(0xf0|(x>>18)));out.push_back(static_cast<char>(0x80|((x>>12)&63)));out.push_back(static_cast<char>(0x80|((x>>6)&63)));out.push_back(static_cast<char>(0x80|(x&63)));}
    }
    unsigned hex4(){unsigned result=0;for(int i=0;i<4;++i){char c=get();result<<=4;if(c>='0'&&c<='9')result+=static_cast<unsigned>(c-'0');else if(c>='a'&&c<='f')result+=static_cast<unsigned>(c-'a'+10);else if(c>='A'&&c<='F')result+=static_cast<unsigned>(c-'A'+10);else throw std::runtime_error("JSON: invalid unicode escape");}return result;}
    std::string string(){expect('"');std::string out;while(true){char c=get();if(c=='"')break;if(static_cast<unsigned char>(c)<32)throw std::runtime_error("JSON: control character");if(c!='\\'){out+=c;continue;}c=get();switch(c){case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'u':{unsigned x=hex4();if(x>=0xd800&&x<=0xdbff){if(get()!='\\'||get()!='u')throw std::runtime_error("JSON: invalid surrogate");const unsigned y=hex4();if(y<0xdc00||y>0xdfff)throw std::runtime_error("JSON: invalid surrogate pair");x=0x10000+((x-0xd800)<<10)+(y-0xdc00);}else if(x>=0xdc00&&x<=0xdfff)throw std::runtime_error("JSON: isolated surrogate");utf8(out,x);break;}default:throw std::runtime_error("JSON: invalid escape");}}return out;}
    Value value(int depth){if(depth>64)throw std::runtime_error("JSON: max nesting exceeded");ws();if(p_>=data_.size())throw std::runtime_error("JSON: empty value");Value v;char c=data_[p_];if(c=='"'){v.kind=Value::Kind::string_value;v.text=string();}else if(c=='{'){v.kind=Value::Kind::object_value;expect('{');ws();if(p_<data_.size()&&data_[p_]=='}'){++p_;return v;}while(true){std::string k=string();expect(':');auto child=value(depth+1);if(!v.object.emplace(std::move(k),std::move(child)).second)throw std::runtime_error("JSON: duplicate key");ws();char sep=get();if(sep=='}')break;if(sep!=',')throw std::runtime_error("JSON: invalid object separator");}}
        else if(c=='['){v.kind=Value::Kind::array_value;expect('[');ws();if(p_<data_.size()&&data_[p_]==']'){++p_;return v;}while(true){v.array.push_back(value(depth+1));ws();char sep=get();if(sep==']')break;if(sep!=',')throw std::runtime_error("JSON: invalid array separator");}}
        else {v.kind=Value::Kind::other_value;const size_t start=p_;while(p_<data_.size()&&data_[p_]!=','&&data_[p_]!=']'&&data_[p_]!='}'&&!std::isspace(static_cast<unsigned char>(data_[p_])))++p_;if(p_==start)throw std::runtime_error("JSON: illegal token");v.text=data_.substr(start,p_-start);if(v.text!="true"&&v.text!="false"&&v.text!="null"&&v.text.find_first_not_of("-+0123456789.eE")!=std::string::npos)throw std::runtime_error("JSON: invalid literal");}return v;}
public:
    explicit Parser(const std::string& text):data_(text){}
    Value parse(){Value root=value(0);ws();if(p_!=data_.size())throw std::runtime_error("JSON: trailing content");return root;}
};
inline Value parse(const std::string& content){return Parser(content).parse();}
}
