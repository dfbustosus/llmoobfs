# 0005 — Modelo de memória e o Be File System como referência

Data: 2026-10-10, America/Sao_Paulo.

## Pedido

Retomar o projeto distinguindo o objetivo (memória/contexto para LLMs) do M1 implementado.
Na retomada foi apontada a lacuna: nenhum documento ligava o objetivo ao mecanismo. O usuário
apresentou o artigo do Be File System e pediu uma ADR usando-o como referência, perguntando
também se seria possível reutilizar o BFS em algum módulo.

## Decisão

ADR-0005 registra o modelo: memória como arquivo, revisões com atributos tipados expostos via
xattr, índices por atributo mantidos pelo módulo, consulta estruturada por ioctl e recuperação
semântica em espaço de usuário. O journaling de metadados do BFS vira referência para M2.

Sobre reuso: o driver `fs/befs` do Linux é somente leitura e não implementa atributos nem
índices (confirmado no Kconfig upstream); o BFS do Haiku é completo mas preso à API do Haiku.
Decidiu-se reutilizar o desenho e os casos de teste, não o código.

## Mudanças

- `docs/decisions/0005-memory-model-bfs-reference.md` (nova).
- `docs/history/README.md`, `CHANGELOG.md` e `ROADMAP.md` atualizados.

## Evidências

Consultas feitas nesta sessão: artigo da Wikipedia, `Documentation/filesystems/befs.rst`,
`fs/befs/Kconfig` e a entrada `BEFS FILE SYSTEM` em `MAINTAINERS` do Linux upstream.
Nenhum código mudou; nenhum teste foi executado nem é afirmado como executado.

## Limitações e próximo passo

A ADR é desenho, não implementação. Próximo passo: especificar o formato M2 (atributos,
índice, UAPI de `QUERY`, journaling) em documento próprio, com casos de entradas inválidas,
antes de qualquer código de escrita. O hardening do M1 segue independente.
