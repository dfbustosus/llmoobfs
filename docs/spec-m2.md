# Especificação do M2 — volume gravável POSIX com extents compartilháveis

Estado: **proposta** (2026-10-10). Nada aqui está implementado ou testado. Decisões em
[ADR-0005](decisions/0005-memory-model-bfs-reference.md) e
[ADR-0006](decisions/0006-m2-posix-writes.md). Infraestrutura reutilizada em
[kernel-infrastructure.md](kernel-infrastructure.md). O formato M1 continua descrito em
[format-m1.md](format-m1.md) e não é alterado.

## 1. Escopo

Entra no M2: filesystem gravável, hierárquico, com journaling de metadados, extents
compartilháveis entre arquivos com copy-on-write, atributos tipados via xattr, índice de
atributos em RAM e consulta por ioctl, ferramentas `mkfs`/`fsck` v2, importação de imagens M1.

Fica fora: snapshots e revisões por memória (M3), dedup automático na escrita, hard links,
symlinks, quotas, ACLs POSIX, criptografia, fs-verity, exportação NFS, user namespaces,
índice de atributos persistente, direct I/O (retorna `EINVAL` no M2).

## 2. Limites

| Item | Limite | Motivo |
| --- | --- | --- |
| Tamanho de bloco | 4.096 bytes | Igual ao M1 e ao tamanho de página x86-64 |
| Volume | 64 MiB a 16 GiB | Números de bloco em 32 bits; índice em RAM limitado |
| Inodes | até 65.536 | Índice de atributos em RAM reconstruído na montagem |
| Tamanho de arquivo | até 4 GiB | Extents com offsets de 32 bits em blocos |
| Nome de entrada | 1 a 255 bytes, qualquer byte exceto `/` e NUL | POSIX; o ai-memory usa UTF-8 e UUIDs |
| Profundidade de diretórios | sem limite próprio | Limite do VFS (`PATH_MAX`) |
| xattr por inode | até 4 KiB no total, até 64 atributos | Um bloco de xattr por inode, como ext2 |
| Journal | 8 MiB a 128 MiB, fixado no `mkfs` | jbd2 exige área contígua |

## 3. Layout em disco (v2)

```text
bloco 0            Superbloco A
bloco 1            Superbloco B (cópia)
bloco 2..          Bitmap de blocos
...                Bitmap de inodes
...                Tabela de inodes (256 bytes por inode)
...                Tabela de extents compartilháveis (objetos)
...                Journal jbd2
...                Blocos de dados, diretórios e xattr
```

Todos os inteiros little-endian, serialização explícita, reservado zerado, como no M1.
CRC32C em superbloco, cada bloco de bitmap, cada inode, cada registro de objeto e cada bloco
de diretório e de xattr, com o campo de checksum zerado durante o cálculo. O journal tem os
checksums do próprio jbd2 (feature `JBD2_FEATURE_INCOMPAT_CSUM_V3`).

### 3.1 Superbloco (4.096 bytes)

Magic `MOOBFSW2`, versão 2, UUID do volume, tamanho de bloco, contagem de blocos, contagem de
inodes, offsets de cada região, blocos do journal, flags de recursos (compat, ro_compat,
incompat, como ext4), estado (`clean`/`dirty`/`error`), contador de montagens, timestamps de
última montagem e última escrita, CRC32C. Flag incompat desconhecida impede montar; flag
ro_compat desconhecida monta só leitura.

### 3.2 Inode (256 bytes)

Modo, uid, gid, tamanho lógico, links (sempre 1 para arquivos; diretórios contam `.` e
subdiretórios), timestamps em nanossegundos (atime, mtime, ctime, crtime), geração, flags,
bloco de xattr (0 se inline vazio), **até 8 extents inline** `(offset lógico em blocos,
objeto ou bloco físico, comprimento em blocos, flags)`, ponteiro para bloco de extents
adicional, área inline de xattr de 96 bytes para atributos pequenos, CRC32C.

Um extent aponta ou para blocos físicos exclusivos do inode (flag `OWN`) ou para um **objeto**
compartilhável (flag `SHARED`). Escrita sobre extent `SHARED` faz copy-on-write: aloca blocos
novos, copia o que não foi sobrescrito, troca o extent para `OWN` e decrementa o objeto.

### 3.3 Objeto (registro de 64 bytes na tabela de objetos)

ID, bloco físico inicial, comprimento em blocos, tamanho lógico em bytes, contagem de
referências, SHA-256 do conteúdo lógico (válido só com flag `SEALED`), flags, CRC32C.
Um objeto com `refcount == 0` é liberado na mesma transação. `SEALED` é definido quando o
objeto nasce por `remap_file_range` ou importação; um objeto é imutável por construção, pois
toda escrita em extent compartilhado faz copy-on-write.

### 3.4 Diretórios

Blocos lineares ao estilo ext2: entradas `(inode, tamanho do registro, tamanho do nome, tipo,
nome)`, com `.` e `..`, e um rodapé com CRC32C por bloco. Busca linear é aceitável até o
limite de inodes do M2; htree ou B+ tree ficam para depois, com medição.

