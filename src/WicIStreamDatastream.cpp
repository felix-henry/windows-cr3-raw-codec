#include "WicIStreamDatastream.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

WicIStreamDatastream::WicIStreamDatastream(IStream* stream)
    : stream_(stream), buffer_(kBufferSize)
{
    if (!stream_)
        return;

    stream_->AddRef();

    STATSTG stat{};
    if (SUCCEEDED(stream_->Stat(&stat, STATFLAG_NONAME)))
    {
        totalSize_ = static_cast<INT64>(stat.cbSize.QuadPart);
        valid_ = seekUnderlying(0);
    }
    else
    {
        LARGE_INTEGER zero{};
        ULARGE_INTEGER endPos{};
        if (SUCCEEDED(stream_->Seek(zero, STREAM_SEEK_END, &endPos)))
        {
            totalSize_ = static_cast<INT64>(endPos.QuadPart);
            streamPos_ = totalSize_;
            valid_ = seekUnderlying(0);
        }
    }
}

WicIStreamDatastream::~WicIStreamDatastream()
{
    if (stream_)
    {
        stream_->Release();
        stream_ = nullptr;
    }
}

int WicIStreamDatastream::valid()
{
    return valid_ ? 1 : 0;
}

bool WicIStreamDatastream::seekUnderlying(INT64 offset)
{
    if (!stream_)
        return false;
    if (streamPos_ == offset)
        return true;

    LARGE_INTEGER li{};
    li.QuadPart = offset;
    ULARGE_INTEGER newPos{};
    if (FAILED(stream_->Seek(li, STREAM_SEEK_SET, &newPos)))
        return false;

    streamPos_ = static_cast<INT64>(newPos.QuadPart);
    return true;
}

bool WicIStreamDatastream::fillBuffer(INT64 offset)
{
    if (!valid_ || offset < 0 || offset >= totalSize_)
        return false;

    if (!seekUnderlying(offset))
        return false;

    const ULONG toRead = static_cast<ULONG>(std::min<INT64>(kBufferSize, totalSize_ - offset));
    ULONG bytesRead = 0;
    const HRESULT hr = stream_->Read(buffer_.data(), toRead, &bytesRead);
    if (FAILED(hr) || bytesRead == 0)
        return false;

    streamPos_ += bytesRead;
    bufStart_ = offset;
    bufEnd_ = offset + bytesRead;
    return true;
}

int WicIStreamDatastream::read(void* ptr, size_t size, size_t nmemb)
{
    if (!valid_ || !ptr || size == 0 || nmemb == 0)
        return 0;

    if (nmemb > (std::numeric_limits<size_t>::max)() / size)
        return 0;

    size_t remaining = size * nmemb;
    if (pos_ >= totalSize_)
        return 0;

    const INT64 avail = totalSize_ - pos_;
    if (static_cast<INT64>(remaining) > avail)
        remaining = static_cast<size_t>(avail);

    uint8_t* dst = static_cast<uint8_t*>(ptr);
    size_t totalCopied = 0;

    while (remaining > 0)
    {
        if (pos_ >= bufStart_ && pos_ < bufEnd_)
        {
            const size_t offsetInBuf = static_cast<size_t>(pos_ - bufStart_);
            const size_t chunk = std::min<size_t>(remaining, static_cast<size_t>(bufEnd_ - pos_));
            std::memcpy(dst + totalCopied, buffer_.data() + offsetInBuf, chunk);
            pos_ += static_cast<INT64>(chunk);
            totalCopied += chunk;
            remaining -= chunk;
            continue;
        }

        if (remaining >= kBufferSize)
        {
            if (!seekUnderlying(pos_))
                break;

            const ULONG chunkReq = static_cast<ULONG>(
                std::min<size_t>(remaining, static_cast<size_t>((std::numeric_limits<ULONG>::max)())));
            ULONG chunkRead = 0;
            const HRESULT hr = stream_->Read(dst + totalCopied, chunkReq, &chunkRead);
            if (FAILED(hr) || chunkRead == 0)
                break;

            streamPos_ += chunkRead;
            pos_ += chunkRead;
            totalCopied += chunkRead;
            remaining -= chunkRead;
        }
        else
        {
            if (!fillBuffer(pos_))
                break;
        }
    }

    return static_cast<int>(totalCopied / size);
}

int WicIStreamDatastream::seek(INT64 offset, int whence)
{
    if (!valid_)
        return -1;

    INT64 target = 0;
    switch (whence)
    {
    case SEEK_SET:
        target = offset;
        break;
    case SEEK_CUR:
        target = pos_ + offset;
        break;
    case SEEK_END:
        target = totalSize_ + offset;
        break;
    default:
        return -1;
    }

    if (target < 0)
        target = 0;
    else if (target > totalSize_)
        target = totalSize_;

    pos_ = target;
    return 0;
}

INT64 WicIStreamDatastream::tell()
{
    return pos_;
}

INT64 WicIStreamDatastream::size()
{
    return totalSize_;
}

int WicIStreamDatastream::get_char()
{
    if (!valid_ || pos_ >= totalSize_)
        return -1;

    if (pos_ < bufStart_ || pos_ >= bufEnd_)
    {
        if (!fillBuffer(pos_))
            return -1;
    }

    return buffer_[static_cast<size_t>(pos_++ - bufStart_)];
}

char* WicIStreamDatastream::gets(char* str, int sz)
{
    if (!valid_ || !str || sz < 1 || pos_ >= totalSize_)
        return nullptr;

    int i = 0;
    while (i < sz - 1)
    {
        const int ch = get_char();
        if (ch < 0)
            break;
        str[i++] = static_cast<char>(ch);
        if (ch == '\n')
            break;
    }
    if (i == 0)
        return nullptr;
    str[i] = '\0';
    return str;
}

int WicIStreamDatastream::scanf_one(const char* fmt, void* val)
{
    if (!valid_ || !fmt || !val)
        return EOF;

    char token[64]{};
    int ch = get_char();
    while (ch >= 0 && (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'))
        ch = get_char();
    if (ch < 0)
        return EOF;

    size_t idx = 0;
    while (ch >= 0 && ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' && idx < sizeof(token) - 1)
    {
        token[idx++] = static_cast<char>(ch);
        ch = get_char();
    }
    token[idx] = '\0';

    char* endPtr = nullptr;
    if (std::strcmp(fmt, "%d") == 0)
    {
        const long parsed = std::strtol(token, &endPtr, 10);
        if (endPtr == token)
            return EOF;
        *static_cast<int*>(val) = static_cast<int>(parsed);
        return 1;
    }

    const float parsed = std::strtof(token, &endPtr);
    if (endPtr == token)
        return EOF;
    *static_cast<float*>(val) = parsed;
    return 1;
}

int WicIStreamDatastream::eof()
{
    return (!valid_ || pos_ >= totalSize_) ? 1 : 0;
}
