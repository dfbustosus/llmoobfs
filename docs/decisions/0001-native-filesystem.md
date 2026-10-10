# ADR-0001 — Filesystem montável nativo do Linux

Data: 2026-10-10. Estado: aceito pelo usuário.

## Contexto

O objetivo é experimentar armazenamento persistente compartilhado para memória de LLM com acesso por filesystem montado.

## Decisão

Implementar `moobfs.ko` como módulo do Linux, registrado no VFS, com formato próprio em dispositivo de blocos. Busca e processamento semântico podem existir em espaço de usuário, sem participar das leituras normais.

## Alternativas consideradas

- FUSE: reduz o custo inicial de desenvolvimento, mas foi explicitamente descartado pelo usuário.
- Biblioteca/serviço sobre filesystem existente: útil como motor de memória, mas não atende ao requisito de filesystem nativo montável.

## Consequências

É necessário compilar contra um kernel definido e testar montagem, I/O e entradas inválidas em VM. Remover FUSE não demonstra, por si só, ganho de desempenho. O primeiro marco é de leitura; o MVP completo inclui publicação e durabilidade.
