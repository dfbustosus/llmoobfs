# Especificação do M3 — identidade, isolamento e operações de consulta

Estado: **proposta** (2026-10-10, revisada no mesmo dia após revisão externa; ver registro
0019). Nada implementado. Decisões de base: ADR-0005, ADR-0007. Depende do M2 concluído.

## 1. Diagnóstico do que existe (evidência no código)

| Proposta | Já existe? | Evidência | Mudança necessária | Risco |
| --- | --- | --- | --- | --- |
| Atributos `namespace`, `agent`, `memory_id` como strings tipadas | Sim, como qualquer `user.moob.<nome>` de tipo `string` até 1 KiB, nome `[a-z0-9_.]` ≤ 64 bytes | `include/moobfs_attr.h` (`MOOB_ATTR_STRING_MAX`, `MOOB_ATTR_NAME_MAX`), validação em `moobl_typed_check` (`layer/main.c`), índice por nome em `layer/index.c` | Nenhuma no kernel; só convenção documentada | nenhum |
| `QUERY` por esses atributos, em conjunção, com prefixo | Sim | `do_query`, `entry_matches` em `layer/index.c`; fase 2d comparada ao oráculo (registro 0014) | nenhuma | nenhum |
| **`QUERY` respeita permissões do chamador** | **Não** | `fill_result` (`layer/index.c`) devolve inode e caminho de toda entrada do índice; a única verificação é `file->f_path.dentry == sb->s_root` em `moobl_root_ioctl`; o índice é construído com as credenciais do montador (`override_creds` em `moobl_index_build`) | filtrar resultados pelas credenciais do chamador | **vazamento de existência e de caminhos entre usuários; corrigir antes de qualquer uso multiusuário** |
| Identidade estável sob `rename` | Parcial | o índice é por inode; `moobl_index_renamed` só atualiza o caminho | nenhuma para identidade por atributo (o atributo acompanha o inode) | nenhum |
| Revisão lógica, histórico, cursor de mudanças | Não | índice volátil, reconstruído na montagem (`moobl_index_build`); nenhum contador de revisão | ver §5 | — |
| Consulta em lote | Não | um `QUERY` por chamada, até 4.096 resultados (`MOOB_QUERY_MAX_LIMIT`) | ver §4 | — |
| Leitura em lote de conteúdo | Não | resultados trazem caminho; o cliente abre um a um | ver §4 | — |

## 2. Modelo de identidade

Identidade lógica `(namespace, agent, memory_id)` expressa em três atributos tipados comuns,
gerados e garantidos únicos **pelo cliente**:

| Atributo | Tipo | Regra |
| --- | --- | --- |
| `user.moob.namespace` | string, ≤ 128 bytes recomendados | domínio lógico; opaco para o kernel |
| `user.moob.agent` | string, ≤ 128 bytes | identidade do agente que criou ou atualizou; opaco |
| `user.moob.memory_id` | string, ≤ 128 bytes; recomendação UUID v7 em texto | identidade estável; sobrevive a `rename` e a `FICLONE` (cópia carrega o atributo, o cliente decide se gera outro) |

Chave lógica: `(namespace, memory_id)`. `agent` é autoria ou responsabilidade, não parte da
chave, para que outro agente atualize uma memória sem lhe mudar a identidade. O mesmo
`memory_id` em namespaces diferentes são duas memórias distintas, que podem partilhar origem,
conteúdo ou blocos; conteúdo igual não é identidade igual. `FICLONE` e cópia criam outro inode:
quando a operação é uma clonagem de memória, o cliente gera um `memory_id` novo e registra a
origem em `user.moob.origin_namespace` e `user.moob.origin_id` (strings; convenção, não
verificação). Hard link é o mesmo inode e os mesmos atributos, logo não serve para duas
identidades; para isso, cópia ou reflink. `moobfs-check` classifica: normal, o mesmo id em
namespaces distintos; aviso, o mesmo id com conteúdo idêntico e sem proveniência (comparação
de conteúdo opcional); erro, dois inodes distintos com o mesmo `(namespace, memory_id)`;
vários caminhos para o mesmo inode não são duplicidade.

