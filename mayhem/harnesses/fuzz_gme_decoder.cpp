// Fuzzes the gmedecoder plugin's IDecoder (GmeDecoder, libgme: NSF/SPC/GBS/VGM/AY/HES/KSS/SAP/GYM)
// through its exported GetDecoderFactory(): Open() wraps the host IDataStream in a GmeDataStream
// (parsing the "gme://<track>/<file>" external id the plugin's indexer emits), loads the whole
// stream with gme_open_data, reads track info, starts the track; then GetBuffer()/SetPosition().
#include "sdk_stubs.h"
#include <musikcore/sdk/IDecoderFactory.h>
#include <string>

extern "C" musik::core::sdk::IDecoderFactory* GetDecoderFactory();
extern "C" void SetDebug(musik::core::sdk::IDebug* debug);
extern "C" void SetPreferences(musik::core::sdk::IPreferences* prefs);

static fuzzsdk::NullDebug nullDebug;
static fuzzsdk::DefaultPreferences defaultPrefs;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    SetDebug(&nullDebug);         // the host sets both before any decoder is created
    SetPreferences(&defaultPrefs);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return 0;
    // first byte: track index encoded in the external id; the file name points nowhere so the
    // plugin's sibling-.m3u lookup finds nothing and the result depends only on the input
    std::string uri = "gme://" + std::to_string(data[0] % 8) + "/nonexistent-mayhem-dir/fuzz.vgm";
    auto decoder = GetDecoderFactory()->CreateDecoder();
    fuzzsdk::MemStream stream(data + 1, size - 1, uri, ".vgm");
    if (decoder->Open(&stream)) {
        fuzzsdk::VecBuffer buffer;
        for (int i = 0; i < 8 && decoder->GetBuffer(&buffer); i++) {}
        double duration = decoder->GetDuration();
        if (duration > 0.0 && duration < 1e6) {
            decoder->SetPosition(duration / 2.0);
            decoder->GetBuffer(&buffer);
        }
        decoder->Exhausted();
    }
    decoder->Release();
    return 0;
}
