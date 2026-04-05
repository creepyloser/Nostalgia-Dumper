#pragma once

#include <windows.h>
#include <map>
#include <sstream>
#include <string>

namespace Dumpers
{
    void RunUnityIl2Cpp(HANDLE process, std::map<std::string, u64>& symbols, std::ostringstream& log);
    void RunUnityMono(HANDLE process, std::map<std::string, u64>& symbols, std::ostringstream& log);
    void RunSource1(HANDLE process, std::map<std::string, u64>& symbols, std::ostringstream& log);
    void RunSource2(HANDLE process, std::map<std::string, u64>& symbols, std::ostringstream& log);
    void RunGodot(HANDLE process, std::map<std::string, u64>& symbols, std::ostringstream& log);
    void RunCryEngine(HANDLE process, std::map<std::string, u64>& symbols, std::ostringstream& log);
    void RunUniversal(HANDLE process, std::map<std::string, u64>& symbols, std::ostringstream& log);
    void RunGameMaker(HANDLE process, std::map<std::string, u64>& symbols, std::ostringstream& log);
    void RunUnigine(HANDLE process, std::map<std::string, u64>& symbols, std::ostringstream& log);
}
