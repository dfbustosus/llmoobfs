# 0020 — Fase 3b: autorização POSIX no `QUERY` (em andamento)

Data: 2026-10-10, America/Sao_Paulo. Estado: **em andamento**. Código na branch
`m3/3b-query-authz`; este registro existe para que a próxima sessão retome sem perda.

## Decisões (segunda rodada da revisão externa, aceitas)

- Autorização por candidato com os mecanismos do VFS, sem cache próprio: resolver o caminho
  registrado com `vfs_path_lookup` sob as credenciais do chamador, exigir que o objeto
  pertença a esta montagem (`p.mnt` e `d_sb`) e tenha o inode indexado, aplicar
  `inode_permission(MAY_READ)`, emitir só o autorizado; `count`, `limit` e `truncated` contam
  só resultados visíveis.
- O limite de resultados não limita candidatos examinados: varredura em lotes de 256 com
  retomada pela chave ordenada `(tipo, valor, inode)`, lock de leitura liberado durante as
  resoluções; memória limitada por lote.
- Falhas operacionais (`ENOMEM`, `EFAULT`) retornam erro; só falhas de autorização omitem.
- Contrato TOCTOU na UAPI v1: a autorização vale no instante da consulta; o cliente faz
  `open()` normalmente e aceita `EACCES` ou `ENOENT`; sem handles especiais.
- Caminhos obsoletos após renomear diretório: tentar alias de dentry, confirmar identidade,
  autorizar pelo VFS, omitir se não houver caminho verificável; sem atualização recursiva
  antes de medir; falso negativo documentado como limitação.
- Descartados por ora: cache de `MAY_EXEC` por diretório, pré-computação de diretórios
  atravessáveis por todos, verificação só de `MAY_READ`, autorização só por alias.
- Baselines SQLite: banco no mesmo filesystem dos arquivos; W3 passa a ter formas de consulta
  fixas (não combinações arbitrárias), cada uma coberta por um índice composto com igualdade
  antes de intervalo; índices simples redundantes com prefixos de compostos são removidos;
  prefixo de string como intervalo binário com `COLLATE BINARY`; existência como
  `IS NOT NULL`; `-0.0`/`0.0` evitados pelo gerador; gate de `EXPLAIN QUERY PLAN` após
  `ANALYZE` registrando índice e predicados aproveitados, com exceções de `SCAN` documentadas;
  `sqlite3_stmt_scanstatus_v2` só se a build do SQLite tiver a opção. Levado para
  `docs/benchmark-harness.md` §4.1; contrato TOCTOU e erros em `docs/spec-m3.md` §3.1.

## Implementado na branch

- `layer/index.c`: `do_query` reescrito (`caller_may_read`, `first_after`, lotes de 256,
  retomada por chave, `ENOMEM` propagado); ioctl recebe o `file` do chamador.
- `tests/query.c` e `tools/moobfs-index.c`: opção `--uid N` para rodar como outro usuário.
- `vm/suite.sh`: teste de isolamento com dois uids e dois namespaces, namespace compartilhado,
  reatribuição de atributo não concede acesso, `chmod` concede imediatamente, remontagem, e
  600 candidatos privados com 6 autorizados (mais de um lote); raiz do mount em 0755 porque
  o ioctl exige descritor aberto da raiz.

## Rodadas

1. Primeira versão (teto de 4×limite de candidatos): falhou em `iso-query-1001-ext4` porque
   a raiz estava em 0711 e o cliente sem privilégio não abria o diretório. Corrigido.
2. Versão em lotes com retomada: falhou no mesmo ponto; causa no harness, a raiz do initramfs
   nasce 0700 (herdada de `mktemp -d`) e usuários sem privilégio não atravessam `/`.
   Corrigido em `vm/init` (`chmod 0755 /`, `/tmp` 1777). Nova execução a anexar.

## Pendências para fechar a fase

Resultado do harness Debian; KASAN sobre a branch; ACLs POSIX no teste (pacote `acl` na
imagem); teste de `chmod`/`rename`/`unlink` concorrentes com consultas; anotar em
`docs/spec-m3.md` o contrato TOCTOU e a semântica de erros; atualizar
`docs/benchmark-harness.md` com as formas fixas de W3 e os índices; registro final.
(Atualização 2026-10-10: tudo feito exceto o KASAN sobre a branch, em execução; ver as seções
de evidência abaixo.)

