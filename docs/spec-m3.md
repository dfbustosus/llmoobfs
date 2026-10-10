# Especificação do M3 — identidade, isolamento, proveniência e revisões

Estado: **proposta** (2026-10-10). Nada implementado. Decisões de base: ADR-0005 (memória =
arquivo com atributos tipados; semântica fora do kernel), ADR-0007 (camada sobre FS maduro;
git versiona conteúdo; snapshots do inferior) e registro 0019 (isolamento, lote e cursor
adotados a partir de revisão externa). Depende do M2 concluído (fases 2a a 2e validadas).

## 1. Problema

O M2 responde "quais arquivos têm estes atributos". Para servir vários agentes sobre o mesmo
armazenamento faltam quatro coisas que nenhum cliente consegue construir sozinho de forma
consistente por cima de um diretório: identidade estável de cada memória, isolamento entre
agentes e namespaces, proveniência verificável e consulta temporal, e um jeito barato de
saber o que mudou desde a última vez. Tudo isso sem substituir o git, que continua sendo o
histórico de conteúdo, e sem lógica de política no kernel.

## 2. Modelo

Três conceitos, todos expressos com o que já existe (diretórios, permissões POSIX, atributos
tipados), mais um contador por mount:

| Conceito | Representação | Quem define |
| --- | --- | --- |
| Namespace | Diretório de primeiro nível sob o mount, com dono e modo POSIX; atributo `user.moob.ns` (string) no diretório | administrador do mount, por `mkdir` e `chmod`/`chown` |
| Agente | Usuário ou grupo POSIX; atributo `user.moob.agent` (string) em cada memória que o agente cria | o próprio agente, ao gravar |
| Memória | Arquivo; atributos reservados `user.moob.id` (raw, 16 bytes, UUID v7 atribuído pela camada na criação se ausente), `user.moob.rev` (int64, revisão da memória), `user.moob.mrev` (int64, revisão do mount em que foi gravada) | a camada |

Revisão do mount (`mrev`): contador monotônico de 64 bits, por superbloco, incrementado a
cada operação que altera conteúdo ou atributos tipados de uma memória, e persistido no
inferior como `user.moob.mrev` na raiz a cada incremento (uma escrita de xattr; custo medido
no M4). Após remontagem, continua do valor persistido. Não é um relógio: é um número de
sequência que permite perguntar "o que mudou desde a revisão N".

Revisão da memória (`rev`): contador por arquivo, incrementado pela camada a cada escrita que
chega ao inferior através dela (`write`, `truncate`, `setxattr` tipado, `rename`). Não guarda
versões: quem guarda conteúdo anterior é o git do cliente ou um snapshot do inferior.

## 3. Isolamento

Regra única: a camada não inventa controle de acesso. Um agente lê e escreve o que as
permissões POSIX do inferior permitem, verificadas em dupla (camada e inferior), como hoje.
O que o M3 acrescenta é que **o `QUERY` respeita o mesmo isolamento**: um resultado só é
devolvido se o chamador tiver permissão de busca em todos os diretórios do caminho e de
leitura no arquivo, avaliadas no momento da consulta com as credenciais do chamador, não do
montador. Hoje o `QUERY` devolve inodes e caminhos de tudo que o índice contém; isso é uma
falha de isolamento e é corrigida aqui.

Namespaces são, portanto, diretórios com permissões; "agente A não vê o namespace B" é
`chmod 0750` mais grupo. Compartilhar uma memória entre namespaces é `FICLONE` para dentro do
outro namespace (conteúdo físico compartilhado pelo inferior, metadados e permissões próprios),
ou link duro se o mount não usar `nolinks`. Nenhuma dessas operações é nova.

Fora do escopo: ACLs POSIX (passam pela camada se o inferior tiver), user namespaces,
criptografia por namespace, quotas por agente.

## 4. Proveniência

Atributos reservados gravados pela camada, não editáveis pelo cliente (`EPERM` em
`setxattr`, como `user.moob.format` hoje): `id`, `rev`, `mrev`, `created` (time, primeira
gravação através da camada), `modified` (time), `agent_last` (string: nome do usuário POSIX
do último escritor, resolvido pelo uid). O cliente pode gravar os seus (`agent`, `source`,
`session`, `kind`...), que seguem convenção, não verificação.

Verificação de conteúdo continua sendo do inferior (checksums do btrfs) ou do git; a camada
não calcula hashes no M3. fs-verity para memórias seladas fica como extensão posterior.

## 5. Consulta temporal

Sem versões guardadas, "temporal" no M3 significa duas coisas, ambas baratas:

- `QUERY` aceita termos sobre `rev`, `mrev`, `created` e `modified` como qualquer atributo
  `int64`/`time` (já funciona com a UAPI v1; só exige que a camada mantenha esses atributos).
