// Minimal implementations of the musikcore SDK interfaces a host (musikcore) hands to
// plugins: an in-memory IDataStream, a growable IBuffer, an IDebug sink and an ITagStore.
#pragma once
#include <musikcore/sdk/IDataStream.h>
#include <musikcore/sdk/IBuffer.h>
#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/ITagStore.h>
#include <cstring>
#include <map>
#include <string>
#include <memory>
#include <vector>

namespace fuzzsdk {

using namespace musik::core::sdk;

class MemStream : public IDataStream {
  public:
    MemStream(const uint8_t* data, size_t size, std::string uri, std::string type)
    : data(data), size(size), pos(0), uri(std::move(uri)), type(std::move(type)) {}
    bool Open(const char*, OpenFlags) override { return true; }
    bool Close() override { return true; }
    void Interrupt() override {}
    void Release() override {}
    bool Readable() override { return true; }
    bool Writable() override { return false; }
    PositionType Read(void* buffer, PositionType n) override {
        if (n <= 0 || pos >= size) return 0;
        size_t take = std::min<size_t>((size_t) n, size - pos);
        memcpy(buffer, data + pos, take);
        pos += take;
        return (PositionType) take;
    }
    PositionType Write(void*, PositionType) override { return 0; }
    bool SetPosition(PositionType p) override {
        if (p < 0 || (size_t) p > size) return false;
        pos = (size_t) p;
        return true;
    }
    PositionType Position() override { return (PositionType) pos; }
    bool Seekable() override { return true; }
    bool Eof() override { return pos >= size; }
    long Length() override { return (long) size; }
    const char* Type() override { return type.c_str(); }
    const char* Uri() override { return uri.c_str(); }
    bool CanPrefetch() override { return true; }
  private:
    const uint8_t* data;
    size_t size, pos;
    std::string uri, type;
};

// Growable sample buffer with musikcore's audio::Buffer semantics (Buffer::SetSamples /
// ResizeBuffer): the logical sample count changes freely, the backing store is reallocated to
// exactly the requested size only when it grows, and never shrinks.
class VecBuffer : public IBuffer {
  public:
    long SampleRate() const override { return rate; }
    void SetSampleRate(long r) override { rate = r; }
    int Channels() const override { return channels; }
    void SetChannels(int c) override { channels = c; }
    float* BufferPointer() const override { return store.get(); }
    long Samples() const override { return count; }
    void SetSamples(long n) override {
        count = n;
        if (n > capacity) { store.reset(new float[(size_t) n]()); capacity = n; }
    }
    long Bytes() const override { return (long) sizeof(float) * count; }
    void Release() override {}
  private:
    long rate = 0;
    int channels = 0;
    long count = 0, capacity = 0;
    std::unique_ptr<float[]> store;
};

class NullDebug : public IDebug {
  public:
    void Verbose(const char*, const char*) override {}
    void Info(const char*, const char*) override {}
    void Warning(const char*, const char*) override {}
    void Error(const char*, const char*) override {}
};

class MapTagStore : public ITagStore {
  public:
    void Retain() override {}
    void Release() override {}
    void SetValue(const char* key, const char* value) override {
        auto& v = values[key];
        if (!v.empty()) v += "|";
        v += value;
    }
    void ClearValue(const char* key) override { values.erase(key); }
    bool Contains(const char* key) override { return values.find(key) != values.end(); }
    void SetThumbnail(const char* d, long size) override { thumbnail.assign(d, d + (size > 0 ? size : 0)); hasThumb = true; }
    bool ContainsThumbnail() override { return hasThumb; }
    void SetReplayGain(const ReplayGain& rg) override { gain = rg; hasGain = true; }

    std::map<std::string, std::string> values;
    std::vector<char> thumbnail;
    bool hasThumb = false, hasGain = false;
    ReplayGain gain{};
};

}  // namespace fuzzsdk

#include <musikcore/sdk/IPreferences.h>
namespace fuzzsdk {
// Fresh preferences: every getter returns the caller's default, like a new musikcube profile.
class DefaultPreferences : public musik::core::sdk::IPreferences {
  public:
    void Release() override {}
    bool GetBool(const char*, bool d) override { return d; }
    int GetInt(const char*, int d) override { return d; }
    double GetDouble(const char*, double d) override { return d; }
    int GetString(const char*, char* dst, size_t size, const char* d) override {
        size_t n = strlen(d);
        if (dst && size > 0) { size_t c = std::min(n, size - 1); memcpy(dst, d, c); dst[c] = 0; }
        return (int) n + 1;
    }
    void SetBool(const char*, bool) override {}
    void SetInt(const char*, int) override {}
    void SetDouble(const char*, double) override {}
    void SetString(const char*, const char*) override {}
    void Save() override {}
};
}  // namespace fuzzsdk
