# Estado do Bunny Loader C#

Este arquivo é o índice estável de status referenciado pela documentação do repositório.
Os resultados de testes, a disponibilidade de APIs e os APKs publicados devem ser
verificados pelas versões, pelas PRs e pelos workflows correspondentes. A existência
de uma classe no SDK não significa, por si só, que ela foi validada no Android.

## Onde consultar

- [Visão geral do repositório](README.md)
- [SDK C#](SDK-CSharp.md)
- [Roadmap do SDK](SDK-ROADMAP.md)
- [Validação do SDK](SDK-VALIDACAO.md)
- [Orientações de build](BUILD.md)
- [Histórico](HISTORICO.md)

## Regras para novas APIs

1. Implementar a superfície pública e os contratos de erro do lote escolhido.
2. Incluir exemplo C# que compile, com explicações no próprio código.
3. Executar os testes específicos e os de regressão aplicáveis.
4. Não produzir APK de um lote antes de concluir os seus critérios de implementação.
5. Diferenciar claramente testes gerenciados de testes no Android real.

Lotes em revisão permanecem nas branches e nas PRs até que seus próprios gates
estejam concluídos. Este arquivo não substitui os relatórios de testes ou as
evidências de funcionamento em jogo.
