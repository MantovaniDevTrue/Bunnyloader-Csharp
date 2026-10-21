#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace bl::csharp::worldsave {

enum class Result {
    Ok,
    Missing,
    InvalidSession,
    InvalidPath,
    Corrupt,
    IoError
};

// Este armazenamento recebe o caminho real do WorldFile IL2CPP, nunca um
// caminho escolhido pelo mod. Ele não invoca MonoVM ou código do Terraria.
class NativeWorldStore final {
public:
    bool beginWorld(uint64_t epoch, const std::string& worldPath, bool cloudSave);
    void endWorld(uint64_t epoch);
    Result read(uint64_t epoch, std::vector<uint8_t>* document);
    Result write(uint64_t epoch, const std::vector<uint8_t>& document);
    bool active(uint64_t epoch) const;

    static bool validDocument(const uint8_t* data, size_t size);

private:
    bool sessionValid(uint64_t epoch) const;

    mutable std::mutex mutex_;
    std::string folder_;
    std::string basename_;
    uint64_t epoch_ = 0;
    bool active_ = false;
};

} // namespace bl::csharp::worldsave
