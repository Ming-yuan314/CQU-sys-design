#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>

namespace server {

class Session {
public:
    enum class Level {
        Guest = 0,
        Low,
        High
    };

    struct UploadState {
        bool inProgress = false;
        std::string uploadId;
        std::string finalName;
        std::string tempPath;
        uint64_t declaredSize = 0;
        uint64_t receivedSize = 0;
        uint32_t nextIndex = 0;
        uint32_t chunkSize = 0;
        std::unique_ptr<std::ofstream> stream;

        void reset() {
            if (stream) {
                stream->close();
                stream.reset();
            }
            inProgress = false;
            uploadId.clear();
            finalName.clear();
            tempPath.clear();
            declaredSize = 0;
            receivedSize = 0;
            nextIndex = 0;
            chunkSize = 0;
        }
    };

    struct DownloadState {
        bool inProgress = false;
        std::string downloadId;
        std::string filename;
        std::string path;
        uint64_t fileSize = 0;
        uint32_t nextIndex = 0;
        uint32_t chunkSize = 0;
        std::unique_ptr<std::ifstream> stream;

        void reset() {
            if (stream) {
                stream->close();
                stream.reset();
            }
            inProgress = false;
            downloadId.clear();
            filename.clear();
            path.clear();
            fileSize = 0;
            nextIndex = 0;
            chunkSize = 0;
        }
    };

    Level level() const { return level_; }
    void setLevel(Level level) { level_ = level; }

    const std::string& username() const { return username_; }
    void setUsername(const std::string& name) { username_ = name; }

    std::string levelString() const {
        switch (level_) {
        case Level::Guest:
            return "GUEST";
        case Level::Low:
            return "LOW";
        case Level::High:
            return "HIGH";
        default:
            return "GUEST";
        }
    }

    UploadState& upload() { return upload_; }
    const UploadState& upload() const { return upload_; }
    DownloadState& download() { return download_; }
    const DownloadState& download() const { return download_; }

private:
    Level level_ = Level::Guest;
    std::string username_;
    UploadState upload_;
    DownloadState download_;
};

} // namespace server
