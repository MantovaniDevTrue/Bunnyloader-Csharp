#pragma once

#include "experiments/csharp/ManagedGoreDiscovery.h"
#include <string>
#include <vector>
struct Il2CppObject;

namespace bl::csharp::gore {

// Discovery only queues CPU metadata; textures are created by the game-thread pump.
bool stage(const std::string& modId, const std::vector<Source>& sources, std::string* error);
bool pendingInstall();

// Chamado somente na thread do jogo, depois da descoberta de arquivos.
// O registro e a carga das texturas nunca executam no worker do Mono.
bool install(const std::string& modId, const std::vector<Source>& sources,
             std::string* error);

struct DeclaredGore {
    std::string name;
    int type = 0;
    Il2CppObject* asset = nullptr;
    int specialAI = 0;
    bool safe = false;
};
// Adopt an already published declaration into the same lookup/repair catalog.
bool adoptDeclared(const std::string& modId, const std::vector<DeclaredGore>& entries,
                   std::string* error);

// Consultas sao lock-free: podem ser usadas pelo SDK C# no worker MonoVM.
int typeOf(const std::string& modId, const std::string& name);

// Ciclo de vida do mundo: limpa os efeitos pendentes para nao nascerem
// em outro mundo quando o jogador troca de personagem/sessao.
void worldBoundary();
void worldReady();
void pumpOnGameThread();

}