### 3.5 Atributos tipados (xattr)

Namespace `user.moob.<nome>`. Valor serializado como `tipo (1 byte) + payload`:
`string` UTF-8, `int64`, `double`, `bool`, `time` (ns desde epoch, int64), `raw`.
`setxattr` com tipo inválido ou tamanho fora do limite retorna `EINVAL`. Outros namespaces
(`user.*` sem prefixo `moob.`, `trusted.`, `security.`) são aceitos como bytes opacos, para
compatibilidade com ferramentas comuns, mas não indexados. Blocos de xattr idênticos entre
inodes são compartilhados via `mbcache` com refcount, como ext2/ext4.

### 3.6 Journal

Área contígua gerida por jbd2. Transações cobrem bitmaps, inodes, tabela de objetos,
diretórios e blocos de xattr. Dados de arquivo não entram no journal; são forçados ao disco
antes do commit da transação que publica seus metadados (ordenação `data=ordered`). O
superbloco é atualizado fora do journal com escrita alternada A/B.

## 4. Operações

| Operação | Comportamento no M2 |
| --- | --- |
| `create`, `mkdir`, `unlink`, `rmdir`, `rename` | Transações jbd2; `rename` atômico inclusive sobre destino existente; `RENAME_EXCHANGE` e `RENAME_NOREPLACE` suportados |
| `link`, `symlink` | `EPERM` |
| `read`, `write`, `truncate`, `fallocate` (só modo padrão) | iomap; escrita em extent `SHARED` faz copy-on-write |
| `mmap` | Privado e compartilhado, leitura e escrita (`iomap_page_mkwrite`) |
| `fsync`, `fdatasync` | Força dados do inode e faz `jbd2_journal_force_commit` da transação pendente; retorno de sucesso implica durabilidade dos metadados e dados daquele arquivo |
| `syncfs`, `sync`, freeze | Via `sync_fs` e `freeze_fs` do VFS |
| `fcntl` locks, `flock` | Genéricos do VFS |
| `setxattr`, `getxattr`, `listxattr`, `removexattr` | Handlers por prefixo; `user.moob.*` validado e indexado |
| `FICLONE`, `FICLONERANGE`, `FIDEDUPERANGE` | `.remap_file_range`: cria ou reaproveita objeto `SEALED`, aponta extents `SHARED` dos dois arquivos; para `DEDUP` o VFS já comparou os bytes |
| `FIEMAP` | Como no M1, com flag `SHARED` em extents de objeto |
| `QUERY` (ioctl em `.control`) | Predicado: até 4 termos `(atributo, operador ∈ {=, <, <=, >, >=, prefixo}, valor tipado)` em conjunção; retorna até N inodes com números e caminhos canônicos; leitura do índice em RAM |
| `GET_INFO` (ioctl) | UUID, versão, limites, estado do journal, contadores |
| `O_DIRECT` | `EINVAL` |

Montagem `ro`: nenhuma transação é aberta; journal é apenas reproduzido se `dirty`
(`norecovery` monta sem reproduzir e recusa escrita). Remount `rw → ro` espera transações.

## 5. Índice de atributos em RAM

Na montagem, após o replay do jbd2, o módulo percorre a tabela de inodes e carrega os
atributos `user.moob.*` em estruturas em memória (`xarray` por atributo, chaveada pelo valor
codificado para ordenação; `rbtree` para strings). Custo de montagem proporcional ao número de
inodes, limitado pelo teto de 65.536. Cada `setxattr`/`removexattr`/`unlink` atualiza o índice
dentro do mesmo caminho de código que grava a transação, sob o lock do inode; a consistência
do índice com o disco é garantida por reconstrução na próxima montagem, não por persistência.
Memória estimada: até 64 atributos × 65.536 inodes × ~64 bytes ≈ 256 MiB no pior caso;
o mount rejeita volumes cujo índice exceda o orçamento configurável (padrão 128 MiB).

## 6. Durabilidade e recuperação

- `fsync` bem-sucedido garante dados e metadados do arquivo e das entradas de diretório que
  o referenciam (como ext4 `data=ordered` com `auto_da_alloc` desligado; não prometemos mais).
- Após queda, `mount` reproduz o journal via jbd2; metadados ficam no último commit. Dados
  gravados sem `fsync` podem ser perdidos; nunca aparecem blocos de outro arquivo (ordenação).
- Erro de I/O em metadados aborta o journal (`jbd2_journal_abort`), o volume vira só leitura e
  marca `error` no superbloco. Erro em dados retorna ao chamador e marca `AS_EIO` no mapping.
- `fsck.moobfs` v2: verifica superbloco, bitmaps contra uso real, inodes, extents, objetos e
  refcounts, diretórios, xattr e checksums; reproduz ou descarta journal apenas com opção
  explícita; **não repara** no M2, só diagnostica (como o M1).

## 7. Ferramentas

