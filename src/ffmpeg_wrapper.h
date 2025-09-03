#pragma once
#include <string>
#include <vector>

class FFmpegWrapper {
public:
    struct StreamOptions {
        std::vector<std::string> audioLanguages;
        std::vector<std::string> subtitleLanguages;
        bool copyStreams = true;
        int threads = 8;
        std::string bufferSize = "256M";
    };
    
    static bool RemuxBDMV(const std::string& inputMPLS, 
                         const std::string& outputMKV,
                         const StreamOptions& options);
    
    static bool IsFFmpegAvailable();
    static std::string GetFFmpegVersion();
    
private:
    static std::string BuildFFmpegCommand(const std::string& input, 
                                        const std::string& output,
                                        const StreamOptions& options);
    static std::string LanguageNameToCode(const std::string& languageName);
};