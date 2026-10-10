# 0012 — Fase 2b: passagem completa sobre ext4, btrfs e XFS, com reflink delegado

Data: 2026-10-10, America/Sao_Paulo. Estado: implementado e validado na VM de teste.

## Pedido

Iniciar a fase 2b da especificação do M2: rename, links, xattr opacos, btrfs e XFS como
inferiores, reflink delegado. Antes disso o usuário pediu para corrigir a atribuição dos
commits no GitHub (ver seção própria).

## Implementação

- `layer/main.c` (agora 1.101 linhas): `rename` com `lock_rename` e `vfs_rename`
  (inclui `RENAME_NOREPLACE` e `RENAME_EXCHANGE` passados ao inferior); `link` e `symlink`
  por `vfs_link`/`vfs_symlink`, `get_link` por `vfs_get_link`; opção de montagem `nolinks`
  que devolve `EPERM` para ambos; xattr opaco em todos os namespaces por um único
  `xattr_handler` com prefixo vazio delegando a `vfs_getxattr`/`vfs_setxattr`/
  `vfs_removexattr`, `listxattr` por `vfs_listxattr`; `remap_file_range` delegando a
  `vfs_clone_file_range` e `vfs_dedupe_file_range_one`; `copy_file_range` delegando a
  `vfs_copy_file_range`; `fiemap` chamando o do inode inferior; inodes de symlink com
  operações próprias. Tudo com as credenciais do montador nas chamadas delegadas.
- `tests/layer.c` (257 linhas): acrescenta rename em cinco variantes, xattr (set, create
  exclusivo, replace, list, remove, ausente), links e symlinks (ou sua recusa com `nolinks`),
  `FICLONE` com verificação de `FIEMAP_EXTENT_SHARED` nos dois lados e copy-on-write do
  inferior, `FIDEDUPERANGE` com cópia byte a byte que passa a compartilhar extents, e
  `EOPNOTSUPP` onde o inferior não tem reflink.
- Harness: imagens ext4 (64 MiB), btrfs (256 MiB) e XFS (300 MiB) criadas no container como
  arquivos esparsos e anexadas ao QEMU como discos virtio; cadeia de módulos resolvida de
  `modules.dep` para `virtio_blk`, `ext4`, `btrfs` e `xfs` mais `crc32c_generic`; VM com
  512 MiB; `vm/init` repete, para cada inferior, teste de passagem, três execuções de `fsx`,
  comparação com a cópia inferior, remontagem com `nolinks` e novo teste; `vm/Dockerfile`
  ganha `btrfs-progs` e `xfsprogs`.

## Evidências executadas

`make vm-test`, kernel Debian `6.12.111+deb13-amd64`, primeira execução da fase 2b sem
correções. `build/vm/console.log`:

```text
MOOBFS_VM_PASS: shared extents, independent inodes, reads, readonly, remount, 15 malformed images
LAYER_LOWER_PASS: ext4 reflink=no
LAYER_LOWER_PASS: btrfs reflink=yes
LAYER_LOWER_PASS: xfs reflink=yes
MOOBFS_LAYER_PASS: pass-through over ext4, btrfs and xfs; layer tests; fsx 9000 ops each; reflink on btrfs/xfs; nolinks; unload
```

Cadeia de módulos carregada: `crc32c_generic virtio_blk crc16 mbcache jbd2 ext4 xor raid6_pq
libcrc32c btrfs xfs`. Nenhum `BUG`, `Oops`, `WARNING` ou `Call Trace` no `dmesg`. Tempo de
guest até o desligamento: 39 s sob TCG. SHA-256 de `moobfs_layer.ko`:
`8d669c89644cc3d72a81212fe3707347077776d69a35351ec80f4c55f6be08b4`.

Resultado relevante para o projeto: `cp --reflink`-equivalente (`FICLONE`) e `FIDEDUPERANGE`
funcionam através da camada sobre btrfs e XFS sem nenhuma linha de código de dedup própria,
e o `FIEMAP` passado adiante mostra os extents compartilhados nos dois lados. É a
confirmação prática do item 4 da ADR-0007.

## Atribuição dos commits
  
  Decisão: todos os commits do projeto usam a identidade `Claudio <claudio@studiomoob.com>`;
  este repositório foi configurado para isso e a história anterior foi reescrita em 2026-10-10
  para aplicar a mesma identidade aos commits antigos (ver registro 0018).
  
  ## Limitações

Kernel sem KASAN nem `PROVE_LOCKING`; o aninhamento `lock_rename` da camada sobre o do
inferior e os locks de inode em `setattr`/`write` não foram validados por lockdep. xfstests
completo não executado (exige construir o pacote inteiro; fica para um harness separado).
`dm-log-writes` ainda não aplicado. Sem git nem SQLite estáticos na VM, a aceitação com
cliente real continua na fase 2e. `mknod` e abertura de dispositivos continuam fora.
Permissões: operações de nome usam as credenciais do chamador com `nop_mnt_idmap`, I/O e
reflink usam as do montador; revisão de segurança pendente antes de uso fora de VM.

## Próximo passo

Fase 2c: codificação tipada de `user.moob.*` com validação em `setxattr`, `moobfs-check` e
`moobfs-attr` em espaço de usuário, fuzzing da codificação. Em paralelo, investigar um
harness separado para kernel com KASAN/lockdep (virtme-ng em runner x86).
