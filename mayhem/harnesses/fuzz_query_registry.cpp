// Fuzzes musikcore's library-query deserializers through QueryRegistry::CreateLocalQueryFor(),
// the entry point the remote server uses for a client's raw query ("send_raw_query":
// LocalMetadataProxy::SendRawQuery). Every *Query::DeserializeQuery parses the request JSON
// (options, predicate lists, limit/offset, track ids, playlist payloads) into a query object;
// the harness then re-serializes it (SerializeQuery) and feeds the same bytes to
// DeserializeResult (the client-side parse of a remote library's response).
//
// Input: byte 0 picks the query name (one of the registry's kQueryName values), the rest is
// the raw JSON request. Exceptions are caught like SendRawQuery does (catch (...)).
// The queries are bound to FuzzLibrary, a minimal ILibrary that starts no indexer, no thread and
// opens no database: Deserialize/Serialize only read the library's Id() (LibraryTrack) and keep
// the pointer. Enqueue/EnqueueAndWait (reached only from TrackList::Get when a result is
// serialized with full track metadata) run nothing and return, which is what an empty library
// yields too, so the track stays uncached. Nothing persists across inputs except the immutable
// stub, so each input's result depends only on that input.
#include <musikcore/library/QueryRegistry.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/library/IQuery.h>
#include <cstdint>
#include <cstdlib>
#include <string>

using namespace musik::core;

static const char* kNames[] = {
    "AlbumListQuery", "AllCategoriesQuery", "AppendPlaylistQuery", "GetPlaylistQuery",
    "CategoryListQuery", "CategoryTrackListQuery", "DeletePlaylistQuery",
    "DirectoryTrackListQuery", "LyricsQuery", "MarkTrackPlayedQuery", "SavePlaylistQuery",
    "SearchTrackListQuery", "SetTrackRatingQuery", "TrackMetadataBatchQuery", "TrackMetadataQuery",
};

namespace {
class FuzzLibrary : public ILibrary, public ILibrary::IResourceLocator {
  public:
    int Enqueue(QueryPtr, Callback) override { return -1; }
    int EnqueueAndWait(QueryPtr, size_t, Callback) override { return -1; }
    IIndexer* Indexer() override { return nullptr; }
    int Id() override { return 0; }
    const std::string& Name() override { return name; }
    void SetMessageQueue(runtime::IMessageQueue&) override {}
    runtime::IMessageQueue& GetMessageQueue() override { return queue; }
    IResourceLocator& GetResourceLocator() override { return *this; }
    bool IsConfigured() override { return true; }
    ConnectionState GetConnectionState() const override { return ConnectionState::Connected; }
    Type GetType() const override { return Type::Local; }
    void Close() override {}
    std::string GetTrackUri(sdk::ITrack*, const std::string& defaultUri) override { return defaultUri; }
  private:
    std::string name{"fuzz"};
    runtime::MessageQueue queue;
};
}

static ILibraryPtr gLibrary = std::make_shared<FuzzLibrary>();

// musikcore resolves its data directory from HOME / XDG_CONFIG_HOME; point both at a private
// scratch dir so no code path can read or write the host user's musikcube config.
extern "C" int LLVMFuzzerInitialize(int*, char***) {
    const char* tmp = getenv("TMPDIR");
    std::string dir = std::string(tmp && *tmp ? tmp : "/tmp") + "/musikcube-fuzz-XXXXXX";
    if (mkdtemp(&dir[0])) {
        setenv("XDG_CONFIG_HOME", dir.c_str(), 1);
        setenv("HOME", dir.c_str(), 1);
    }
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return 0;
    std::string name = kNames[data[0] % (sizeof(kNames) / sizeof(kNames[0]))];
    std::string json(reinterpret_cast<const char*>(data + 1), size - 1);
    std::shared_ptr<db::ISerializableQuery> query;
    try {
        query = library::QueryRegistry::CreateLocalQueryFor(name, json, gLibrary);
    }
    catch (...) {
        return 0;
    }
    if (!query) return 0;
    try {
        query->SerializeQuery();
    }
    catch (...) {}
    try {
        query->DeserializeResult(json);
        query->SerializeResult();
    }
    catch (...) {}
    return 0;
}
