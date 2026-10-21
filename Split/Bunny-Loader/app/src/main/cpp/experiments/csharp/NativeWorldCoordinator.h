#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace bl::csharp::worldsave {

bool encodeWorldBase64(const std::vector<uint8_t>& bytes, std::string* encoded);
bool decodeWorldBase64(const std::string& encoded, std::vector<uint8_t>* bytes);

// Apenas para hooks internos do jogo. O mod nunca escolhe caminho ou epoch.
// Falha de qualquer fase recusa escrita em vez de sobrescrever dados existentes.
bool enterManagedWorld(uint64_t epoch, bool authoritative, std::string* error);
bool saveManagedWorld(uint64_t epoch, bool authoritative, std::string* error);
bool leaveManagedWorld(uint64_t epoch, std::string* error);
}
