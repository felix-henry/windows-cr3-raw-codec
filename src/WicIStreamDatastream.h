#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <libraw/libraw.h>
#include <array>
#include <cstdint>
#include <vector>

// Adapts a COM IStream* (provided by WIC) to LibRaw_abstract_datastream with
// a 4x64 KB LRU block cache so random-access ISOBMFF/EXIF/CRX header reads
// stay fast on NAS shares and SD cards without copying the entire file.
class WicIStreamDatastream final : public LibRaw_abstract_datastream
{
public:
    explicit WicIStreamDatastream(IStream* stream);
    ~WicIStreamDatastream() override;

    WicIStreamDatastream(const WicIStreamDatastream&) = delete;
    WicIStreamDatastream& operator=(const WicIStreamDatastream&) = delete;

    int valid() override;
    int read(void* ptr, size_t size, size_t nmemb) override;
    int seek(INT64 offset, int whence) override;
    INT64 tell() override;
    INT64 size() override;
    int get_char() override;
    char* gets(char* str, int sz) override;
    int scanf_one(const char* fmt, void* val) override;
    int eof() override;
    int jpeg_src(void*) override { return -1; }
    void buffering_off() override {}
    void buffering_on() override {}
    bool is_buffered() override { return false; }
    int lock() override { return 1; }
    void unlock() override {}
    const char* fname() override { return nullptr; }
#ifdef LIBRAW_WIN32_UNICODEPATHS
    const wchar_t* wfname() override { return nullptr; }
#endif

private:
    struct CacheSlot
    {
        INT64 blockStart = -1;
        INT64 blockEnd = -1;
        uint64_t lastUsedSeq = 0;
        std::vector<uint8_t> data;
    };

    static constexpr size_t kBlockSize = 64 * 1024;
    static constexpr size_t kCacheSlots = 4;

    CacheSlot* findSlotForOffset(INT64 offset);
    CacheSlot* loadBlockForOffset(INT64 offset);
    bool seekUnderlying(INT64 offset);

    IStream* stream_ = nullptr;
    INT64 totalSize_ = 0;
    INT64 pos_ = 0;
    INT64 streamPos_ = -1;
    uint64_t accessSeq_ = 0;
    std::array<CacheSlot, kCacheSlots> cache_{};
    bool valid_ = false;
};