O kernel não valida unicidade nem formato além das regras gerais de tipo e nome, não
interpreta esses atributos como credenciais nem proveniência, e não os altera. Comparação é por bytes.
Nenhum tipo novo e nenhum atributo reservado. A alternativa de identificadores atribuídos
pelo kernel (`id`, `rev`, `mrev` como atributos reservados, gravados a cada escrita) foi
avaliada e **adiada**: custaria um `setxattr` no inferior por mudança e criaria estado que o
cliente não pode reproduzir sozinho; só volta se o M4 mostrar que o cliente não consegue
manter identidade e revisão com custo aceitável.

## 3. Isolamento por POSIX e correção do `QUERY`

Invariantes: alterar `namespace` ou `agent` não concede acesso; `QUERY` não revela memórias
inacessíveis; o índice não contorna permissões; isolamento vale após remontagem; `rename` não
muda identidade; hard links e reflinks não acrescentam privilégio; consultas concorrentes com
mudanças de permissão não vazam.

Layout de referência (não imposto): `/<ns>/shared/` e `/<ns>/agents/<agent>/`, com donos,
grupos e modos POSIX definidos por quem administra o mount. O módulo não provisiona nada.

Mudança no kernel, a única do M3 nesta forma: em `do_query`, para cada entrada candidata,
antes de copiá-la ao usuário, resolver o caminho registrado com as credenciais do **chamador**
por `vfs_path_lookup(sb->s_root, mnt, caminho, 0, &p)`; a resolução falha com `EACCES` se
faltar permissão de busca em qualquer diretório do caminho, e a entrada é omitida sem erro e
sem contar em `truncated`. Se resolver, exigir que o inode obtido seja o da entrada (protege
contra caminho obsoleto) e `inode_permission(MAY_READ)` com as credenciais do chamador. Sem
caminho registrado, tentar o alias de dentry em cache e aplicar as mesmas verificações;
sem alias, omitir. `truncated` passa a contar só resultados visíveis. O lock de leitura do
índice é liberado antes das resoluções para não dormir com ele; a lista de candidatos é
copiada primeiro. Custo: uma resolução de caminho por resultado, limitada por `limit`; medido
no M4 (W3).

Responsabilidades do cliente: criar diretórios, usuários, grupos e ACLs; gerar
identificadores; decidir o que compartilhar por `FICLONE` ou link.

Testes: na VM, dois uids e dois namespaces com modos distintos; consultas cruzadas devem
omitir o namespace alheio e `truncated` não pode revelar contagem; `chmod` durante consultas
concorrentes; remontagem e repetição; o oráculo em espaço de usuário executado como cada
uid deve coincidir com o kernel. Regressão completa (POSIX, git, SQLite, dm-log-writes,
dm-flakey, KASAN/lockdep). Critério de pronto: identidade estável e consultável sem segunda
política de autorização; zero vazamento entre usuários em todos os inferiores.

### 3.1 Contrato de autorização do `QUERY` (UAPI v1, semântica)

- A autorização é verificada no instante da consulta, com as credenciais efetivas do
  chamador, pela resolução do caminho no VFS e por `inode_permission(MAY_READ)` no objeto
  resolvido, que precisa pertencer à montagem consultada e ter o inode indexado.
- Um resultado não concede acesso: o cliente faz `open()` normalmente e deve aceitar `EACCES`
  ou `ENOENT` se permissões ou nomes mudaram depois da consulta. Não há garantia de ausência
  de janela TOCTOU na v1 e não haverá handle especial para fechá-la sem necessidade medida.
- `count`, `limit` e `truncated` referem-se só a resultados visíveis ao chamador. O número de
  candidatos examinados não é limitado pelo `limit`: a varredura percorre todos os candidatos
  em lotes, com retomada por chave ordenada.