## Terceira rodada da revisão externa: decisões aceitas e evidência de código

**Identidade (fase 3a).** Chave lógica `(namespace, memory_id)`; `agent` é autoria, não parte
da chave. O mesmo `memory_id` em namespaces diferentes são duas memórias; conteúdo igual não é
identidade igual; reflink é compartilhamento físico, não lógico; hard link é o mesmo inode e os
mesmos xattr, logo não serve para duas identidades. Proveniência por convenção do cliente:
`user.moob.origin_namespace` e `user.moob.origin_id` (strings). O kernel não gera ids nem
interpreta proveniência. `moobfs-check`: normal (mesmo id em namespaces distintos), aviso
(mesmo id, conteúdo idêntico, sem proveniência), erro (dois inodes com o mesmo
`(namespace, memory_id)`); hard links do mesmo inode não são duplicidade; comparação de
conteúdo opcional. Nenhuma mudança no kernel. Pendente: formalizar em `docs/spec-m3.md` §2 e
implementar no verificador com os cenários listados pela revisão (cópia, reflink, hard link,
proveniência ausente ou inválida, rename, remontagem).

**Autorização (fase 3b), evidência atual.** `moobl_permission` (`layer/main.c:699`) delega a
`inode_permission(&nop_mnt_idmap, inode_inferior, mask)`: isso executa a verificação do
inferior inteira, inclusive ACLs POSIX via `generic_permission` do inferior e
`security_inode_permission` sobre a inode inferior; o VFS já chamou
`security_inode_permission` sobre a inode superior antes de chegar à camada. Não há segunda
verificação DAC sobre metadados da inode superior. `caller_may_read` (`layer/index.c`) usa
`vfs_path_lookup` com o `file->f_path.mnt` do chamador e `inode_permission(MAY_READ)` sobre a
inode superior, o que passa pela mesma `moobl_permission`; portanto a decisão do `QUERY` é a
mesma de um `open()` pela camada. Divergência conhecida a documentar: ambas usam
`nop_mnt_idmap`, correto apenas porque a camada não admite idmapped mounts.

**Idmapped mounts e user namespaces.** `moobl_fs_type` não declara `FS_ALLOW_IDMAP` nem
`FS_USERNS_MOUNT` (`layer/main.c`): no 6.12, `mount_setattr` com idmapping sobre um
filesystem sem `FS_ALLOW_IDMAP` é recusado pelo VFS com `EINVAL`, e montar sem
`FS_USERNS_MOUNT` exige `CAP_SYS_ADMIN` no user namespace inicial, com `EPERM` caso contrário.
Decisão: não acrescentar verificação redundante no módulo; documentar como contrato e provar
na VM (`mount_setattr` deve falhar com `EINVAL`; `unshare -Ur` + mount deve falhar com
`EPERM`). Pendente: pacote `acl` na imagem, testes de ACL (bits negam/ACL concede, bits
concedem/ACL nega, travessia por ACL, ACL alterada após índice e durante consultas), e os
testes de idmap/userns em `vm/suite.sh`.

Execução local da fase 3b com a raiz do initramfs em 0755 (`make vm-test`, kernel Debian
6.12.111): `ISOLATION_PASS` em ext4, btrfs e XFS, inclusive o cenário de 600 candidatos
privados com 6 autorizados; regressão inteira passou (`MOOBFS_ACCEPTANCE_PASS`,
`MOOBFS_LAYER_PASS`), sem avisos de kernel. Pendente para fechar a fase: ACLs, idmap/userns na
suíte, KASAN sobre a branch, e merge em `main` por pull request.


## Achado da terceira rodada: ACLs POSIX não atravessavam a camada

O teste de ACL pedido pela revisão falhou de imediato: `setfacl` pela camada devolvia
"Operation not supported". Causa: no 6.12 o VFS trata `system.posix_acl_access/default` pelas
operações `get_acl`/`set_acl` da inode (e pela flag `SB_POSIXACL` do superbloco), não pelos
handlers de xattr; a camada só tinha o handler genérico. A afirmação anterior de que "ACLs
chegam ao inferior pela passagem de xattr" estava errada e a revisão acertou em exigir prova.
Correção: `moobl_get_acl`/`moobl_set_acl` delegando a `vfs_get_acl`/`vfs_set_acl`/
`vfs_remove_acl` no dentry inferior, como o ecryptfs, e `SB_POSIXACL` herdada do inferior. A
decisão de autorização continua delegada ao inferior; só a escrita e a leitura de ACLs pela
camada passaram a funcionar.

