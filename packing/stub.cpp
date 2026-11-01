// Capson single-file self-extracting launcher.
// The payload (exe + all Qt DLLs and plugins) is appended to this stub along
// with a small table. On run it unpacks the app into %LOCALAPPDATA%\Capson\app
// (when the bundled version changed) and launches it. No external dependencies.
#define UNICODE 1
#define _UNICODE 1
#include <windows.h>
#include <shlobj.h>
#include <zlib.h>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct FileEntry {
    bool isDir = false;
    bool compressed = false;
    std::string name;
    size_t size = 0;
    size_t compSize = 0; 
};

std::wstring selfPath()
{
    std::vector<wchar_t> buf(32768);
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n == 0 || n >= buf.size()) return L"";
    return std::wstring(buf.data(), n);
}

std::vector<unsigned char> readWholeFile(const std::wstring &path)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return {};
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) return {};
    std::vector<unsigned char> data(static_cast<size_t>(size));
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char *>(data.data()), size);
    return data;
}

bool writeFile(const std::wstring &path, const unsigned char *data, size_t size)
{
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
    return out.good();
}

std::wstring toWide(const std::string &s)
{
    if (s.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

void ensureDir(const std::wstring &dir)
{
    std::wstring acc;
    for (size_t i = 0; i < dir.size(); ++i) {
        acc.push_back(dir[i]);
        if (dir[i] == L'\\' || dir[i] == L'/') {
            acc.back() = L'\\';
            CreateDirectoryW(acc.c_str(), nullptr);
        }
    }
    CreateDirectoryW(acc.c_str(), nullptr);
}

void removeTree(const std::wstring &dir)
{
    std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        const std::wstring full = dir + L"\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            removeTree(full);
        else
            DeleteFileW(full.c_str());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    RemoveDirectoryW(dir.c_str());
}

bool localAppDataDir(std::wstring &out)
{
    PWSTR p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) {
        out = p;
        CoTaskMemFree(p);
        return !out.empty();
    }
    return false;
}

void fail(const std::wstring &msg)
{
    MessageBoxW(nullptr, msg.c_str(), L"Capson", MB_ICONERROR | MB_OK);
}

// FNV-1a 64 hash over the whole payload entry table (names + sizes + flags).
// Any file added, removed, renamed or resized in the payload changes it, so a
// stub with a different hash re-extracts even if Capson.exe itself is unchanged.
struct Hasher {
    uint64_t h = 1469598103934665603ULL;
    void write(const void *p, size_t n) {
        const uint8_t *q = static_cast<const uint8_t *>(p);
        for (size_t i = 0; i < n; ++i) { h ^= q[i]; h *= 1099511628211ULL; }
    }
    void writeU32(uint32_t v) { write(&v, sizeof(v)); }
};

std::string toHex(uint64_t v)
{
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
    return std::string(buf);
}

bool inflateRaw(const unsigned char *in, size_t inLen, size_t expected,
                std::vector<unsigned char> &out)
{
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) return false;
    zs.next_in = const_cast<Bytef *>(in);
    zs.avail_in = static_cast<uInt>(inLen);
    out.reserve(expected ? expected : inLen * 3);

    std::vector<unsigned char> tmp(8192);
    int ret = Z_OK;
    do {
        zs.next_out = tmp.data();
        zs.avail_out = static_cast<uInt>(tmp.size());
        ret = inflate(&zs, Z_NO_FLUSH);
        out.insert(out.end(), tmp.begin(), tmp.end() - zs.avail_out);
        if (ret == Z_BUF_ERROR && zs.avail_out == tmp.size()) { ret = Z_DATA_ERROR; break; }
    } while (ret != Z_STREAM_END);
    inflateEnd(&zs);
    return ret == Z_STREAM_END && out.size() == expected;
}

} // namespace

