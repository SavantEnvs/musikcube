// Functional oracle for the ffmpeg decoder plugin: decodes a file through
// GetDecoderFactory()->CreateDecoder()->Open()/GetBuffer() and prints the stream format,
// the decoded sample count and a rounded checksum of the PCM.
#include "sdk_stubs.h"
#include <musikcore/sdk/IDecoderFactory.h>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>

extern "C" musik::core::sdk::IDecoderFactory* GetDecoderFactory();
extern "C" void SetDebug(musik::core::sdk::IDebug* debug);

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <file>\n", argv[0]); return 2; }
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    static fuzzsdk::NullDebug dbg;
    SetDebug(&dbg);
    auto factory = GetDecoderFactory();
    printf("canhandle.wav=%d canhandle.xyz=%d\n", factory->CanHandle(".wav"), factory->CanHandle(".xyz"));
    auto decoder = factory->CreateDecoder();
    fuzzsdk::MemStream stream(bytes.data(), bytes.size(), argv[1], ".wav");
    if (!decoder->Open(&stream)) { printf("open=false\n"); return 1; }
    printf("duration=%.3f\n", decoder->GetDuration());
    fuzzsdk::VecBuffer buf;
    long total = 0; double sum = 0, peak = 0;
    while (decoder->GetBuffer(&buf)) {
        for (long i = 0; i < buf.Samples(); i++) {
            float s = buf.BufferPointer()[i];
            sum += std::fabs(s);
            if (std::fabs(s) > peak) peak = std::fabs(s);
        }
        total += buf.Samples();
    }
    printf("rate=%ld channels=%d samples=%ld abssum=%.1f peak=%.3f\n",
           buf.SampleRate(), buf.Channels(), total, sum, peak);
    decoder->Release();
    return 0;
}
