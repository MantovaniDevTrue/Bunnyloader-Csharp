#pragma once
#include <cstdint>

namespace bl::runtime::tile_draw_shape {
inline constexpr uintptr_t maxCoordinateRows = 256;

constexpr bool valid(int rows, uintptr_t heightsLength) {
    return rows > 0 && static_cast<uintptr_t>(rows) <= maxCoordinateRows &&
           heightsLength > 0 && heightsLength <= maxCoordinateRows;
}

inline constexpr int defaultCoordinateHeight = 16;
inline constexpr int maxCoordinateHeight = 256;
inline constexpr int maxCoordinatePadding = 64;

// Alturas por linha SEM ler o Int32[] CoordinateHeights do IL2CPP (no Android
// 301720 o objeto devolvido pelo getter nao passou em nenhuma checagem de array:
// length com cara de ponteiro, classe de elemento ausente, classe de runtime
// desconhecida). TileObjectData.Calculate faz
//   CoordinateFullHeight = soma(CoordinateHeights[i] + CoordinatePadding),
// entao a soma das alturas e exata. As linhas sao 16 e a ultima leva o resto:
// e o formato dos tiles nativos do ExampleMod ({16,18} do sapling, altura unica
// do workbench, estilos vanilla de 16). Um tile com linhas do meio diferentes
// de 16 ficaria com a soma certa e a divisao aproximada.
constexpr bool reconstructHeights(int rows, int fullHeight, int padding,
                                  int* out, int capacity) {
    if (rows <= 0 || static_cast<uintptr_t>(rows) > maxCoordinateRows ||
        capacity < rows || padding < 0 || padding > maxCoordinatePadding ||
        fullHeight <= 0)
        return false;
    const long long sum = static_cast<long long>(fullHeight) -
                          static_cast<long long>(padding) * rows;
    const long long last = sum - static_cast<long long>(defaultCoordinateHeight) * (rows - 1);
    if (last < 1 || last > maxCoordinateHeight) return false;
    for (int i = 0; i + 1 < rows; ++i) out[i] = defaultCoordinateHeight;
    out[rows - 1] = static_cast<int>(last);
    return true;
}
}