int wmain()
{
    const std::vector<unsigned char> buf = readWholeFile(selfPath());
    if (buf.size() < 16) { fail(L"This Capson file is missing its payload."); return 1; }

    auto readU64 = [&buf](size_t off) {
        uint64_t v;
        memcpy(&v, buf.data() + off, sizeof(v));
        return v;
    };
    auto readU32 = [&buf](size_t off) {
        uint32_t v;
        memcpy(&v, buf.data() + off, sizeof(v));
        return v;
    };

    const size_t trailer = buf.size() - 16;
    const size_t tableOff = static_cast<size_t>(readU64(trailer));
    const uint64_t totalData = readU64(trailer + 8);
    if (tableOff >= trailer || totalData > tableOff) { fail(L"Package is damaged."); return 1; }

    const size_t count = readU32(tableOff);
    std::vector<FileEntry> entries;
    size_t cursor = tableOff + 4;
    bool foundExe = false;
    Hasher fp;
    const size_t tableEnd = trailer;   // the table ends right before the 16-byte trailer
    size_t i = 0;
    for (; i < count && cursor + 14 <= tableEnd; ++i) {
        const bool isDir = (buf[cursor++] != 0);
        const bool compressed = (buf[cursor++] != 0);
        const uint32_t nameLen = readU32(cursor); cursor += 4;
        const uint32_t dataLen = readU32(cursor); cursor += 4;
        const uint32_t compLen = readU32(cursor); cursor += 4;
        if (cursor + nameLen > tableEnd) { fail(L"Package table is damaged."); return 1; }
        std::string name(reinterpret_cast<const char *>(buf.data() + cursor), nameLen);
        cursor += nameLen;
        entries.push_back({isDir, compressed, name, dataLen, compLen});
        if (!isDir && name == "Capson.exe") { foundExe = true; }

        const uint8_t dirFlag = isDir ? 1 : 0;
        const uint8_t compFlag = compressed ? 1 : 0;
        fp.write(&dirFlag, sizeof(dirFlag));
        fp.write(&compFlag, sizeof(compFlag));
        fp.write(name.data(), name.size());
        fp.writeU32(dataLen);
        fp.writeU32(compLen);
        if (name == ".capson-payload") { fail(L"Package contains a reserved file name."); return 1; }
    }
    if (i != count) { fail(L"Package table is damaged."); return 1; }
    if (!foundExe) { fail(L"Package is missing the application."); return 1; }

    std::wstring target;
    if (!localAppDataDir(target)) { fail(L"Could not resolve the local AppData folder."); return 1; }
    target += L"\\Capson\\app";
    const std::wstring exePath = target + L"\\Capson.exe";
    const std::wstring markerPath = target + L"\\.capson-payload";
    const std::string wantFp = toHex(fp.h);

    const std::vector<unsigned char> marker = readWholeFile(markerPath);
    const std::string haveFp(marker.begin(), marker.end());
    const bool upToDate = (haveFp == wantFp);

    if (!upToDate) {
        removeTree(target);
        ensureDir(target);
        const size_t dataOff = tableOff - static_cast<size_t>(totalData);
        size_t dataCursor = dataOff;
        for (const FileEntry &entry : entries) {
            const std::wstring full = target + L"\\" + toWide(entry.name);
            if (entry.isDir) {
                ensureDir(full);
                continue;
            }
            const size_t sep = full.find_last_of(L"\\/");
            ensureDir(sep == std::wstring::npos ? target : full.substr(0, sep));

            const unsigned char *src = buf.data() + dataCursor;
            if (entry.compressed) {
                std::vector<unsigned char> out;
                inflateRaw(src, entry.compSize, entry.size, out);
                writeFile(full, out.data(), out.size());
            } else {
                writeFile(full, src, entry.compSize);
            }
            dataCursor += entry.compSize;
        }
        writeFile(markerPath, reinterpret_cast<const unsigned char *>(wantFp.data()), wantFp.size());
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(exePath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr,
                        target.c_str(), &si, &pi)) {
        fail(L"Could not start Capson. Error " + std::to_wstring(GetLastError()) + L".");
        return 1;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}