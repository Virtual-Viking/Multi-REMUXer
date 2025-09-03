#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <thread>
#include <filesystem>
#include <fstream>
#include <regex>
#include <iostream>
#include <algorithm>
#include "bdmv_parser.h"
#include "ffmpeg_wrapper.h"

namespace fs = std::filesystem;

// Window controls IDs
#define ID_LISTVIEW_FILES       1001
#define ID_LISTVIEW_AUDIO       1002  
#define ID_LISTVIEW_SUBTITLES   1003
#define ID_TEXTBOX_CONSOLE      1004
#define ID_PROGRESSBAR          1005
#define ID_BUTTON_BROWSE        1006
#define ID_BUTTON_START         1007
#define ID_BUTTON_STOP          1008
#define ID_EDIT_OUTPUT          1009
#define ID_BUTTON_OUTPUT_BROWSE 1010

// Custom messages
#define WM_UPDATE_PROGRESS      (WM_USER + 1)
#define WM_ADD_LOG             (WM_USER + 2)
#define WM_PROCESSING_COMPLETE  (WM_USER + 3)

struct BDMVFile {
    std::string path;
    std::string description;
    std::vector<BDMVTitle> titles;
    std::string status;
};

class MultiRemuxer {
private:
    HWND hMainWindow;
    HWND hFileListView;
    HWND hAudioListView; 
    HWND hSubtitleListView;
    HWND hConsoleEdit;
    HWND hProgressBar;
    HWND hOutputEdit;
    HWND hStartButton;
    HWND hStopButton;
    
    std::vector<BDMVFile> files;
    std::vector<std::string> selectedAudioLanguages;
    std::vector<std::string> selectedSubtitleLanguages;
    std::string outputDirectory;
    
    bool isProcessing = false;
    std::thread processingThread;
    
public:
    MultiRemuxer() {}
    
    bool Initialize(HINSTANCE hInstance) {
        // Initialize common controls
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS;
        InitCommonControlsEx(&icex);
        
        // Create main window
        WNDCLASSEX wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WindowProc;
        wcex.hInstance = hInstance;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszClassName = L"MultiRemuxerClass";
        wcex.cbWndExtra = sizeof(MultiRemuxer*);
        
        if (!RegisterClassEx(&wcex)) return false;
        
        hMainWindow = CreateWindowEx(
            0, L"MultiRemuxerClass", L"Multi-REMUXer v1.0",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
            nullptr, nullptr, hInstance, this
        );
        
        if (!hMainWindow) return false;
        
        CreateControls();
        
        // Enable drag and drop
        DragAcceptFiles(hMainWindow, TRUE);
        
        ShowWindow(hMainWindow, SW_SHOW);
        UpdateWindow(hMainWindow);
        
        return true;
    }
    
