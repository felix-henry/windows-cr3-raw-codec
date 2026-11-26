#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <libraw/libraw.h>
#include <vector>
#include <cstdint>

// Adapts a COM IStream* (provided by WIC) to LibRaw_abstract_datastream with
// buffered read-ahead so header parsing and CRX decoding stay fast without
// copying the entire RAW file into memory.
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
    bool fillBuffer(INT64 offset);
    bool seekUnderlying(INT64 offset);

    static constexpr size_t kBufferSize = 64 * 1024;

    IStream* stream_ = nullptr;
    INT64 totalSize_ = 0;
    INT64 pos_ = 0;
    INT64 streamPos_ = -1;
    INT64 bufStart_ = 0;
    INT64 bufEnd_ = 0;
    std::vector<uint8_t> buffer_;
    bool valid_ = false;
};
