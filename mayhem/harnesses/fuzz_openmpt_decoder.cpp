// Fuzzes the libopenmptdecoder plugin's IDecoder (OpenMptDecoder) through its exported
// GetDecoderFactory(): Open() wraps the host IDataStream in an OpenMptDataStream (parsing the
// "libopenmpt://<track>/<file>" external id) and loads the module via read/seek/tell
// callbacks; then GetBuffer() renders audio, SetPosition() seeks, Release() tears down.
#include "sdk_stubs.h"
#include <musikcore/sdk/IDecoderFactory.h>
#include <string>

extern "C" musik::core::sdk::IDecoderFactory* GetDecoderFactory();
extern "C" void SetDebug(musik::core::sdk::IDebug* debug);

static fuzzsdk::NullDebug nullDebug;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    SetDebug(&nullDebug);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return 0;
    // first byte: subsong index the indexer would have encoded into the external id
    std::string uri = "libopenmpt://" + std::to_string(data[0] % 4) + "/fuzz.mod";
    auto decoder = GetDecoderFactory()->CreateDecoder();
    fuzzsdk::MemStream stream(data + 1, size - 1, uri, ".mod");
    if (decoder->Open(&stream)) {
        fuzzsdk::VecBuffer buffer;
        double duration = decoder->GetDuration();
        for (int i = 0; i < 8 && decoder->GetBuffer(&buffer); i++) {}
        if (duration > 0.0 && duration < 1e6) {
            decoder->SetPosition(duration / 2.0);
            decoder->GetBuffer(&buffer);
        }
        decoder->Exhausted();
    }
    decoder->Release();
    return 0;
}
