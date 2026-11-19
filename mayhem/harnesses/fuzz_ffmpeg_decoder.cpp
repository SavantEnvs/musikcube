// Fuzzes the ffmpegdecoder plugin's IDecoder (FfmpegDecoder) through its exported
// GetDecoderFactory(): Open() over an in-memory IDataStream (its custom AVIO read/seek
// callbacks), then the GetBuffer() decode/resample loop, a seek, and Release() teardown.
#include "sdk_stubs.h"
#include <musikcore/sdk/IDecoderFactory.h>

extern "C" musik::core::sdk::IDecoderFactory* GetDecoderFactory();
extern "C" void SetDebug(musik::core::sdk::IDebug* debug);

static fuzzsdk::NullDebug nullDebug;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    SetDebug(&nullDebug);  // the host always provides an IDebug before decoding
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    auto factory = GetDecoderFactory();
    auto decoder = factory->CreateDecoder();
    fuzzsdk::MemStream stream(data, size, "fuzz.input", ".mp3");
    if (decoder->Open(&stream)) {
        fuzzsdk::VecBuffer buffer;
        double duration = decoder->GetDuration();
        for (int i = 0; i < 32 && decoder->GetBuffer(&buffer); i++) {}
        if (duration > 0.0 && duration < 1e6) {
            decoder->SetPosition(duration / 2.0);
            for (int i = 0; i < 4 && decoder->GetBuffer(&buffer); i++) {}
        }
        decoder->Exhausted();
    }
    decoder->Release();
    return 0;
}
