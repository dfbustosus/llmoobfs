# 0002 — Primeiro marco nativo

Data: 2026-10-10, America/Sao_Paulo. Estado: em execução.

## Pedido

Criar imagem MoobFS e módulo nativo que monte no Linux e leia dois arquivos com conteúdo compartilhado. Antes, estabelecer histórico duradouro das decisões e construção do projeto.

## Preparação

- Criados política de continuidade, diário e ADRs 0001–0003.
- Confirmados macOS arm64 e Docker Desktop com backend Linux arm64.
- Escolhido formato experimental M1 somente para leitura, distinguido do MVP transacional proposto.
- A validação será feita em kernel próprio de uma VM, sem carregar módulo no kernel do Docker Desktop.

## Resultados

Implementação e testes ainda em andamento. Completar este registro com comandos, versões, resultados e limitações antes do commit de implementação.
