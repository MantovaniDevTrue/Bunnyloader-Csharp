#pragma once

namespace bl::runtime {

/**
 * Estilos de agua de mod (ModWaterStyle): numeros depois dos 15 do jogo.
 *
 * O Main.DrawWaters do celular anda pelos estilos em dois lacos com o 15
 * compilado: o que sobe e desce o Main.liquidAlpha e monta a lista
 * activeLiquidAlpha (a agua atras de rampas e meios blocos, no TileDrawing), e
 * o que desenha os estilos que estao sumindo. O primeiro indexa o array pelo
 * endereco (`cmp x, #15 + 8`, 8 = o cabecalho em floats) e sai no b.ne; o
 * segundo testa no comeco (`cmp x, #15; b.eq`). Trocando os dois, o proprio
 * jogo faz o fade e o desenho dos estilos de mod, como o laco
 * `WaterStylesLoader.TotalCount` do tModLoader.
 *
 * As tabelas de texturas devem estar completas ANTES desta chamada. O
 * servico WaterAlphaTables prepara liquidAlpha/activeLiquidAlpha juntos e o
 * guard nativo verifica esses arrays antes de cada DrawWaters, inclusive
 * depois de recriar LocalUserGameState. Nao depende de executar uma VM.
 */
constexpr int kVanillaWaterStyleCount = 15;   // Main.maxLiquidTypes
constexpr int kMaxWaterStyleCount = 4095-8;   // limite + cabecalho precisa caber em imm12

/**
 * Os lacos do Main.DrawWaters passam a ir ate `total` (>= 15). Pode ser
 * chamado de novo com um total maior. Na thread do jogo. Devolve false se o
 * codigo nao for o esperado (outra versao do jogo): nada e escrito. As cinco
 * tabelas sao verificadas por tipo/capacidade antes da publicacao. Os dois
 * padroes e permissoes sao preparados juntos; nunca se escreve so um laco.
 * Uma falha do SO ao restaurar RX e relatada separadamente: nesse caso ambos
 * os lacos ja foram aplicados, a contagem real e preservada e a proxima
 * tentativa primeiro repara as permissoes. Nao existe sucesso ficticio.
 */
bool setWaterStyleCount(int total);

} // namespace bl::runtime
