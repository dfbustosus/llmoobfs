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
  `sqlite3_stmt_scanstatus_v2` só se a build do SQLite tiver a opção. Pendente de levar para
  `docs/benchmark-harness.md`.

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
2. Versão em lotes com retomada: execução local em andamento no momento deste registro;
   resultado a anexar.

## Pendências para fechar a fase

Resultado do harness Debian; KASAN sobre a branch; ACLs POSIX no teste (pacote `acl` na
imagem); teste de `chmod`/`rename`/`unlink` concorrentes com consultas; anotar em
`docs/spec-m3.md` o contrato TOCTOU e a semântica de erros; atualizar
`docs/benchmark-harness.md` com as formas fixas de W3 e os índices; registro final.
