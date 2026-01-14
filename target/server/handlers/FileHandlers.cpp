#include "FileHandlers.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../../common/utils/Base64.h"

namespace server {

namespace {

bool IsSafeFilename(const std::string& name) {
    if (name.empty() || name.size() > 128) {
        return false;
    }
    if (name.find("..") != std::string::npos) {
        return false;
    }
    for (char c : name) {
        if (c == '/' || c == '\\') {
            return false;
        }
        const bool ok = (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') ||
                        c == '_' || c == '-' || c == '.';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::string JoinPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) {
        return name;
    }
    if (dir.back() == '/' || dir.back() == '\\') {
        return dir + name;
    }
    return dir + "/" + name;
}

bool FileExists(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    return fin.good();
}

std::string MakeUniqueName(const std::string& dir, const std::string& name) {
    const std::string::size_type dot = name.find_last_of('.');
    const std::string base = (dot == std::string::npos) ? name : name.substr(0, dot);
    const std::string ext = (dot == std::string::npos) ? "" : name.substr(dot);

    for (int i = 1; i <= 1000; ++i) {
        std::ostringstream oss;
        oss << base << "_" << i << ext;
        const std::string candidate = oss.str();
        if (!FileExists(JoinPath(dir, candidate))) {
            return candidate;
        }
    }
    return base + "_upload" + ext;
}

std::string NewUploadId() {
    static uint64_t counter = 0;
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    oss << "U" << now << "_" << (++counter);
    return oss.str();
}

void ResetUpload(Session& session, bool removeTemp) {
    auto& st = session.upload();
    if (removeTemp && !st.tempPath.empty()) {
        std::remove(st.tempPath.c_str());
    }
    st.reset();
}

std::string NewDownloadId() {
    static uint64_t counter = 0;
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    oss << "D" << now << "_" << (++counter);
    return oss.str();
}

void ResetDownload(Session& session) {
    auto& st = session.download();
    st.reset();
}

void SetString(protocol::JsonObject& obj, const std::string& key, const std::string& value) {
    obj.fields[key] = protocol::MakeString(value);
}

void SetNumber(protocol::JsonObject& obj, const std::string& key, int64_t value) {
    obj.fields[key] = protocol::MakeNumber(value);
}

void SetBool(protocol::JsonObject& obj, const std::string& key, bool value) {
    obj.fields[key] = protocol::MakeBool(value);
}

} // namespace

void RegisterFileHandlers(CommandRouter& router, const ServerConfig& config) {
    router.RegisterCommand("LIST_FILES", Session::Level::High,
        [config](const protocol::RequestMessage&, Session&, protocol::ResponseMessage& resp) {
            protocol::JsonValue arr = protocol::MakeArray();
            std::error_code ec;
            size_t count = 0;
            for (const auto& entry : std::filesystem::directory_iterator(config.storageDir, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_regular_file()) {
                    continue;
                }
                const std::string name = entry.path().filename().string();
                if (name.size() >= 5 && name.substr(name.size() - 5) == ".part") {
                    continue;
                }
                if (arr.a) {
                    arr.a->items.push_back(protocol::MakeString(name));
                }
                if (++count >= 1000) {
                    break;
                }
            }

            if (ec) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::InternalError;
                resp.msg = "list files failed";
                resp.data.fields.clear();
                return;
            }

            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "OK";
            resp.data.fields.clear();
            resp.data.fields["files"] = arr;
        });

    router.RegisterCommand("UPLOAD_INIT", Session::Level::High,
        [config](const protocol::RequestMessage& req, Session& session, protocol::ResponseMessage& resp) {
            auto& st = session.upload();
            if (st.inProgress) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "upload already in progress";
                resp.data.fields.clear();
                return;
            }

            std::string filename;
            if (!protocol::GetString(req.args, "filename", filename) || !IsSafeFilename(filename)) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "invalid filename";
                resp.data.fields.clear();
                return;
            }

            int64_t fileSize = 0;
            if (!protocol::GetNumber(req.args, "file_size", fileSize) || fileSize <= 0) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "invalid file_size";
                resp.data.fields.clear();
                return;
            }

