// Fuzzes the taglib_plugin ITagReader (TaglibMetadataReader) through its exported
// GetTagReader() entry point, the way musikcore's Indexer reads a media file's tags:
// Read(path, ITagStore*). The reader dispatches on the file extension, so the first input
// byte picks one of the extensions CanRead() accepts; the rest is the file body.
#include "sdk_stubs.h"
#include <musikcore/sdk/ITagReader.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

extern "C" musik::core::sdk::ITagReader* GetTagReader();

static const char* kExt[] = {
    "mp3", "aac", "flac", "ogg", "oga", "opus", "m4a", "mp4", "alac", "wav",
    "aif", "aiff", "ape", "wv", "wma", "mpc", "spx", "tta",
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return 0;
    static musik::core::sdk::ITagReader* reader = GetTagReader();
    const char* ext = kExt[data[0] % (sizeof(kExt) / sizeof(kExt[0]))];

    const char* tmp = getenv("TMPDIR");
    std::string path = std::string(tmp && *tmp ? tmp : "/tmp") + "/musikcube-taglib-XXXXXX." + ext;
    int fd = mkstemps(&path[0], (int) strlen(ext) + 1);
    if (fd < 0) return 0;
    const uint8_t* body = data + 1;
    size_t left = size - 1;
    while (left > 0) {
        ssize_t w = write(fd, body, left);
        if (w <= 0) break;
        body += w;
        left -= (size_t) w;
    }
    close(fd);

    if (reader->CanRead(ext)) {
        fuzzsdk::MapTagStore store;
        reader->Read(path.c_str(), &store);
    }
    unlink(path.c_str());
    return 0;
}
