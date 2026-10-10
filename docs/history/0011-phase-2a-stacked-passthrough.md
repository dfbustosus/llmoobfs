# 0011 — Fase 2a: camada empilhada mínima sobre ext4, validada em VM

Data: 2026-10-10, America/Sao_Paulo. Estado: implementado e validado na VM de teste.

## Pedido

Iniciar a fase 2a da especificação do M2 (ADR-0007): passagem mínima estilo wrapfs sobre um
filesystem maduro, com `fsx`. A linguagem do módulo foi decidida nesta sessão (abaixo).

## Implementação

- `layer/main.c` (módulo `moobfs_layer.ko`, tipo de mount `moobfs_layer`, opção `lower=`):
  um inode da camada por inode inferior (`iget5_locked` chaveado pelo inode inferior, como o
  ecryptfs), caminho inferior em `d_fsdata`, I/O delegado por `fs/backing-file.c`
  (`backing_file_open/read_iter/write_iter/splice/mmap`, como o overlayfs), credenciais do
  montador guardadas no superbloco e usadas no I/O delegado. Operações: lookup, readdir, create,
  mkdir, unlink, rmdir, getattr, setattr (truncate), permission, read, write, mmap compartilhado e
  privado, fsync, llseek, fallocate, splice, statfs. Sem link, symlink e rename (`EPERM` pelo
  VFS), sem xattr, sem índice: conforme o escopo da fase.
- Montagem recusa: ausência de `lower=`, diretório inexistente, empilhar sobre outro
  `moobfs_layer`, profundidade acima de `FILESYSTEM_MAX_STACK_DEPTH`.
- `tests/layer.c`: aceitação de passagem, verificando no diretório inferior inodes, modos,
  tamanhos e bytes após cada operação.
- `tests/third_party/fsx/`: `fsx.c` e `statx.h` do xfstests (commit `57d71a8`, 2026-03-17),
  com `global.h` próprio para compilar sem o build do xfstests. Licença GPL-2.0.
- Harness: `make layer`; `vm/in-container.sh` cria imagem ext4 de 64 MiB com `mke2fs`, resolve
  a cadeia de módulos do ext4 em `modules.dep` e descompacta cada um para a VM; `vm/init` executa
  o M1 e depois a fase 2a; `vm/Dockerfile` ganha `e2fsprogs`; timeout do QEMU 180 s → 600 s.

## Evidências executadas

`make vm-test` (Docker, QEMU TCG em host arm64), kernel Debian `6.12.111+deb13-amd64`, GCC
14.2.0, QEMU 10.0.13. `build/vm/console.log`:

```text
MOOBFS_VM_PASS: shared extents, independent inodes, reads, readonly, remount, 15 malformed images
LAYER_PASSTHROUGH_TESTS_PASS
All 10000 operations completed A-OK!
All 10000 operations completed A-OK!
All 3000 operations completed A-OK!
MOOBFS_LAYER_PASS: pass-through over ext4, layer tests, fsx 23000 ops, remount, unload
```

Três execuções de `fsx` (sementes 1, 2 com I/O de 512 bytes, 3 com arquivo de 2 MB), arquivo
final comparado byte a byte com a cópia inferior, desmontagem, remontagem e descarga do módulo
sem `BUG`, `Oops` ou `WARNING` no `dmesg`. SHA-256 de `moobfs_layer.ko`:
`1de9a67d9aa45ee0dced8b9c54a846103ff70c99cf746193977173eeb515f3e4`. A regressão do M1 passou
inalterada na mesma execução.

## Tentativas e correções

1. Os cabeçalhos do kernel Debian trazem a API de `backing_file_open(const struct file *, …)`
   mais nova que a tag upstream v6.12 (`const struct path *`). O módulo compila só contra os
   cabeçalhos fixados; a documentação em `kernel-infrastructure.md` cita o upstream e a diferença
   fica registrada aqui.
2. O ext4 pede o shash `crc32c` pela API de cripto, dependência que `modules.dep` não registra;
   `crc32c_generic.ko` foi adicionado à cadeia.
3. `vfs_getattr` dispara `WARN_ON_ONCE` quando recebe flags internas de consulta; a variante
   correta para FS empilhado é `vfs_getattr_nosec`, como no ecryptfs. Corrigido; o aviso
   aparecia no `dmesg` e fazia o teste falhar com `EPERM`.
4. Bug no próprio teste: função de caminho com buffer estático único fazia `link(a, a)`.
   Corrigido com buffers rotativos.

## Sobre Rust

Pergunta registrada: por que C e não Rust. Resposta: no 6.12 não há abstrações estáveis de VFS
em Rust (o conjunto de patches de filesystems em Rust foi abandonado em 2024 e nunca entrou na
árvore); um FS empilhado depende de dezenas de helpers do VFS cujos bindings teriam de ser
escritos à mão; e o kernel Debian alvo não é construído com `CONFIG_RUST`. Rust fica como
alternativa a avaliar para as ferramentas de espaço de usuário da fase 2c.

## Limitações

Kernel sem KASAN e sem `PROVE_LOCKING`; o aninhamento de locks de inode da camada sobre os do
inferior não foi validado por lockdep. Sem xfstests completo, sem btrfs/XFS como inferior, sem
dm-log-writes nesta fase. `rename` ausente impede git e SQLite por enquanto (fase 2b).
Credenciais: as operações de diretório usam as do chamador com `nop_mnt_idmap`; só o I/O
delegado usa as do montador. Isso precisa de revisão de segurança antes de qualquer uso fora
de VM.

## Próximo passo

Fase 2b: rename, unlink de arquivos abertos já coberto, xattr opacos, btrfs e XFS como
inferiores, reflink delegado, e kernel com KASAN/lockdep via virtme-ng.