            if (static_cast<uint64_t>(fileSize) > config.maxFileSize) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "file too large";
                resp.data.fields.clear();
                return;
            }

            int64_t chunkSize = 0;
            if (protocol::GetNumber(req.args, "chunk_size", chunkSize) && chunkSize > 0) {
                if (chunkSize > config.maxChunkBytes) {
                    chunkSize = config.maxChunkBytes;
                }
            } else {
                chunkSize = config.maxChunkBytes;
            }

            std::string finalName = filename;
            const std::string finalPath = JoinPath(config.storageDir, finalName);
            if (FileExists(finalPath)) {
                if (config.overwrite == "reject") {
                    resp.ok = false;
                    resp.code = protocol::ErrorCode::FileExists;
                    resp.msg = "file exists";
                    resp.data.fields.clear();
                    return;
                }
                if (config.overwrite == "rename") {
                    finalName = MakeUniqueName(config.storageDir, finalName);
                } else if (config.overwrite == "overwrite") {
                    std::remove(finalPath.c_str());
                }
            }

            const std::string tempPath = JoinPath(config.storageDir, finalName + ".part");
            std::remove(tempPath.c_str());

            auto stream = std::make_unique<std::ofstream>(tempPath, std::ios::binary | std::ios::out | std::ios::trunc);
            if (!stream->is_open()) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::InternalError;
                resp.msg = "open temp file failed";
                resp.data.fields.clear();
                return;
            }

            st.reset();
            st.inProgress = true;
            st.uploadId = NewUploadId();
            st.finalName = finalName;
            st.tempPath = tempPath;
            st.declaredSize = static_cast<uint64_t>(fileSize);
            st.receivedSize = 0;
            st.nextIndex = 0;
            st.chunkSize = static_cast<uint32_t>(chunkSize);
            st.stream = std::move(stream);

            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "upload_init_ok";
            resp.data.fields.clear();
            SetString(resp.data, "upload_id", st.uploadId);
            SetNumber(resp.data, "chunk_size", st.chunkSize);
            SetNumber(resp.data, "next_index", st.nextIndex);
        });

    router.RegisterCommand("UPLOAD_CHUNK", Session::Level::High,
        [config](const protocol::RequestMessage& req, Session& session, protocol::ResponseMessage& resp) {
            auto& st = session.upload();
            if (!st.inProgress) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "no upload in progress";
                resp.data.fields.clear();
                return;
            }

            std::string uploadId;
            if (!protocol::GetString(req.args, "upload_id", uploadId) || uploadId != st.uploadId) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "upload_id mismatch";
                resp.data.fields.clear();
                return;
            }

            int64_t index = -1;
            if (!protocol::GetNumber(req.args, "chunk_index", index) || index < 0) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "invalid chunk_index";
                resp.data.fields.clear();
                return;
            }

            if (static_cast<uint32_t>(index) != st.nextIndex) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "chunk_index mismatch";
                resp.data.fields.clear();
                SetNumber(resp.data, "expected_index", st.nextIndex);
                return;
            }

            std::string dataB64;
            if (!protocol::GetString(req.args, "data_b64", dataB64)) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "data_b64 required";
                resp.data.fields.clear();
                return;
            }

            std::vector<uint8_t> bytes;
            if (!util::Base64Decode(dataB64, bytes)) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "invalid base64";
                resp.data.fields.clear();
                return;
            }

            if (bytes.size() > config.maxChunkBytes) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "chunk too large";
                resp.data.fields.clear();
                return;
            }

            if (st.receivedSize + bytes.size() > st.declaredSize) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::SizeMismatch;
                resp.msg = "size overflow";
                resp.data.fields.clear();
                ResetUpload(session, true);
                return;
            }

            st.stream->write(reinterpret_cast<const char*>(bytes.data()),
                             static_cast<std::streamsize>(bytes.size()));
            if (!(*st.stream)) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::InternalError;
                resp.msg = "write failed";
                resp.data.fields.clear();
                ResetUpload(session, true);
                return;
            }

            st.receivedSize += bytes.size();
            st.nextIndex += 1;

            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "chunk_ok";
            resp.data.fields.clear();
            SetNumber(resp.data, "received", static_cast<int64_t>(st.receivedSize));
            SetNumber(resp.data, "next_index", st.nextIndex);
        });

    router.RegisterCommand("UPLOAD_FINISH", Session::Level::High,
        [config](const protocol::RequestMessage& req, Session& session, protocol::ResponseMessage& resp) {
            auto& st = session.upload();
            if (!st.inProgress) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "no upload in progress";
                resp.data.fields.clear();
                return;
            }

            std::string uploadId;
            if (!protocol::GetString(req.args, "upload_id", uploadId) || uploadId != st.uploadId) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "upload_id mismatch";
                resp.data.fields.clear();
                return;
            }

            if (st.stream) {
                st.stream->flush();
                st.stream->close();
            }

            if (st.receivedSize != st.declaredSize) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::SizeMismatch;
                resp.msg = "size mismatch";
                resp.data.fields.clear();
                ResetUpload(session, true);
                return;
            }

            const std::string finalPath = JoinPath(config.storageDir, st.finalName);
            if (std::rename(st.tempPath.c_str(), finalPath.c_str()) != 0) {
                if (config.overwrite == "overwrite" && FileExists(finalPath)) {
                    std::remove(finalPath.c_str());
                    if (std::rename(st.tempPath.c_str(), finalPath.c_str()) != 0) {
                        resp.ok = false;
                        resp.code = protocol::ErrorCode::InternalError;
                        resp.msg = "rename failed";
                        resp.data.fields.clear();
                        ResetUpload(session, true);
                        return;
                    }
                } else {
                    resp.ok = false;
                    resp.code = protocol::ErrorCode::InternalError;
                    resp.msg = "rename failed";
                    resp.data.fields.clear();
                    ResetUpload(session, true);
                    return;
                }
            }

            const std::string finalName = st.finalName;
            const uint64_t finalSize = st.receivedSize;
            ResetUpload(session, false);

            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "upload_finish_ok";
            resp.data.fields.clear();
            SetString(resp.data, "filename", finalName);
            SetNumber(resp.data, "size", static_cast<int64_t>(finalSize));
        });

    router.RegisterCommand("DOWNLOAD_INIT", Session::Level::High,
        [config](const protocol::RequestMessage& req, Session& session, protocol::ResponseMessage& resp) {
            auto& st = session.download();
            if (st.inProgress) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "download already in progress";
                resp.data.fields.clear();
                return;
            }

            std::string filename;
            if (!protocol::GetString(req.args, "filename", filename) || !IsSafeFilename(filename)) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "invalid filename";
                resp.data.fields.clear();
                return;
            }

            const std::string path = JoinPath(config.storageDir, filename);
            std::error_code ec;
            const uint64_t fileSize = std::filesystem::file_size(path, ec);
            if (ec) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::FileNotFound;
                resp.msg = "file not found";
                resp.data.fields.clear();
                return;
            }

            int64_t chunkSize = 0;
            if (protocol::GetNumber(req.args, "chunk_size", chunkSize) && chunkSize > 0) {
                if (chunkSize > config.maxChunkBytes) {
                    chunkSize = config.maxChunkBytes;
                }
            } else {
                chunkSize = config.maxChunkBytes;
            }

            auto stream = std::make_unique<std::ifstream>(path, std::ios::binary);
            if (!stream->is_open()) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::InternalError;
                resp.msg = "open file failed";
                resp.data.fields.clear();
                return;
            }

            st.reset();
            st.inProgress = true;
            st.downloadId = NewDownloadId();
            st.filename = filename;
            st.path = path;
            st.fileSize = fileSize;
            st.nextIndex = 0;
            st.chunkSize = static_cast<uint32_t>(chunkSize);
            st.stream = std::move(stream);

            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "download_init_ok";
            resp.data.fields.clear();
            SetString(resp.data, "download_id", st.downloadId);
            SetNumber(resp.data, "file_size", static_cast<int64_t>(st.fileSize));
            SetNumber(resp.data, "chunk_size", st.chunkSize);
            SetNumber(resp.data, "next_index", st.nextIndex);
        });

    router.RegisterCommand("DOWNLOAD_CHUNK", Session::Level::High,
        [config](const protocol::RequestMessage& req, Session& session, protocol::ResponseMessage& resp) {
            auto& st = session.download();
            if (!st.inProgress) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "no download in progress";
                resp.data.fields.clear();
                return;
            }

            std::string downloadId;
            if (!protocol::GetString(req.args, "download_id", downloadId) || downloadId != st.downloadId) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "download_id mismatch";
                resp.data.fields.clear();
                return;
            }

            int64_t index = -1;
            if (!protocol::GetNumber(req.args, "chunk_index", index) || index < 0) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "invalid chunk_index";
                resp.data.fields.clear();
                return;
            }

            if (static_cast<uint32_t>(index) != st.nextIndex) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "chunk_index mismatch";
                resp.data.fields.clear();
                SetNumber(resp.data, "expected_index", st.nextIndex);
                return;
            }

            std::vector<uint8_t> buffer(static_cast<size_t>(st.chunkSize));
            st.stream->read(reinterpret_cast<char*>(buffer.data()),
                            static_cast<std::streamsize>(buffer.size()));
            const std::streamsize got = st.stream->gcount();
            if (got < 0) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::InternalError;
                resp.msg = "read failed";
                resp.data.fields.clear();
                ResetDownload(session);
                return;
            }
            buffer.resize(static_cast<size_t>(got));

            const bool isLast = (st.stream->eof() || st.stream->peek() == std::ifstream::traits_type::eof());
            const std::string b64 = util::Base64Encode(buffer);

            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "chunk_ok";
            resp.data.fields.clear();
            SetNumber(resp.data, "chunk_index", static_cast<int64_t>(st.nextIndex));
            SetString(resp.data, "data_b64", b64);
            SetBool(resp.data, "is_last", isLast);

            st.nextIndex += 1;
            if (isLast) {
                ResetDownload(session);
            }
        });

    router.RegisterCommand("DOWNLOAD_ABORT", Session::Level::High,
        [](const protocol::RequestMessage&, Session& session, protocol::ResponseMessage& resp) {
            auto& st = session.download();
            if (!st.inProgress) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::TransferStateError;
                resp.msg = "no download in progress";
                resp.data.fields.clear();
                return;
            }
            ResetDownload(session);
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "download_aborted";
            resp.data.fields.clear();
        });
}

} // namespace server
