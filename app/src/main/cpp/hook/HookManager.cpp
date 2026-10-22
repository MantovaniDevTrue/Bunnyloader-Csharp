#include "hook/HookManager.h"
#include "core/Log.h"
#include "shadowhook.h"
#include <mutex>
#include <vector>
#include <cstring>

namespace bl::hook {

namespace {

/**
 * Todos os hooks de UM endereco.
 *
 * O ShadowHook roda em modo UNIQUE: um endereco aceita uma substituicao so, e
 * o segundo pedido e recusado. Era o que acontecia com Vida Cheia + Sem Queda,
 * os dois em Player.Update — o segundo mod ligado simplesmente nao carregava.
 *
 * Em vez de pedir de novo ao ShadowHook, o segundo hook entra NA CADEIA: ele
 * passa a ser o "original" do anterior, e o original dele e a funcao real.
 *
 *     jogo -> prologo patcheado -> R1 -> R2 -> ... -> Rn -> funcao real
 *
 * Todos os Ri hookam a mesma funcao, entao tem a mesma ABI e qualquer um pode
 * chamar o proximo como se fosse o original. Acrescentar um elo e trocar UM
 * ponteiro (o `original` de Rn-1): nada e desinstalado, nenhum trampolim do
 * ShadowHook e liberado com alguem possivelmente executando dentro dele.
 */
struct Chain {
    void* address = nullptr;
    void* stub = nullptr;             // o que o shadowhook_unhook quer
    std::vector<void**> originals;    // o `original` de cada elo, de fora pra dentro
    std::vector<unsigned char> proof;
};

std::mutex g_mutex;

std::vector<Chain>& chains() {
    static std::vector<Chain> list;
    return list;
}

std::string bytes(const void* data,size_t size) {
    constexpr char digits[]="0123456789abcdef";
    const auto* value=static_cast<const unsigned char*>(data);
    std::string result;result.reserve(size*2);
    for(size_t i=0;i<size;++i) {result+=digits[value[i]>>4];result+=digits[value[i]&15];}
    return result;
}
bool rejectedProof(void* address,const void* expected,size_t length,const std::vector<unsigned char>& actual) {
    BL_ERROR("[Native][ERROR][Hook] API=HookManager.installVerified Address=%p Stage=ValidateOriginalInstructions Expected=%s Actual=%s Result=Rejected Reason=%s",
        address,bytes(expected,length).c_str(),actual.empty()?"UnverifiedExistingChain":bytes(actual.data(),actual.size()).c_str(),
        actual.empty()?"OriginalProofUnavailable":"OriginalProofMismatch");
    return false;
}

bool installLocked(void* address,void* replacement,void** original,const void* expected,size_t length) {
    for (Chain& c : chains()) {
        if (c.address != address) continue;
        if(expected && (c.proof.size()!=length || std::memcmp(c.proof.data(),expected,length))) return rejectedProof(address,expected,length,c.proof);
        // O novo elo aponta para onde o ultimo apontava (a funcao real, via
        // trampolim) ANTES de ser publicado: quem ler o ponteiro do ultimo elo
        // a partir de agora ja encontra um elo pronto para seguir.
        void** last = c.originals.back();
        __atomic_store_n(original, __atomic_load_n(last, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
        __atomic_store_n(last, replacement, __ATOMIC_RELEASE);
        c.originals.push_back(original);
        BL_DEBUG("hook encadeado em %p (%zu no mesmo metodo)", address, c.originals.size());
        return true;
    }

    std::vector<unsigned char> proof;
    if(expected) {
        if(std::memcmp(address,expected,length)) {
            const auto* actual=static_cast<const unsigned char*>(address);
            return rejectedProof(address,expected,length,{actual,actual+length});
        }
        auto* bytes=static_cast<const unsigned char*>(expected);
        proof.assign(bytes,bytes+length);
    }
    void* stub = shadowhook_hook_func_addr(address, replacement, original);
    if (!stub) {
        BL_ERROR("Hook falhou em %p: %s", address,
                 shadowhook_to_errmsg(shadowhook_get_errno()));
        return false;
    }
    chains().push_back({address, stub, {original}, std::move(proof)});
    return true;
}
} // namespace

bool install(void* address, void* replacement, void** original) {
    if (!address || !replacement || !original) return false;
    std::lock_guard<std::mutex> guard(g_mutex);
    return installLocked(address,replacement,original,nullptr,0);
}

bool installVerified(void* address,const void* expected,size_t length,void* replacement,void** original) {
    if(!address || !expected || !replacement || !original || length==0 || length>64) return false;
    std::lock_guard<std::mutex> guard(g_mutex);
    return installLocked(address,replacement,original,expected,length);
}

bool install(const MethodInfo* method, void* replacement, void** original) {
    if (!method) return false;
    return install(il2cpp::methodPointer(method), replacement, original);
}

} // namespace bl::hook