    void CreateControls() {
        // File list view
        hFileListView = CreateWindow(
            WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
            20, 80, 700, 250,
            hMainWindow, (HMENU)ID_LISTVIEW_FILES, nullptr, nullptr
        );
        
        // Setup file list columns
        LVCOLUMN lvc = {};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText = (LPWSTR)L"Sr. No.";
        lvc.cx = 60;
        ListView_InsertColumn(hFileListView, 0, &lvc);
        
        lvc.pszText = (LPWSTR)L"Type";
        lvc.cx = 80;
        ListView_InsertColumn(hFileListView, 1, &lvc);
        
        lvc.pszText = (LPWSTR)L"Description";
        lvc.cx = 400;
        ListView_InsertColumn(hFileListView, 2, &lvc);
        
        lvc.pszText = (LPWSTR)L"Status";
        lvc.cx = 100;
        ListView_InsertColumn(hFileListView, 3, &lvc);
        
        // Audio languages list
        hAudioListView = CreateWindow(
            WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT,
            750, 80, 200, 120,
            hMainWindow, (HMENU)ID_LISTVIEW_AUDIO, nullptr, nullptr
        );
        
        // Enable checkboxes for audio list
        ListView_SetExtendedListViewStyle(hAudioListView, LVS_EX_CHECKBOXES);
        
        lvc.pszText = (LPWSTR)L"Audio Languages";
        lvc.cx = 180;
        ListView_InsertColumn(hAudioListView, 0, &lvc);
        
        // Subtitle languages list  
        hSubtitleListView = CreateWindow(
            WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT,
            980, 80, 200, 120,
            hMainWindow, (HMENU)ID_LISTVIEW_SUBTITLES, nullptr, nullptr
        );
        
        // Enable checkboxes for subtitle list
        ListView_SetExtendedListViewStyle(hSubtitleListView, LVS_EX_CHECKBOXES);
        
        lvc.pszText = (LPWSTR)L"Subtitle Languages";
        lvc.cx = 180;
        ListView_InsertColumn(hSubtitleListView, 0, &lvc);
        
        // Console text box
        hConsoleEdit = CreateWindow(
            L"EDIT", L"Multi-REMUXer ready...\r\n",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            20, 350, 1160, 150,
            hMainWindow, (HMENU)ID_TEXTBOX_CONSOLE, nullptr, nullptr
        );
        
        // Set console font to monospace
        HFONT hConsoleFont = CreateFont(
            14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas"
        );
        SendMessage(hConsoleEdit, WM_SETFONT, (WPARAM)hConsoleFont, TRUE);
        
        // Progress bar
        hProgressBar = CreateWindow(
            PROGRESS_CLASS, L"",
            WS_CHILD | WS_VISIBLE,
            20, 520, 800, 25,
            hMainWindow, (HMENU)ID_PROGRESSBAR, nullptr, nullptr
        );
        SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        
        // Output directory
        CreateWindow(L"STATIC", L"Output Folder:",
            WS_CHILD | WS_VISIBLE,
            20, 560, 100, 20,
            hMainWindow, nullptr, nullptr, nullptr
        );
        
        hOutputEdit = CreateWindow(
            L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            20, 580, 700, 25,
            hMainWindow, (HMENU)ID_EDIT_OUTPUT, nullptr, nullptr
        );
        
        CreateWindow(L"BUTTON", L"Browse",
            WS_CHILD | WS_VISIBLE,
            730, 580, 80, 25,
            hMainWindow, (HMENU)ID_BUTTON_OUTPUT_BROWSE, nullptr, nullptr
        );
        
        // Control buttons
        hStartButton = CreateWindow(
            L"BUTTON", L"Start Processing",
            WS_CHILD | WS_VISIBLE,
            850, 580, 150, 25,
            hMainWindow, (HMENU)ID_BUTTON_START, nullptr, nullptr
        );
        
        hStopButton = CreateWindow(
            L"BUTTON", L"Stop",
            WS_CHILD | WS_VISIBLE | WS_DISABLED,
            1020, 580, 80, 25,
            hMainWindow, (HMENU)ID_BUTTON_STOP, nullptr, nullptr
        );
        
        CreateWindow(L"BUTTON", L"Add Files/Folders",
            WS_CHILD | WS_VISIBLE,
            20, 40, 150, 30,
            hMainWindow, (HMENU)ID_BUTTON_BROWSE, nullptr, nullptr
        );
        
        // Drop zone label
        CreateWindow(L"STATIC", L"Drag and Drop BDMV folders or ISO files here:",
            WS_CHILD | WS_VISIBLE,
            200, 50, 400, 20,
            hMainWindow, nullptr, nullptr, nullptr
        );
    }
    
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        MultiRemuxer* pThis = nullptr;
        
        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<MultiRemuxer*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hWnd, 0, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<MultiRemuxer*>(GetWindowLongPtr(hWnd, 0));
        }
        
        if (pThis) {
            return pThis->HandleMessage(hWnd, uMsg, wParam, lParam);
        }
        
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    
    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_COMMAND:
                HandleCommand(LOWORD(wParam));
                break;
                
            case WM_DROPFILES:
                HandleDropFiles((HDROP)wParam);
                break;
                
            case WM_UPDATE_PROGRESS:
                SendMessage(hProgressBar, PBM_SETPOS, wParam, 0);
                break;
                
            case WM_ADD_LOG: {
                std::string* logMessage = reinterpret_cast<std::string*>(lParam);
                AddConsoleLog(*logMessage);
                delete logMessage;
                break;
            }
            
            case WM_PROCESSING_COMPLETE:
                OnProcessingComplete();
                break;
                
            case WM_NOTIFY: {
                LPNMHDR pnmhdr = (LPNMHDR)lParam;
                if (pnmhdr->idFrom == ID_LISTVIEW_AUDIO && pnmhdr->code == LVN_ITEMCHANGED) {
                    UpdateSelectedAudioLanguages();
                } else if (pnmhdr->idFrom == ID_LISTVIEW_SUBTITLES && pnmhdr->code == LVN_ITEMCHANGED) {
                    UpdateSelectedSubtitleLanguages();
                }
                break;
            }
            
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
                
            default:
                return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
        return 0;
    }
    
    void HandleCommand(WORD commandId) {
        switch (commandId) {
            case ID_BUTTON_BROWSE:
                BrowseForFiles();
                break;
                
            case ID_BUTTON_OUTPUT_BROWSE:
                BrowseForOutputFolder();
                break;
                
            case ID_BUTTON_START:
                StartProcessing();
                break;
                
            case ID_BUTTON_STOP:
                StopProcessing();
                break;
        }
    }
    
    void HandleDropFiles(HDROP hDrop) {
        UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
        
        for (UINT i = 0; i < fileCount; i++) {
            wchar_t filePath[MAX_PATH];
            DragQueryFile(hDrop, i, filePath, MAX_PATH);
            
            // Convert to string and analyze
            std::wstring wPath(filePath);
            std::string path(wPath.begin(), wPath.end());
            
            AnalyzeAndAddFile(path);
        }
        
        DragFinish(hDrop);
        RefreshLanguageLists();
    }
    
    void AnalyzeAndAddFile(const std::string& path) {
        try {
            fs::path fsPath(path);
            
            // Check if it's a BDMV folder or contains BDMV
            if (fs::is_directory(fsPath)) {
                if (fsPath.filename() == "BDMV" || fs::exists(fsPath / "BDMV")) {
                    // Use BDMVParser to analyze the folder
                    std::vector<BDMVTitle> titles = BDMVParser::ParseBDMVFolder(path);
                    
                    if (!titles.empty()) {
                        BDMVFile file;
                        file.path = path;
                        file.status = "Ready";
                        
                        if (fsPath.filename() == "BDMV") {
                            file.description = fsPath.parent_path().filename().string();
                        } else {
                            file.description = fsPath.filename().string();
                        }
                        
                        // Convert BDMVTitle to our internal format
                        for (const auto& title : titles) {
                            file.titles.push_back(title);
                        }
                        
                        files.push_back(file);
                        AddFileToListView(file, files.size());
                        AddConsoleLog("Added: " + file.description);
                    }
                }
            } else if (fsPath.extension() == ".iso") {
                AddConsoleLog("ISO files require mounting - not yet implemented");
            }
        } catch (const std::exception& e) {
            AddConsoleLog("Error analyzing: " + path + " - " + e.what());
        }
    }
        
    void AddFileToListView(const BDMVFile& file, int index) {
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = index - 1;
        lvi.lParam = index - 1;
        
        // Sr. No.
        std::wstring srNo = std::to_wstring(index) + L".";
        lvi.pszText = (LPWSTR)srNo.c_str();
        ListView_InsertItem(hFileListView, &lvi);
        
        // Type
        ListView_SetItemText(hFileListView, index - 1, 1, (LPWSTR)L"Blu-Ray");
        
        // Description
        std::wstring desc(file.description.begin(), file.description.end());
        ListView_SetItemText(hFileListView, index - 1, 2, (LPWSTR)desc.c_str());
        
        // Status
        std::wstring status(file.status.begin(), file.status.end());
        ListView_SetItemText(hFileListView, index - 1, 3, (LPWSTR)status.c_str());
    }
    
    void RefreshLanguageLists() {
        // Clear existing items
        ListView_DeleteAllItems(hAudioListView);
        ListView_DeleteAllItems(hSubtitleListView);
        
        // Collect all unique languages
        std::set<std::string> audioLangs, subtitleLangs;
        
        for (const auto& file : files) {
            for (const auto& title : file.titles) {
                audioLangs.insert(title.audioLanguages.begin(), title.audioLanguages.end());
                subtitleLangs.insert(title.subtitleLanguages.begin(), title.subtitleLanguages.end());
            }
        }
        
        // Add to list views
        int index = 0;
        for (const auto& lang : audioLangs) {
            LVITEM lvi = {};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = index++;
            std::wstring wLang(lang.begin(), lang.end());
            lvi.pszText = (LPWSTR)wLang.c_str();
            ListView_InsertItem(hAudioListView, &lvi);
            
            // Check English by default
            if (lang == "English") {
                ListView_SetCheckState(hAudioListView, index - 1, TRUE);
            }
        }
        
        index = 0;
        for (const auto& lang : subtitleLangs) {
            LVITEM lvi = {};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = index++;
            std::wstring wLang(lang.begin(), lang.end());
            lvi.pszText = (LPWSTR)wLang.c_str();
            ListView_InsertItem(hSubtitleListView, &lvi);
            
            // Check English by default
            if (lang == "English") {
                ListView_SetCheckState(hSubtitleListView, index - 1, TRUE);
            }
        }
        
        UpdateSelectedAudioLanguages();
        UpdateSelectedSubtitleLanguages();
    }
    
    void UpdateSelectedAudioLanguages() {
        selectedAudioLanguages.clear();
        int itemCount = ListView_GetItemCount(hAudioListView);
        
        for (int i = 0; i < itemCount; i++) {
            if (ListView_GetCheckState(hAudioListView, i)) {
                wchar_t buffer[256];
                ListView_GetItemText(hAudioListView, i, 0, buffer, 256);
                std::wstring wLang(buffer);
                selectedAudioLanguages.emplace_back(wLang.begin(), wLang.end());
            }
        }
    }
    
    void UpdateSelectedSubtitleLanguages() {
        selectedSubtitleLanguages.clear();
        int itemCount = ListView_GetItemCount(hSubtitleListView);
        
        for (int i = 0; i < itemCount; i++) {
            if (ListView_GetCheckState(hSubtitleListView, i)) {
                wchar_t buffer[256];
                ListView_GetItemText(hSubtitleListView, i, 0, buffer, 256);
                std::wstring wLang(buffer);
                selectedSubtitleLanguages.emplace_back(wLang.begin(), wLang.end());
            }
        }
    }
    
    void BrowseForFiles() {
        // Use folder browser for BDMV folders
        BROWSEINFO bi = {};
        bi.hwndOwner = hMainWindow;
        bi.lpszTitle = L"Select BDMV folder or disc root";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        
        LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
        if (pidl) {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDList(pidl, path)) {
                std::wstring wPath(path);
                std::string strPath(wPath.begin(), wPath.end());
                AnalyzeAndAddFile(strPath);
                RefreshLanguageLists();
            }
            CoTaskMemFree(pidl);
        }
    }
    
    void BrowseForOutputFolder() {
        BROWSEINFO bi = {};
        bi.hwndOwner = hMainWindow;
        bi.lpszTitle = L"Select output directory";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        
        LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
        if (pidl) {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDList(pidl, path)) {
                SetWindowText(hOutputEdit, path);
                std::wstring wPath(path);
                outputDirectory = std::string(wPath.begin(), wPath.end());
            }
            CoTaskMemFree(pidl);
        }
    }
    
    void StartProcessing() {
        if (files.empty()) {
            MessageBox(hMainWindow, L"No files to process", L"Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        if (outputDirectory.empty()) {
            MessageBox(hMainWindow, L"Please select output directory", L"Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        isProcessing = true;
        EnableWindow(hStartButton, FALSE);
        EnableWindow(hStopButton, TRUE);
        
        // Start processing thread
        processingThread = std::thread(&MultiRemuxer::ProcessFiles, this);
        processingThread.detach();
        
        AddConsoleLog("Processing started...");
    }
    
    void StopProcessing() {
        isProcessing = false;
        AddConsoleLog("Processing stopped by user");
    }
    
    void ProcessFiles() {
        try {
            fs::create_directories(outputDirectory);
            
            for (size_t i = 0; i < files.size() && isProcessing; i++) {
                auto& file = files[i];
                
                // Update status in list view
                std::wstring status = L"Processing...";
                ListView_SetItemText(hFileListView, static_cast<int>(i), 3, (LPWSTR)status.c_str());
                
                // Process main title (longest duration)
                if (!file.titles.empty()) {
                    auto mainTitle = *std::max_element(file.titles.begin(), file.titles.end(),
                        [](const BDMVTitle& a, const BDMVTitle& b) {
                            return a.duration < b.duration;
                        });
                    
                    std::string outputFile = outputDirectory + "\\" + file.description + ".mkv";
                    
                    bool success = ProcessTitle(file.path, mainTitle, outputFile);
                    
                    if (success) {
                        file.status = "Completed";
                        status = L"Completed";
                    } else {
                        file.status = "Error";
                        status = L"Error";
                    }
                    
                    ListView_SetItemText(hFileListView, static_cast<int>(i), 3, (LPWSTR)status.c_str());
                }
                
                // Update progress
                int progress = (int)(((i + 1.0) / files.size()) * 100);
                PostMessage(hMainWindow, WM_UPDATE_PROGRESS, progress, 0);
                
                std::string* logMsg = new std::string("Processed: " + file.description);
                PostMessage(hMainWindow, WM_ADD_LOG, 0, (LPARAM)logMsg);
            }
            
        } catch (const std::exception& e) {
            std::string* logMsg = new std::string("Processing error: " + std::string(e.what()));
            PostMessage(hMainWindow, WM_ADD_LOG, 0, (LPARAM)logMsg);
        }
        
        PostMessage(hMainWindow, WM_PROCESSING_COMPLETE, 0, 0);
    }
    
    bool ProcessTitle(const std::string& bdmvPath, const BDMVTitle& title, const std::string& outputFile) {
        try {
            // Build MPLS file path
            std::string mplsPath = bdmvPath;
            if (fs::path(bdmvPath).filename() != "BDMV") {
                mplsPath += "\\BDMV";
            }
            mplsPath += "\\PLAYLIST\\" + title.filename;
            
            // Use FFmpegWrapper to process
            FFmpegWrapper::StreamOptions options;
            options.audioLanguages = selectedAudioLanguages;
            options.subtitleLanguages = selectedSubtitleLanguages;
            options.threads = 8; // Use 8 threads for good performance
            
            return FFmpegWrapper::RemuxBDMV(mplsPath, outputFile, options);
            
        } catch (const std::exception& e) {
            AddConsoleLog("Error processing title: " + std::string(e.what()));
            return false;
        }
    }
    
    void OnProcessingComplete() {
        isProcessing = false;
        EnableWindow(hStartButton, TRUE);
        EnableWindow(hStopButton, FALSE);
        SendMessage(hProgressBar, PBM_SETPOS, 100, 0);
        AddConsoleLog("All processing completed!");
    }
    
    void AddConsoleLog(const std::string& message) {
        // Get current time
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        char timeStr[32];
        sprintf_s(timeStr, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
        
        std::string fullMessage = timeStr + message + "\r\n";
        
        // Append to console
        int length = GetWindowTextLength(hConsoleEdit);
        SendMessage(hConsoleEdit, EM_SETSEL, length, length);
        
        std::wstring wMessage(fullMessage.begin(), fullMessage.end());
        SendMessage(hConsoleEdit, EM_REPLACESEL, FALSE, (LPARAM)wMessage.c_str());
        
        // Scroll to bottom
        SendMessage(hConsoleEdit, EM_SCROLLCARET, 0, 0);
    }
    
    void Run() {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    MultiRemuxer app;
    
    if (!app.Initialize(hInstance)) {
        MessageBox(nullptr, L"Failed to initialize application", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    app.Run();
    return 0;
}