## Evidência final da fase na VM (2026-10-10)

`make vm-test` sobre a branch `m3/3b-query-authz` (commit `fb8f9a1`, kernel Debian 6.12.111),
depois do handshake no teste de user namespace: `ISOLATION_PASS` em ext4, btrfs e XFS, cada um
precedido de `IDMAP_REFUSED_EINVAL` e `USERNS_MOUNT_REFUSED_EPERM`; os testes de ACL (bits
negam e ACL concede, bits concedem e ACL nega, travessia por ACL, ACL alterada depois do índice)
e de identidade (`moobfs-check` com duplicata em inode distinto, hard link aceito, mesmo id em
outro namespace aceito) passaram dentro do mesmo bloco; `MOOBFS_ACCEPTANCE_PASS` e
`MOOBFS_LAYER_PASS` sem avisos de kernel. Ficam pendentes KASAN sobre a branch e o merge em
`main` por pull request.

## Concorrência entre mudanças de permissão, nomes e inodes e o `QUERY` (2026-10-10)

Teste novo em `tests/query.c` (`query --uid N <mount> race <segundos>`) e bloco correspondente
em `vm/suite.sh`: durante 5 s, root alterna `chmod 0600/0644`, renomeia e recria cada um de
40 arquivos e, a cada sete rodadas, renomeia o diretório inteiro, enquanto o uid 1001 repete o
`QUERY`. Invariantes verificados a cada resposta: resultado bem formado (caminho terminado em
zero, relativo, sem `..`), contagem dentro do limite, e cada caminho abre com as credenciais do
chamador ou falha só com `ENOENT` (removido ou renomeado depois da resposta) ou `EACCES`
(`chmod` depois da resposta), que é exatamente a janela TOCTOU aceita no contrato de
`docs/spec-m3.md` §3.1; caminhos marcados `MOOB_RES_PATH_UNKNOWN` (diretório renomeado) são
ignorados. Ao fim, `chmod 0644` em tudo e o `QUERY` deve ser igual ao oráculo.

Resultado (`make vm-test`, kernel Debian 6.12.111, `build/vm-test-race.log`):

| Inferior | consultas em 5 s | resultados abertos | `ENOENT` depois da resposta | `EACCES` depois da resposta |
| --- | --- | --- | --- | --- |
| ext4 | 626 | 27624 | 678 | 35 |
| btrfs | 616 | 27513 | 428 | 17 |
| xfs | 618 | 27447 | 493 | 26 |

Nenhum resultado malformado, nenhum erro diferente dos dois previstos, resposta final igual ao
oráculo nos três, sem avisos de kernel; `MOOBFS_LAYER_PASS` e `MOOBFS_ACCEPTANCE_PASS`. O
teste é de robustez e de contrato, não de desempenho: a VM roda em emulação (TCG).

## KASAN sobre a branch (2026-10-10)

Execução 38076335715: falhou em `acl-set-ext4` por ambiente (workflow sem o pacote `acl`, sem
`setfacl` no guest virtme-ng e sem o binário `idmap`); corrigido em `.github/workflows/kasan.yml`
e `vm/kasan-guest.sh`. Execução 38077926732: ACLs passaram; falhou em `contract-idmap-ext4`
porque o kernel KASAN não habilitava `CONFIG_USER_NS`, logo `unshare(CLONE_NEWUSER)` falhava no
teste. Correção: `CONFIG_NAMESPACES`, `CONFIG_USER_NS` e `CONFIG_FS_POSIX_ACL` em
`vm/kasan.config`. Execução 38079499011 (kernel 6.12.112 com KASAN, UBSAN e lockdep):
`MOOBFS_KASAN_PASS`, `ISOLATION_PASS` em ext4, btrfs e xfs com `IDMAP_REFUSED_EINVAL` e
`USERNS_MOUNT_REFUSED_EPERM`, teste de concorrência com 41 a 75 consultas em 5 s (kernel
instrumentado é mais lento), sem nenhum diagnóstico de kernel. A fase 3b está encerrada no
que depende de código e teste; resta o merge em `main` por pull request.
