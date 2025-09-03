#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

struct PlayItem {
    std::string clipName;
    uint32_t inTime;
    uint32_t outTime;
    double GetDurationSeconds() const;
};

struct BDMVTitle {
    int id;
    std::string filename;
    double duration;
    size_t size;
    std::vector<std::string> audioLanguages;
    std::vector<std::string> subtitleLanguages;
};

class BDMVParser {
private:
    static std::map<std::string, std::string> languageMap;
    
public:
    static std::vector<BDMVTitle> ParseBDMVFolder(const std::string& path);
    static BDMVTitle ParseMPLSFile(const fs::path& mplsPath, const fs::path& streamDir);
    static PlayItem ParsePlayItem(std::ifstream& file);
    static std::vector<std::string> GetAudioLanguages(const fs::path& streamDir, 
                                                     const std::vector<PlayItem>& playItems);
    static std::vector<std::string> GetSubtitleLanguages(const fs::path& streamDir, 
                                                        const std::vector<PlayItem>& playItems);
    static std::set<std::string> AnalyzeStreamLanguages(const fs::path& m2tsPath, 
                                                       const std::string& streamType);
    static std::string GetLanguageName(const std::string& code);
};