- Falhas de autorização omitem o resultado em silêncio. Falhas operacionais (`ENOMEM`,
  `EFAULT`) retornam erro.
- Entradas cujo caminho registrado ficou obsoleto (diretório renomeado) são resolvidas pelo
  alias de dentry quando existe; sem caminho verificável, são omitidas. Isso pode produzir
  falsos negativos até a próxima montagem; é limitação documentada, não vazamento.

## 4. Operações em lote: contrato, decisão condicionada a medição

Duas candidatas, com propósitos diferentes:

- `QUERY_BATCH`: várias consultas independentes numa chamada, cada uma com a semântica exata
  de `QUERY`, resultado e erro por consulta, sem atomicidade nem snapshot entre elas,
  limite explícito (proposta: 64 consultas por chamada, `limit` individual ≤ 4.096,
  memória total ≤ 64 MiB), permissões verificadas por resultado, UAPI v2 aditiva (comando
  novo, estruturas v1 intactas).
- `READ_BATCH`: para uma lista de inodes já obtida, devolver atributos escolhidos e os
  primeiros K bytes de conteúdo com permissões por inode, para substituir N aberturas.

Decisão: **nenhuma das duas entra antes do M4 medir** W3 (consultas) e W2 (leitura por
caminho) contra chamadas individuais. Se o custo de chamada ao kernel não dominar, a
implementação é dispensada e isso fica registrado. Testes obrigatórios se implementadas:
versão desconhecida, contagem zero e acima do limite, ponteiro nulo e inválido, overflow de
tamanho, reservados não zerados, consulta interna inválida, falha parcial, remoção, renomeação
e `setxattr` concorrentes, resultados inacessíveis, compatibilidade com cliente v1, fuzzing
dos parsers.

## 5. Revisão lógica e cursor de mudanças: decisão

Dois conceitos distintos: **revisão lógica persistente** (evolução de uma memória) e **cursor
de enumeração** (progresso de uma consulta, válido no máximo por montagem).

Decisão: a revisão lógica e o histórico ficam **no cliente**, com os mecanismos que já
existem: git para conteúdo, um atributo tipado `user.moob.rev` (int64) mantido pelo cliente
quando quiser consultá-lo pelo `QUERY`, e `modified`/`ctime` do inferior para ordenação
grosseira. Motivos: o índice é volátil e não constitui histórico; alterações feitas fora da
montagem não seriam vistas por um log do kernel; um log persistente no kernel duplicaria o
git; e nenhum cliente pediu essa operação com um benefício medido. A forma de cursor
avaliada (lista circular por montagem, `EOVERFLOW` quando expirado, `CHANGES_SINCE`) fica
documentada como alternativa e só é reconsiderada se o M4 mostrar que a varredura do cliente
após uma sessão custa mais do que o aceitável. Perguntas que qualquer implementação teria de
responder antes: identidade e escopo da revisão, persistência, ordenação, concorrência,
recuperação após falha, validade do cursor, códigos de erro, integração com git.

## 6. Fases

| Fase | Entrega | Saída |
| --- | --- | --- |
| 3a | Convenção de identidade documentada; `moobfs-check` opcionalmente verifica unicidade de `memory_id` por namespace | documento e verificação offline |
| 3b | `QUERY` com permissões do chamador; teste multiusuário; regressão completa com KASAN/lockdep | zero vazamento; números preliminares do custo |
| 3c | Decisões de §4 e §5 revisitadas com os números do M4 | registro em `docs/history/` |

## 7. Riscos e limitações

Resolver caminhos por resultado pode ser caro em consultas largas; mitigação: `limit` e
medição. Caminhos registrados ficam obsoletos após renomear diretórios; o fallback por alias
cobre entradas em cache e as demais são omitidas até a próxima montagem, comportamento que
deve ser documentado e medido. Nada aqui altera a UAPI v1.
