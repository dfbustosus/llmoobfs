# 0007 — Leitura técnica do ai-memory e posição relativa

Data: 2026-10-10, America/Sao_Paulo.

## Pedido

Entender o que `akitaonrails/ai-memory` pode contribuir e se o MoobFS seria uma opção de
armazenamento para ele; leitura completa, inclusive do código, incluindo a posição relativa
dos dois projetos e o momento adequado para um contato com o mantenedor.

## Método

Clone raso em diretório de rascunho, leitura direta de README, ARCHITECTURE, design-decisions,
vector-backend-policy, temporal, backup, wiki-migrations e comparison; três leituras de código
em paralelo (store + core, wiki + git + watcher, hooks + consolidate + llm + mcp). Nada foi
compilado nem executado. Métricas do repositório lidas da API do GitHub na data.

## Conclusões

- O ai-memory é aplicação: ocupa o papel de "cliente" da ADR-0005. Fonte de verdade é um
  diretório `wiki/` de markdown em git; SQLite é índice derivado. Não há abstração de backend.
- O MoobFS cabe embaixo de `wiki/`, mas hoje não pode hospedá-lo: o ai-memory precisa de
  escrita POSIX, `.git` gravável e SQLite com WAL. Isso impõe ao M2 escrita POSIX, não só
  ioctl. Registrado como requisito a levar para a especificação do M2.
- Git já deduplica blobs idênticos no commit e já versiona. O ganho do MoobFS precisa ser
  demonstrado: dedup entre caminhos sem commit, revisões nativas, atributos indexados.
- Caminho de integração sem alterar o núcleo deles: webhooks de admissão espelhando cada
  página via `PUT` em MoobFS.
- Contato com o mantenedor faz sentido só após M2 funcional, com números. Três divergências
  doc/código encontradas podem servir de primeiro contato verificável.

## Mudanças

- `docs/research-ai-memory.md` (novo), linha em `docs/ecosystem.md`, índice do histórico,
  `CHANGELOG.md`.

## Limitações

Leitura de código sem execução; contagens de linhas e proporções de teste são estimativas por
posição de `mod tests`. Versão lida: v2.6.3; o projeto muda rápido.

## Próximo passo

Especificação do M2 incorporando o requisito de escrita POSIX e o caso de uso "hospedar
`wiki/` do ai-memory", antes de código.
