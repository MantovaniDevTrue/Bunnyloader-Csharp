# Contratos do Terraria Android 301720

Esta matriz foi obtida dos metadados IL2CPP v31 enviados para análise. Os binários proprietários do jogo não estão incluídos neste repositório.

A identidade é vinculada ao SHA-256 de global-metadata.dat e libil2cpp.so. Os dados verificáveis ficam em `terraria-mobile-301720.json`. O CI impede que nomes e constantes reconhecidamente incompatíveis voltem a ser assumidos pelo Bunny.

## Lote 1

- ExampleCostume: identificadores GoldFlame=228 e BlueFlare=187 são constantes short. O fallback resolve os IDs quando reflexão não materializa os campos literais.
- Exemplo do traje: DrawHead, HidesTopSkin, HidesArms e HidesBottomSkin não existem na versão Android. Usamos HidesHead como fallback de cabeça e MissingHand com sinal invertido para as mãos; a ocultação de tronco e pernas permanece no desenho padrão Android. Os seis slots visuais, o ciclo wet/normal e os efeitos continuam registrados. **A equivalência visual depende de QA no aparelho.**
- TileDraw e AnglerQuest: preservamos todos os contratos de ponteiros e tipos; a razão específica da rejeição do array agora aparece no log. Nenhuma cópia ou alteração da memória é liberada por esta instrumentação.

## Próximos lotes

Os metadados estáticos não fornecem diretamente offsets do runtime, ponteiros de métodos ou equivalência de efeitos de desenho. Use inspeção de ABI do Terraria carregado no dispositivo antes de gerar bindings executáveis.
