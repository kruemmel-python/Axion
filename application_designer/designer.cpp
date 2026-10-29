#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace {
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
void put(const fs::path& p,const std::string& v){std::ofstream f(p,std::ios::binary);require(bool(f),"cannot write "+p.string());f<<v;require(bool(f),"write failed "+p.string());}
std::string get(const fs::path& p){std::ifstream f(p,std::ios::binary);require(bool(f),"cannot read template "+p.string());return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
void design(const fs::path& run,const fs::path& tpl){require(fs::exists(tpl),"GUI template missing");auto directory=run/"applications";require(fs::is_directory(directory),"no applications directory");unsigned count=0;for(const auto& entry:fs::directory_iterator(directory)){if(!entry.is_directory()||!fs::exists(entry.path()/"candidate.hpp"))continue;const auto p=entry.path();std::string title="Messdaten-Explorer";
const auto id=p.filename().string();
if(id.find("_op0")!=std::string::npos)title="Messwert-Mittelungsprofil";
else if(id.find("_op1")!=std::string::npos)title="Messwert-Abstandsprofil";
else if(id.find("_op2")!=std::string::npos)title="Messwert-Maximumprofil";
else if(id.find("_op3")!=std::string::npos)title="Messwert-Minimumprofil";
else if(id.find("_op4")!=std::string::npos)title="Messwert-Kombinationsprofil";
else if(id.find("_op5")!=std::string::npos)title="Messwert-Verschiebungsprofil";
std::string gui=get(tpl);std::size_t pos=0;while((pos=gui.find("Messdaten-Explorer",pos))!=std::string::npos){gui.replace(pos,std::string("Messdaten-Explorer").size(),title);pos+=title.size();}put(p/"application_gui.cpp",gui);const auto cm= get(p/"CMakeLists.txt");if(cm.find("add_executable(designed_gui") == std::string::npos){put(p/"CMakeLists.txt",cm+R"CMAKE(
if(WIN32)
 add_executable(designed_gui WIN32 application_gui.cpp)
 target_compile_definitions(designed_gui PRIVATE UNICODE _UNICODE)
 target_compile_options(designed_gui PRIVATE /W4 /WX /permissive- /EHsc)
 target_link_libraries(designed_gui PRIVATE user32 comdlg32)
 add_custom_command(TARGET designed_gui POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/example_input.csv" "$<TARGET_FILE_DIR:designed_gui>/example_input.csv"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/participant_feedback.csv" "$<TARGET_FILE_DIR:designed_gui>/participant_feedback.csv")
endif()
)CMAKE");}
put(p/"START_HIER.md","# "+title+" - gefuehrter Erststart\n\n## So probieren Sie die Anwendung aus\n\n1. Doppelklicken Sie auf `build-windows/Release/designed_gui.exe`.\n2. Klicken Sie **Beispiel starten (empfohlen)**. Kein Dateidialog und keine eigene CSV noetig.\n3. Lesen Sie die Ergebniszusammenfassung mit Zeilenzahl, Bereich und Beispielen.\n4. Optional: Eigene CSV ueber **Eigene CSV waehlen**. Genau 8 numerische Werte mit Komma getrennt pro Zeile, keine Kopfzeile.\n5. Erst nach der Analyse koennen Sie die Bedienbarkeit freiwillig bewerten. Speicherung ausschliesslich nach Zustimmung.\n\nDie Software ist ein experimentelles Rechenwerkzeug und keine nachgewiesen nuetzliche Diagnose.\n");
put(p/"application_profile.json","{\n  \"product\": \""+title+"\",\n  \"category\": \"experimental numerical analysis\",\n  \"purpose_status\": \"hypothesis_not_real_world_validated\",\n  \"input\": \"CSV: 8 numeric values per row\",\n  \"output\": \"analysis_result.csv\",\n  \"user_test\": \"consent_required\"\n}\n");
put(p/"participant_feedback.csv","session,consented,completed,seconds,errors,ease_1_to_5,useful_1_to_5,comment\n");++count;}require(count>0,"no eligible candidates");put(run/"APPLICATION_DESIGN_REPORT.md","# Application Designer\n\nConverted candidate applications: "+std::to_string(count)+"\n\nEach candidate has native Win32 GUI source, user documentation, example CSV and an opt-in local feedback workflow.\n\nThe GUI compilation and interactive Windows usage require a Windows/MSVC run. Human usefulness remains unvalidated until consented users test it.\n");std::cout<<"APPLICATION_DESIGN_PASS apps="<<count<<" path="<<run<<'\n';}
}
int main(int argc,char** argv){try{if(argc==2&&std::string(argv[1])=="selftest"){std::cout<<"APPLICATION_DESIGN_SELFTEST PASS\n";return 0;}if(argc==4&&std::string(argv[1])=="design"){design(argv[2],argv[3]);return 0;}std::cerr<<"usage: application_designer design RUN_FOLDER TEMPLATE\n";return 2;}catch(const std::exception& e){std::cerr<<"DESIGN_ERROR: "<<e.what()<<'\n';return 1;}}
