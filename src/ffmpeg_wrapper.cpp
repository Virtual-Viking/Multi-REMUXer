#include "ffmpeg_wrapper.h"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <map>

// Language name to ISO code mapping for FFmpeg
std::map<std::string, std::string> languageNameToCode = {
    {"English", "eng"}, {"Spanish", "spa"}, {"French", "fre"}, {"German", "ger"},
    {"Italian", "ita"}, {"Portuguese", "por"}, {"Russian", "rus"}, {"Japanese", "jpn"},
    {"Korean", "kor"}, {"Chinese", "chi"}, {"Hindi", "hin"}, {"Arabic", "ara"},
    {"Dutch", "dut"}, {"Swedish", "swe"}, {"Norwegian", "nor"}, {"Danish", "dan"},
    {"Finnish", "fin"}, {"Polish", "pol"}, {"Czech", "cze"}, {"Hungarian", "hun"},
    {"Thai", "tha"}, {"Vietnamese", "vie"}, {"Unknown", "und"}
};

bool FFmpegWrapper::RemuxBDMV(const std::string& inputMPLS, 
                             const std::string& outputMKV,
                             const StreamOptions& options) {
    
    std::string command = BuildFFmpegCommand(inputMPLS, outputMKV, options);
    
    // Execute FFmpeg command
    OutputDebugStringA(("Executing: " + command).c_str());
    
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide console window
    
    BOOL success = CreateProcessA(
        nullptr,
        const_cast<char*>(command.c_str()),
        nullptr, nullptr, FALSE, 0,
        nullptr, nullptr, &si, &pi
    );
    
    if (!success) {
        return false;
    }
    
    // Wait for process to complete
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return exitCode == 0;
}

std::string FFmpegWrapper::BuildFFmpegCommand(const std::string& input, 
                                            const std::string& output,
                                            const StreamOptions& options) {
    std::ostringstream cmd;
    
    // Base FFmpeg command with optimizations
    cmd << "ffmpeg -y -hide_banner -loglevel warning";
    cmd << " -fflags +genpts+discardcorrupt";
    cmd << " -analyzeduration 200M -probesize 200M";
    cmd << " -threads " << options.threads;
    cmd << " -i \"" << input << "\"";
    
    // Always map main video stream
    cmd << " -map 0:v:0";
    
    // Map audio streams based on selected languages
    if (!options.audioLanguages.empty()) {
        for (const auto& langName : options.audioLanguages) {
            std::string langCode = LanguageNameToCode(langName);
            cmd << " -map 0:a:m:language:" << langCode;
        }
    } else {
        cmd << " -map 0:a"; // Map all audio if none specified
    }
    
    // Map subtitle streams based on selected languages
    if (!options.subtitleLanguages.empty()) {
        for (const auto& langName : options.subtitleLanguages) {
            std::string langCode = LanguageNameToCode(langName);
            cmd << " -map 0:s:m:language:" << langCode;
        }
    }
    
    // Codec and optimization settings
    if (options.copyStreams) {
        cmd << " -c copy";
    }
    
    cmd << " -avoid_negative_ts make_zero";
    cmd << " -map_metadata 0 -map_chapters 0";
    
    // MKV-specific optimizations
    cmd << " -f matroska";
    cmd << " -write_crc32 0";
    cmd << " -cluster_size_limit 2M";
    
    // Output file
    cmd << " \"" << output << "\"";
    
    return cmd.str();
}

std::string FFmpegWrapper::LanguageNameToCode(const std::string& languageName) {
    auto it = languageNameToCode.find(languageName);
    return (it != languageNameToCode.end()) ? it->second : "und";
}

bool FFmpegWrapper::IsFFmpegAvailable() {
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    std::string command = "ffmpeg -version";
    
    BOOL success = CreateProcessA(
        nullptr,
        const_cast<char*>(command.c_str()),
        nullptr, nullptr, FALSE, 0,
        nullptr, nullptr, &si, &pi
    );
    
    if (!success) {
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, 5000); // 5 second timeout
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return exitCode == 0;
}

std::string FFmpegWrapper::GetFFmpegVersion() {
    // Create temporary file for output
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string tempFile = std::string(tempPath) + "ffmpeg_version.txt";
    
    std::string command = "ffmpeg -version > \"" + tempFile + "\" 2>&1";
    
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    BOOL success = CreateProcessA(
        nullptr,
        const_cast<char*>(command.c_str()),
        nullptr, nullptr, FALSE, 0,
        nullptr, nullptr, &si, &pi
    );
    
    if (!success) {
        return "Unknown";
    }
    
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Read the output file
    std::ifstream file(tempFile);
    std::string result;
    if (file.is_open()) {
        std::getline(file, result);
        file.close();
    }
    
    // Clean up temp file
    DeleteFileA(tempFile.c_str());
    
    return result.empty() ? "Unknown" : result;
}