- **Cursor de mudanças**: `MOOB_IOC_CHANGES` recebe `since` (mrev) e um limite; devolve
  `(ino, caminho, mrev, tipo de mudança ∈ {criado, conteúdo, atributos, renomeado,
  removido})` para tudo que mudou depois de `since`, em ordem de `mrev`, com `truncated` e o
  maior `mrev` devolvido. Fonte: uma lista circular em memória por superbloco com as últimas
  N mudanças (N configurável, padrão 65.536) mais o índice para o estado atual. Se `since` for
  anterior ao que a lista ainda guarda, o ioctl devolve `EOVERFLOW` e o cliente faz varredura
  completa. Removidos entram na lista com o caminho que tinham. Após remontagem a lista começa
  vazia e `since` menor que o `mrev` da montagem devolve `EOVERFLOW`.

Isso cobre "o que o agente precisa reler desde a última sessão" sem log em disco e sem
reindexação do cliente, que era o custo que o ai-memory paga com watcher e reconciliação.

## 6. Operações em lote

`MOOB_IOC_READ_BATCH`: recebe até 256 inodes (saídos de `QUERY` ou `CHANGES`) e um buffer;
devolve, por inode, atributos tipados escolhidos (lista de nomes) e os primeiros K bytes de
conteúdo (K ≤ 64 KiB), com permissões verificadas por inode com as credenciais do chamador.
Substitui N aberturas e 2N chamadas por uma; é a operação que o M4 mede contra "abrir um a
um". Escrita em lote não entra: a convenção tmp + atributos + fsync + rename continua sendo
a forma de criar memórias, por garantir atomicidade por arquivo no inferior.

## 7. UAPI

Extensão compatível da v1: `MOOB_UAPI_VERSION` passa a 2; `QUERY` e `GET_INFO` mantêm layout
e semântica; `GET_INFO` ganha `mount_rev`, `changes_capacity` e `changes_oldest`; novos
comandos `CHANGES` e `READ_BATCH`. Cliente v1 continua funcionando. Campos reservados zero,
`ENOTTY` para comando desconhecido, testes de entradas inválidas para cada campo novo.

## 8. O que muda no kernel

- Interceptar as operações que alteram memória para incrementar `rev` e `mrev`, gravar os
  atributos reservados e alimentar a lista de mudanças: `write` (no `end_write`), `truncate`,
  `setxattr`/`removexattr` tipados, `rename`, `unlink`, `create`. Tudo em caminhos já
  interceptados; custo adicional é um `setxattr` no inferior por mudança (medir).
- `QUERY` passa a filtrar por permissão do chamador.
- Dois ioctls novos e a lista circular.
- Nada em disco além de xattrs no inferior; o inferior desmontado continua legível e
  `moobfs-check` valida os reservados (formato, unicidade de `id`).

## 9. Entradas inválidas e testes

Reservados editados pelo cliente (`EPERM`); `id` duplicado no inferior (reportado por
`moobfs-check`, a camada mantém o primeiro e registra conflito no `GET_INFO`); `since` futuro
(`EINVAL`); `since` expirado (`EOVERFLOW`); lote acima de 256 ou K acima de 64 KiB (`EINVAL`);
`QUERY` de um usuário sem permissão num namespace (resultado omitido, sem erro, sem vazamento
de existência pelo `truncated`). Harness: os mesmos de M2 mais um teste multiusuário na VM
(dois uids, dois namespaces, consultas cruzadas) e replay de queda com `dm-log-writes`
verificando `mrev` persistido.

## 10. Fases

| Fase | Entrega | Saída |
| --- | --- | --- |
| 3a | Atributos reservados (`id`, `rev`, `mrev`, `created`, `modified`, `agent_last`), `EPERM` para o cliente, `moobfs-check` ciente | VM: replay de queda mantém `mrev` monotônico |
| 3b | `QUERY` com permissões do chamador; teste multiusuário | nenhum vazamento entre namespaces |
| 3c | `CHANGES` com lista circular; `EOVERFLOW` | cliente de memória usa o cursor em vez de varredura |
| 3d | `READ_BATCH` | M4 compara com abertura individual |
| 3e | Cliente de aceitação de dois agentes; registro de números preliminares | critério de entrada do M4 |

## 11. Questões em aberto

- Persistir a lista de mudanças no inferior (arquivo oculto) para sobreviver à remontagem, ou
  aceitar o `EOVERFLOW` e a varredura: decidir com os números do M4.
- Se `agent_last` deve ser uid numérico (estável) ou nome (legível); proposta: ambos.
- Se a atribuição de `id` deve ocorrer na criação ou na primeira gravação de conteúdo;
  proposta: na primeira gravação, para não dar identidade a temporários abandonados.
