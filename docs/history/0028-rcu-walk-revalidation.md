# 0028 — Perfil do `QUERY` e revalidação de dentry compatível com RCU-walk

Data: 2026-10-11. Branch: `m3/rcu-revalidate`. Estado: implementado e validado na VM; medição
no host da Azure pendente.

## Pergunta

Depois do registro 0027, o custo restante do `QUERY` era de cerca de 2 µs por resultado. A
pergunta foi se o Linux já oferece um mecanismo para reduzir esse custo sem enfraquecer a
autorização, em vez de inventarmos um cache próprio.

## Medição

No host da Azure (kernel de benchmark com `function_graph`, kprobes, `perf` e `bpftrace`),
amostragem com `perf` de 3000 execuções da forma Q7 (513 resultados), evidências em
`bench/results/2026-10-11-azure-profile-q7/`. Divisão do tempo dentro de `do_query`:
resolução do caminho com `vfs_path_lookup` 56%; a própria `do_query` (varredura, filtros,
montagem do caminho) 12%; cópia para o usuário 7%; `getname_kernel` (cópia do caminho como
texto, com limpeza de 4 KB) 5%; verificação final de permissão, `kstrdup`, `kfree` e
`path_put`, o restante.

Dentro da resolução de caminho aparecem `__d_lookup`, `try_to_unlazy_next`, `dput` e
`step_into`, que são do modo de referência (ref-walk) do VFS, e quase nada de
`__d_lookup_rcu` (0,4% contra 2,9%). A causa está em `moobl_d_revalidate`, que devolvia
`-ECHILD` sempre que chamada com `LOOKUP_RCU`. Com isso, toda resolução de caminho que passa
pela camada abandona o modo sem lock (RCU-walk) logo no primeiro componente. O mesmo vale para
qualquer `open` pela camada, o que explica parte do acréscimo medido em W2.

## Alternativas consideradas

- Cache de autorização por diretório, dentro de uma consulta: reduziria o número de caminhadas,
  mas é mecanismo próprio e mexe no contrato de autorização da fase 3b. Descartado enquanto o
  mecanismo do próprio VFS não estiver em uso.
- `open_by_handle_at`: exige `CAP_DAC_READ_SEARCH` e não verifica os diretórios do caminho.
  Não serve para autorizar um chamador comum.
- Landlock e AppArmor: são políticas de acesso, não aceleram a verificação.

## Decisão

Usar o RCU-walk do próprio VFS. `moobl_d_revalidate_rcu` responde "válido" sem locks, sem
referências e sem dormir, só no caso simples: o inferior não tem revalidação própria (ext4,
btrfs e XFS locais não têm), o dentry inferior continua no hash, e o dentry superior concorda
com ele (positivo e mapeado para o mesmo inode inferior com link, ou ambos negativos). Em
qualquer outro caso devolve `-ECHILD` e o VFS refaz em ref-walk, que roda a verificação
completa de antes. A troca do dentry inferior em `moobl_instantiate` (correção do registro
0015) passa a publicar o ponteiro com `WRITE_ONCE`; o leitor usa `READ_ONCE`. A segurança vem
de `moobl_dentry` ser liberado com `call_rcu` e de os dentries inferiores só serem liberados
depois de um período de graça. Os atributos não são copiados no modo RCU; `getattr` e as
operações de nome já os atualizam. O contrato de autorização não muda: as mesmas permissões são
verificadas, sem locks.

## Lacuna de segurança registrada (fora do escopo desta mudança)

A verificação por resultado do `QUERY` usa `vfs_path_lookup` e `inode_permission`. Isso cobre
DAC, ACLs e módulos de segurança baseados em inode, mas não os que decidem pelo caminho na
abertura (AppArmor, Landlock). Um processo confinado por eles poderia ver nos resultados um
arquivo que não conseguiria abrir. Vai para o roteiro de segurança.

## Testes

`make vm-test` (Debian 6.12.111): `ISOLATION_PASS` e `BENCH_LAYER_PASS` nos três inferiores,
`MOOBFS_ACCEPTANCE_PASS` (inclusive o caso SQLite WAL com remoção e recriação, que é o cenário
da troca de dentry) e `MOOBFS_LAYER_PASS`; teste de concorrência com cerca de 600 consultas por
inferior, todas bem formadas. KASAN e lockdep: pelo workflow do pull request.

## Próximo passo

Medir no host da Azure com esta mudança somada às dos registros 0025 e 0027 e comparar com o
piloto 3 e o perfil.
