# 0008 — Infraestrutura do kernel reutilizável e especificação do M2 com escrita POSIX

Data: 2026-10-10, America/Sao_Paulo.

## Pedido

Pesquisar módulos e infraestrutura de kernel maduros que evitem reescrever a roda e, com isso,
especificar o M2 com escrita POSIX (consequência da leitura do ai-memory, registro 0007).

## Método e evidências

Verificação direta no código-fonte do Linux v6.12 (símbolos exportados em `fs/jbd2`,
`fs/iomap/buffered-io.c`, `fs/remap_range.c`, `fs/xattr.c`, `fs/mbcache.c`, `fs/verity`) e no
`build/vm/kernel.config` do kernel Debian da VM (`JBD2=m`, `FS_IOMAP=y`, `FS_VERITY=y`,
`DM_LOG_WRITES=m`, `DM_FLAKEY=m`, `DM_THIN_PROVISIONING=m`, `DM_SNAPSHOT=m`). Nenhum código
foi compilado ou executado nesta sessão.

## Decisões (propostas, ADR-0006)

Escrita POSIX completa (sem hard links e symlinks); journaling por jbd2; dados por iomap com
protótipo inicial para validar a combinação; compartilhamento por `remap_file_range`
(`FICLONE`/`FIDEDUPERANGE`) com copy-on-write; atributos tipados como xattr `user.moob.*` com
`mbcache`; índice em RAM e `QUERY` por ioctl; formato novo `MOOBFSW2` com importação de M1;
aceitação com git + SQLite + ai-memory. A interface de escrita por ioctl da proposta original
fica superada; o documento é preservado.

## Mudanças

- `docs/kernel-infrastructure.md`, `docs/decisions/0006-m2-posix-writes.md`, `docs/spec-m2.md`
  (novos); `ROADMAP.md`, `README.md`, `docs/ecosystem.md`, índice do histórico, `CHANGELOG.md`.

## Limitações

A combinação iomap + jbd2 não tem precedente in-tree; a fase 2a existe para decidir isso antes
do formato final. Limites numéricos (16 GiB, 65.536 inodes, 4 KiB de xattr) são tetos de
experimento, não medidos. O índice em RAM não é persistente por decisão.

## Próximo passo

Fase 2a: protótipo mínimo iomap + jbd2 em VM, com `fsx` e `dm-log-writes`, antes de qualquer
código de formato v2. Em paralelo, fuzzing de `common/`.
