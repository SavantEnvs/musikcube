// Functional oracle for the taglib plugin: reads a file through GetTagReader()->Read()
// and prints every tag the reader stored (sorted), plus thumbnail/replaygain facts.
#include "sdk_stubs.h"
#include <musikcore/sdk/ITagReader.h>
#include <cstdio>

extern "C" musik::core::sdk::ITagReader* GetTagReader();

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <file>\n", argv[0]); return 2; }
    auto reader = GetTagReader();
    fuzzsdk::MapTagStore store;
    if (!reader->Read(argv[1], &store)) { printf("read=false\n"); return 1; }
    for (auto& kv : store.values) printf("%s=%s\n", kv.first.c_str(), kv.second.c_str());
    printf("thumbnail=%zu\n", store.hasThumb ? store.thumbnail.size() : (size_t) 0);
    if (store.hasGain) printf("replaygain.track=%.2f\n", store.gain.trackGain);
    reader->Release();
    return 0;
}