- `mkfs.moobfs` v2: cria volume vazio ou `--import-m1 imagem.img` (cada arquivo M1 vira um
  arquivo na raiz, objetos M1 viram objetos `SEALED` compartilhados). Recusa sobrescrever
  dispositivos ou arquivos que já contenham magic conhecido, salvo `--force`.
- `fsck.moobfs` v2: saída JSON, código de saída distinto por classe de problema.
- `moobfs-dedup` (espaço de usuário): varre o mount, agrupa por SHA-256 e chama
  `FIDEDUPERANGE`; não tem acesso ao formato, só usa ioctls padrão. Pode ser substituído por
  `duperemove`, o que serve como teste de conformidade.

## 8. Entradas inválidas que o parser e o módulo devem rejeitar

Superbloco com magic/versão/checksum errados ou tamanhos inconsistentes; flags incompat
desconhecidas; bitmaps com bits fora do volume; inode com modo inválido, extents sobrepostos,
extents fora do volume, referência a objeto inexistente ou com `refcount` 0; objeto com
`refcount` maior que referências reais (diagnóstico do fsck); diretório com entrada apontando
para inode livre, nome com `/` ou NUL, tamanho de registro inválido, `.`/`..` ausentes ou
errados, ciclo; xattr com tipo inválido, tamanho fora do limite, nome duplicado; journal com
checksum inválido ou sequência fora de ordem (tratado pelo jbd2, mas coberto em teste); volume
truncado. Cada caso tem imagem gerada por `tests/test_images.py` e é testado nas ferramentas e
na montagem, como os 15 casos do M1.

## 9. Testes e evidência exigida

1. `make test`: parser compartilhado, ferramentas, sanitizers, fuzzing de `common/` (corpus
   versionado, execução limitada em CI).
2. `make vm-test`: além do M1, cria volume v2 e executa, em sequência, com evidência gravada
   em `build/vm/`:
   - xfstests grupo `generic` com exclusões documentadas por item fora de escopo;
   - `fsx` e `fsstress` por tempo fixo;
   - `dm-log-writes`: carga de `rename`/`fsync`/`FIDEDUPERANGE`, replay em cada marca de FUA,
     `fsck` e comparação de hashes;
   - `dm-flakey`: erro de I/O durante commit, verificar volume só leitura e remontagem limpa;
   - imagens inválidas da seção 8;
   - **aceitação com cliente real**: `git init` + commits, SQLite em WAL com escritor e leitor
     concorrentes, `ai-memory` com `data_dir` no mount executando uma sessão capturada e um
     `reindex`; sucesso é `ai-memory status` consistente e `fsck` limpo após desmontar.
3. Kernel com KASAN e `PROVE_LOCKING` via virtme-ng executando a mesma suíte, pelo menos antes
   de cada marco declarado.
4. Nenhum resultado é declarado sem o comando e o resumo em `docs/history/`.

## 10. Compatibilidade

Formato novo e incompatível com o M1 (magic distinto). Imagens M1 continuam legíveis pelo
módulo e ferramentas M1 do histórico Git e convertíveis por `--import-m1`. Mudanças dentro do
v2 usam flags compat/ro_compat/incompat, nunca reinterpretação silenciosa de campos. A UAPI
dos ioctls `QUERY`/`GET_INFO` tem número de versão próprio; comando desconhecido retorna
`ENOTTY`.

## 11. Fases e critério de saída

| Fase | Entrega | Saída |
| --- | --- | --- |
| 2a | Protótipo iomap + jbd2 em FS mínimo (um diretório plano, sem objetos nem xattr) | Decide item 3 da ADR-0006; `fsx` passa; queda com dm-log-writes recupera |
| 2b | Formato v2 completo, `mkfs`/`fsck`, parser em `common/`, imagens inválidas, fuzzing | `make test` com fuzzing; imagens inválidas rejeitadas |
| 2c | Diretórios, rename, truncate, mmap, fsync; xfstests `generic` com exclusões | Lista de testes passando/excluídos versionada |
| 2d | Objetos, `remap_file_range`, copy-on-write, FIEMAP; `duperemove` funciona | Dedup de corpus real do ai-memory medido |
| 2e | xattr tipados, mbcache, índice em RAM, `QUERY` | Oráculo: mesmas consultas no `bfs_shell` do Haiku |
| 2f | Aceitação com git + SQLite + ai-memory; KASAN/lockdep | Registro em `docs/history/` com comandos e números |

## 12. Questões em aberto

- iomap bufferizado com jbd2 ordenado (fase 2a decide).
- Política de `atime` (proposta: `relatime` padrão, `noatime` recomendado para memórias).
- Se `QUERY` deve devolver caminhos ou só inodes (caminhos exigem lookup reverso; proposta:
  inodes + handle via `open_by_handle_at`, caminhos opcionais e limitados).
- Tamanho do journal padrão em função do volume.
- Se objetos `SEALED` devem ganhar fs-verity opcional ainda no M2 ou só no M3.
