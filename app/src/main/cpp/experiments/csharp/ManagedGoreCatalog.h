#pragma once

#include "experiments/csharp/ManagedGoreDiscovery.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp::gore {

struct Entry {
    std::string modId;
    Source source;
    int type = 0;
};

// Catalogo por mod. Reservar o tipo antes de aumentar as tabelas do Terraria
// permite verificar todas as colisões sem tocar na memoria do IL2CPP.
// Depois do commit, os numeros de tipo sao imutaveis durante a sessao.
class Catalog {
public:
    bool stage(const std::string& modId, const std::vector<Source>& sources,
               std::string* error) {
        if (!error) return false;
        error->clear();
        if (allocated_) {
            *error = "nao e possivel adicionar gores apos reservar os tipos";
            return false;
        }
        if (modId.empty() || modId.size() > 128 ||
            modId.find_first_of("/\\\t\r\n") != std::string::npos) {
            *error = "identificador de mod invalido";
            return false;
        }
        if (sources.size() > 256 || entries_.size() > 4096 - sources.size()) {
            *error = "limite global de gores excedido";
            return false;
        }
        std::map<std::pair<std::string, std::string>, size_t> pending = keys_;
        for (size_t i = 0; i < sources.size(); ++i) {
            const auto& source = sources[i];
            if (source.name.empty() || source.name.size() > 180 ||
                source.relativePath.empty() || source.absolutePath.empty() ||
                source.byteCount == 0 || source.width == 0 || source.height == 0) {
                *error = "registro de gore sem PNG validado";
                return false;
            }
            const auto id = std::make_pair(modId, source.name);
            if (!pending.emplace(id, entries_.size() + i).second) {
                *error = "gore duplicado dentro do mod: " + modId + "/" + source.name;
                return false;
            }
        }
        // Trocar o indice somente apos validar o lote inteiro.
        keys_ = std::move(pending);
        for (const auto& source : sources)
            entries_.push_back({modId, source, 0});
        return true;
    }

    bool allocate(int firstType, int maximumTypeExclusive, std::string* error) {
        if (!error) return false;
        error->clear();
        if (firstType < 1 || maximumTypeExclusive <= firstType ||
            entries_.size() > static_cast<size_t>(maximumTypeExclusive - firstType)) {
            *error = "tipo inicial ou capacidade de GoreID invalida";
            return false;
        }
        if (allocated_) {
            if (firstType != first_ || maximumTypeExclusive < end_) {
                *error = "o catalogo nao pode realocar tipos durante a sessao";
                return false;
            }
            return true;
        }
        first_ = firstType;
        end_ = firstType + static_cast<int>(entries_.size());
        for (size_t i = 0; i < entries_.size(); ++i)
            entries_[i].type = first_ + static_cast<int>(i);
        allocated_ = true;
        return true;
    }

    int typeOf(const std::string& modId, const std::string& name) const {
        const auto match = keys_.find({modId, name});
        if (match == keys_.end()) return 0;
        return entries_[match->second].type;
    }

    const std::vector<Entry>& entries() const { return entries_; }
    bool allocated() const { return allocated_; }
    int firstType() const { return first_; }
    int endType() const { return end_; }

    void clear() {
        entries_.clear();
        keys_.clear();
        first_ = end_ = 0;
        allocated_ = false;
    }

private:
    std::vector<Entry> entries_;
    std::map<std::pair<std::string, std::string>, size_t> keys_;
    int first_ = 0;
    int end_ = 0;
    bool allocated_ = false;
};

}
