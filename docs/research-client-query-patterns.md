# Padrões de consulta estruturada em sistemas de memória para agentes

**Abstract (English).** We read the source of six LLM-agent memory systems (ai-memory, mem0,
Letta in its archived V1 server and in the current Letta Code, Graphiti, the LangGraph store
with LangMem, and OpenAI Agents SDK sessions) to list every structured metadata filter they send
to their stores, separately from vector similarity. Almost every query carries one to three
equality predicates that express scope (user, agent, run, project, namespace), and the
dominant second need is a time range or an ordering by time with a small limit (5 to 100).
Three needs fall outside a kernel query limited to ANDed eq/lt/le/gt/ge/prefix/exists without
ordering: IN/OR lists (message roles, tag "any", kinds, group lists, categories), "missing or
greater than" tests on validity and expiry fields, and ordering by `created_at`/`updated_at`
with limit and cursor or offset. Counts appear in ai-memory and Letta, but in pagination and
administration, not in the retrieval path. Every system pushes the structured filter into the
same engine that ranks by vector or full text, before ranking. Isolation is almost always a
mandatory scope predicate or a namespace prefix enforced by the application, not by the store;
cross-scope queries exist in five of the six systems. Design E (simple kernel query, index
partitioned per permission domain) covers scope by construction and the conjunctive core;
design B (user-space SQLite index) covers the whole observed surface, including FTS, but
moves authorization out of the kernel.

Data: 2026-10-11. Método: clones rasos (`git clone --depth 1`) em diretório temporário, leitura
de código e documentação; nenhum código foi executado, nenhuma dependência instalada. As
citações são caminhos relativos à raiz de cada repositório.

| Sistema | Repositório | Commit lido | Data do commit |
| --- | --- | --- | --- |
| ai-memory | akitaonrails/ai-memory (main) | `88f8804` | 2026-10-10 |
| mem0 (OSS) | mem0ai/mem0 (main) | `b7ad69a` | 2026-10-07 |
| Letta V1 (servidor aposentado) | letta-ai/letta, branch `archive` | `56ba9c2` | 2026-08-13 |
| Letta Code (atual) | letta-ai/letta-code (main) | `271f119` | 2026-10-10 |
| Graphiti | getzep/graphiti (main) | `964bb11` | 2026-10-10 |
| LangGraph store | langchain-ai/langgraph (main) | `799c2f0` | 2026-10-11 |
| LangMem | langchain-ai/langmem (main) | `48e3c11` | 2026-10-02 |
| OpenAI Agents SDK | openai/openai-agents-python (main) | `125efa0` | 2026-10-08 |

O branch principal de `letta-ai/letta` (`5bcdd17`) não tem mais o servidor: o README aponta para
`letta-code` e para o branch `archive`, que guarda o servidor V1 com memória arquival e de
recordação. Os dois foram lidos porque representam modelos diferentes.

## Referência: o que o `QUERY` do MoobFS oferece hoje

`include/moobfs_ioctl.h`: até 4 termos (`MOOB_QUERY_MAX_TERMS`) em conjunção; operadores
`EQ, LT, LE, GT, GE, PREFIX` (só strings), `EXISTS`; até 4096 resultados
(`MOOB_QUERY_MAX_LIMIT`); saída com `count` (resultados visíveis copiados) e `truncated`; sem
OR, NOT, ordenação, cursor, contagem total ou agregação. Um valor por nome de atributo por
arquivo, no máximo 64 atributos tipados por arquivo (`docs/spec-m2.md`). Cada resultado é
autorizado com as credenciais do chamador (`docs/spec-m3.md` §3.1).

## 1. ai-memory

Fonte de verdade em markdown com front matter; o SQLite (`crates/ai-memory-store`) é índice
derivado. Ou seja, o projeto já é uma instância da alternativa B.

- **Busca principal** (`reader.rs`, `search_page_candidates_for_project`, linha 2329): FTS5
  `MATCH` + `workspace_id = ? AND project_id = ?` + `is_latest = 1` + `not_expired`, que é
  `(expires_at IS NULL OR expires_at > now)` (`not_expired`, linha 51). Quatro predicados
  estruturados, um deles com OR de ausência. `ORDER BY pages_fts.rank LIMIT ?`, com
  sobrecoleta `limit*4`, mínimo 20, máximo `limit+300` (`authority_candidate_limit`, linha 474).
- **Viagem no tempo** (`search_page_candidates_for_project_at`, linha 2426):
  `valid_from <= T AND (valid_to IS NULL OR valid_to > T)` + escopo + `not_expired`: cinco
  predicados, dois com OR de ausência.
- **Recentes** (`recent_pages_for_project`, linha 2867): escopo + `is_latest` + `not_expired`,
  `ORDER BY updated_at DESC LIMIT ?`. Versão global (`recent_pages_global`, linha 2936) troca o
  escopo pelo filtro de leitura do usuário.
- **Observações de sessão** (`session_observations_scoped`, linha 3032): `session_id`,
  `workspace_id`, `project_id`, FTS opcional e `kind IN (...)`; `COUNT(*)` total para a
  paginação, um segundo `COUNT(*)` com `NOT (workspace_id = ? AND project_id = ?)` para informar
  quantas foram omitidas, e `ORDER BY created_at {ASC|DESC}, id LIMIT ? OFFSET ?`.
- **Handoffs e mensagens** (linhas 5252, 6113, 6153): `workspace_id, project_id, state='open'`
  ou `state='pending'`, `ORDER BY created_at ASC LIMIT`; `pending_message_count` é `COUNT(*)`.
- **Vetores** (`load_embeddings`, linha 4217): escopo + `is_latest` + `provider`, `model`, `dim`
  (cinco igualdades); carrega os vetores do projeto e calcula similaridade no processo. O filtro
  roda antes do vetor e poderia ser servido por um índice de atributos separado.
- **Isolamento**: predicado obrigatório `workspace_id`/`project_id`; para leitura multiusuário,
  `readable_repository_sql` (linha 165) acrescenta um OR de três subconsultas (projeto não
  restrito ou criado pelo usuário, concessão em `project_grants`, escopo global). O comentário
  em `readable_repository_filter` (linha 78) exige filtrar antes do `LIMIT`, não depois.
  Consultas entre escopos são funcionalidade explícita: `scopes` (lista) e `global=true` no
  `memory_query` (`crates/ai-memory-mcp/src/server.rs`, argumentos perto da linha 585).
- **Tamanhos**: `memory_query` padrão 10, máximo 100 (`server.rs` linhas 1750 e 2683); handoffs e
  mensagens padrão 50, máximo 200; observações padrão 50, máximo 200.

## 2. mem0 (SDK OSS)

- **Escopo** (`mem0/memory/main.py`, `_build_filters_and_metadata`, linha 314): pelo menos um de
  `user_id`, `agent_id`, `run_id` é obrigatório (erro `VALIDATION_001`); `actor_id` opcional. São
  1 a 4 igualdades. `get_all` (linha 1269) e `search` (linha 1393) repetem a exigência.
- **Filtros extras** (`_process_metadata_filters`, linha 1538; docstring de `search`): `eq, ne,
  in, nin, gt, gte, lt, lte, contains, icontains`, curinga `*`, e `AND`/`OR`/`NOT` aninháveis.
  Chaves irmãs são AND implícito. A tradução fica com cada armazenamento vetorial: Qdrant
  `_create_filter` (`mem0/vector_stores/qdrant.py` linha 339); pgvector monta
  `payload->>k = ANY(...)`, `( ... OR ... )` e `NOT (...)` (`_build_filter_conditions`).
- **Plataforma hospedada** (`docs/platform/features/v2-memory-filters.mdx`): `user_id`,
  `agent_id`, `app_id`, `run_id` aceitam `eq, ne, in, *`; `created_at`, `updated_at`,
  `timestamp`, `expiration_date` aceitam faixas e `in`; `categories` aceita `in` (qualquer uma)
  e `contains`. Exemplos documentados: `{"AND": [{"user_id": ...}, {"categories": {"in":
  ["finance","health"]}}]}` e `created_at >= ...`.
- **Ordenação e paginação**: `search` ordena por pontuação combinada (semântica, BM25, entidades)
  com `top_k` padrão 20 e sobrecoleta interna `max(limit*4, 60)` (`_search_vector_store`, linha
  1642). `get_all` não ordena: Qdrant usa `scroll` sem ordem pedida (`qdrant.py` linha 573),
  pgvector faz `SELECT ... WHERE ... LIMIT` sem `ORDER BY` (`pgvector.py` linha 513). A
  plataforma pagina com `page`/`page_size` (`mem0/client/main.py` linha 397). Sem contagem no
  SDK OSS. Expiração é filtrada em Python depois da busca (`_payload_is_expired`), por isso a
  sobrecoleta.
- **Outras leituras**: no `add` (linha 920 em diante) busca até 10 memórias existentes só com as
  chaves de escopo; o histórico de conversa vem do SQLite local por `session_scope = ?
  ORDER BY created_at DESC LIMIT 10` (`mem0/memory/storage.py` linha 298); o armazenamento de
  entidades é listado com `top_k=10000` (linhas 589 e 669); `delete_all` lista em lotes de 1000.
- **Momento do filtro**: junto com o vetor, dentro do motor vetorial (pré-filtro do Qdrant,
  `WHERE` do pgvector). Qdrant cria índices de payload só para `user_id, agent_id, run_id,
  actor_id` (`_create_filter_indexes`, linha 166).
- **Isolamento**: predicado de igualdade obrigatório, na mesma coleção. Omitir uma chave
  atravessa os demais escopos (só `user_id` busca todos os agentes e execuções do usuário); a
  plataforma documenta curinga `*` e `in` sobre `user_id`. O armazenamento não autoriza nada.

## 3. Letta

**V1 (branch `archive`).** Memória arquival (`passages`) e de recordação (`messages`) em
Postgres/SQLite, com Turbopuffer opcional.

- `build_agent_passage_query` (`letta/services/helpers/agent_manager_helper.py` linha 1180):
  `organization_id = ?` sempre; `agent_id` via junção `archives_agents` ou `archive_id`; sem
  nenhum dos dois, busca em toda a organização. `created_at >= start` e `<= end`; texto por
  `lower(text) LIKE %q%`. Até cinco predicados em AND. Paginação por cursor `before`/`after`
  com `(created_at, id)` (OR lexicográfico) e `ORDER BY created_at ASC|DESC, id`; com vetor,
  `ORDER BY cosine_distance`.
- `query_agent_passages_async` (`letta/services/agent_manager.py` linha 2416): `limit` padrão 50;
  tags filtradas **depois** do SQL, em Python, em modo ANY (interseção) ou ALL (subconjunto),
  com um TODO para levar ao SQL. A ferramenta do agente usa `RETRIEVAL_QUERY_DEFAULT_PAGE_SIZE
  = 5` (`letta/constants.py` linha 458).
- Turbopuffer (`letta/helpers/tpuf_client.py`): passagens com `tags ContainsAny` (ANY) ou AND
  de `Contains` (ALL) e `created_at Gte/Lte` (linhas 955 a 1013), um namespace por arquivo
  (archive); mensagens com `agent_id Eq` sempre, `role In [...]`, `project_id`, `template_id`,
  `conversation_id`, faixa de `created_at` (linhas 1111 a 1181), um namespace por organização.
  Sem texto de busca, ordena por `created_at desc` (linha 840).
- Recordação (`conversation_search`, `letta/services/tool_executor/core_tool_executor.py`):
  `roles` (lista), `start_date`, `end_date`, limite padrão 5; o caminho SQL
  (`message_manager.py`, `search_messages_async` linha 1142) usa `list_messages` com `roles`,
  texto e `ascending=False`.
- Contagens: `agent_passage_size_async` (`COUNT`) e `get_tag_counts_for_archive_async`
  (`GROUP BY tag`) em `letta/services/passage_manager.py` linhas 955 e 1074, usadas pela API,
  não pela recuperação do agente.

**Letta Code (atual).** A memória do agente é uma árvore de arquivos markdown com front matter
restrito (`description`, `read_only`, `limit` no formato antigo; `name`, `description` no
memfs-v2; `src/memory-frontmatter.ts`), versionada em git, em `<raiz>/<agentId>/memory`
(`src/utils/local-backend-paths.ts`). A seleção é por prefixo de caminho (`system/` entra no
prompt; `src/backend/local/system-prompt-compilation.ts` linha 333). O isolamento entre agentes
é feito por sandbox de sistema de arquivos no kernel, com falha fechada
(`src/memory-confinement.ts`). Não encontrei consulta estruturada sobre atributos nesse código;
a busca é feita pelo agente com ferramentas de arquivo.

## 4. Graphiti (Zep)

- `SearchFilters` (`graphiti_core/search/search_filters.py`): `node_labels`, `edge_types`
  (`IN`), `edge_uuids`, `edge_source_node_uuids`, `edge_target_node_uuids` (`IN`),
  `valid_at`, `invalid_at`, `created_at`, `expired_at` como lista de listas de `DateFilter`
  (OR externo de AND internos, com `=, <>, >, <, >=, <=, IS NULL, IS NOT NULL`) e
  `property_filters`. Na montagem dos parâmetros de data, o nome é `valid_at_{j}` sem o índice do
  grupo OR; com mais de um grupo os valores parecem se sobrescrever. Não testei; registro como
  observação de leitura.
- O servidor MCP só emite AND: `build_fact_search_filters`
  (`mcp_server/src/utils/type_config.py` linhas 139 a 194) gera um único grupo com `>=` e/ou
  `<=` para `valid_at` e `invalid_at`, mais `edge_types`.
- **Escopo**: `group_id IN $group_ids` em cada busca (`search_utils.py` linhas 228, 336, 474);
  no fulltext, `group_id:"a" OR group_id:"b"` (linha 85). `group_ids=None` não filtra: a busca
  cobre o grafo inteiro. No FalkorDB cada `group_id` é um grafo separado e o decorador
  `handle_multiple_group_ids` (`graphiti_core/decorators.py` linha 29) executa uma busca por
  grupo e funde os resultados: partição física por escopo com fan-out, o mesmo desenho de E.
  O MCP usa o `group_id` padrão da configuração quando o chamador não informa
  (`search_memory_facts`, `graphiti_mcp_server.py` linha 616). Não há autorização: o
  chamador escolhe os grupos.
- **Ordenação e limites**: busca híbrida por RRF ou distância de nó, `DEFAULT_SEARCH_LIMIT = 10`
  (`search/search_config.py`), `max_facts` padrão 10 no MCP. `retrieve_episodes`
  (`utils/maintenance/graph_data_operations.py` linha 67): `valid_at <= ref`, `group_id IN`,
  `source = ?`, `ORDER BY valid_at DESC, created_at DESC LIMIT last_n`, com janela padrão 3
  (`EPISODE_WINDOW_LEN`). Listagem por grupo com cursor `uuid < ?`, `ORDER BY uuid DESC LIMIT`
  (`nodes.py` linha 675).
- **Momento do filtro**: dentro da mesma consulta Cypher que calcula similaridade ou fulltext.

## 5. LangGraph store e LangMem

- `SearchOp`/`BaseStore.search` (`libs/checkpoint/langgraph/store/base/__init__.py` linhas 203
  e 784): `namespace_prefix` (tupla), `filter` dict com `$eq, $ne, $gt, $gte, $lt, $lte`, chaves
  em AND, sem OR, IN, NOT ou existência; `limit=10`, `offset=0`. `list_namespaces` (linha 966):
  `prefix`, `suffix` com curinga `*`, `max_depth`, `limit=100`, `offset`.
- PostgresStore (`libs/checkpoint-postgres/langgraph/store/postgres/base.py`,
  `_prepare_batch_search_queries` linha 426): prefixo vira `(prefix = ? OR prefix LIKE '?.%')`
  (`_namespace_prefix_condition`, linha 1295), ancorado no índice `text_pattern_ops`;
  filtros em `value->k`; `$gt` e afins comparam `value->>k` como **texto** (linha 650), não
  como número. Sem `query`: `ORDER BY updated_at DESC LIMIT ? OFFSET ?`. Com `query`: subconsulta
  vetorial com os mesmos filtros no `WHERE` e sobrecoleta `limit * vetores_por_doc * 2 + 1`.
- InMemoryStore (`libs/checkpoint/langgraph/store/memory/__init__.py`, `_filter_items` linha
  238): filtra por prefixo e predicados em Python, depois pontua; operadores de faixa convertem
  para `float`.
- **Isolamento**: o namespace é o escopo. O exemplo oficial de autorização
  (`libs/sdk-py/langgraph_sdk/auth/__init__.py` linhas 86 a 92) prefixa a identidade do usuário
  em toda operação do store. Prefixo vazio `()` busca o store inteiro.
- LangMem (`src/langmem/knowledge/tools.py` linha 97): namespaces como
  `("memories", "{langgraph_user_id}")`; a ferramenta de busca expõe `limit=10`, `offset` e
  `filter` livre ao modelo (linha 393); a extração consulta com `query_limit=5`
  (`knowledge/extraction.py` linhas 847 e 1020).

## 6. OpenAI Agents SDK (sessões)

`src/agents/memory/sqlite_session.py`, `get_items` (linha 286): `WHERE session_id = ?
ORDER BY id DESC LIMIT ?` (ou tudo em ordem crescente). Uma igualdade de escopo e ordem de
inserção com limite. Não é memória semântica; serve como piso do que um cliente de histórico
precisa.

## 7. Claude Code (convenções públicas, não código)

Pela documentação pública (code.claude.com, consultada via Context7): `CLAUDE.md` em níveis
(usuário, projeto, `.claude/rules/*.md`, `CLAUDE.local.md`) e memória automática por projeto,
carregadas por caminho; o guia de hospedagem isola inquilinos por diretório próprio e
`settingSources` vazio. Não há consulta por atributos; escopo é diretório. Não li o código.

## Tabela comparativa

| Sistema | Filtros estruturados | Operadores | Combinação | Ordenação/paginação | Escopo |
| --- | --- | --- | --- | --- | --- |
| ai-memory | workspace, project, session, is_latest, expires_at, valid_from/to, kind, state, provider/model/dim | =, <=, >, IS NULL, IN, NOT | AND com OR de ausência; OR de autorização | rank FTS; updated_at DESC; created_at ASC/DESC; LIMIT/OFFSET; COUNT | ids de projeto obrigatórios; `scopes` e `global` cruzam, com filtro de leitura em SQL |
| mem0 OSS | user_id, agent_id, run_id, actor_id + metadados livres | eq, ne, in, nin, gt..lte, contains, `*` | AND implícito; OR/NOT opcionais | score; get_all sem ordem; top_k 20 | igualdade obrigatória (≥1 chave), mesma coleção; omissão cruza |
| mem0 plataforma | + app_id, created/updated_at, categories | idem + faixas de data, `in` em categorias | AND/OR/NOT | page/page_size | idem, curinga e `in` em user_id |
| Letta V1 | organization, agent/archive, created_at, tags, role, project, template, conversation, texto | =, >=, <=, IN, ContainsAny/All, LIKE | AND; tags ANY/ALL (pós-filtro no SQL) | created_at ASC/DESC + cursor (created_at,id); distância; COUNT e GROUP BY | org obrigatória; agente opcional; namespace TPUF por arquivo/org |
| Letta Code | caminho (prefixo `system/`) | prefixo de caminho | — | — | diretório por agente + sandbox do kernel |
| Graphiti | group_id, edge_types, labels, uuids, valid/invalid/created/expired_at, source | IN, =, <>, faixas, IS [NOT] NULL | DNF no core; AND no MCP | RRF/score; valid_at DESC LIMIT; cursor uuid | `group_id IN`; None = tudo; FalkorDB: grafo por grupo com fan-out |
| LangGraph store | prefixo de namespace + chaves do valor | $eq, $ne, faixas | AND | updated_at DESC LIMIT/OFFSET; score | prefixo de namespace; auth prefixa a identidade |
| OpenAI sessões | session_id | = | — | id DESC LIMIT | uma tabela, chave de sessão |

## Padrões comuns

1. **Escopo como igualdade ou prefixo, quase sempre presente.** Todos os sistemas com consulta
   estruturada põem 1 a 3 predicados de escopo em toda consulta: mem0 (até 4 contando
   `actor_id`), ai-memory (2), Letta (1 a 2), Graphiti (1 lista), LangGraph (1 prefixo),
   OpenAI (1). Nos que têm usuários, o escopo é imposto pela aplicação; o armazenamento não sabe
   quem pergunta (exceções: o SQL de leitura do ai-memory e o gancho de autorização do
   LangGraph Platform, ambos acima do armazenamento).
2. **Tempo é o segundo filtro.** Faixa em `created_at` (Letta, mem0 plataforma, Graphiti),
   validade bitemporal (`valid_at`/`invalid_at` no Graphiti; `valid_from`/`valid_to` no
   ai-memory) e expiração (`expires_at`, `expiration_date`). Os testes de validade e expiração
   são "ausente ou maior que", não uma faixa simples.
3. **Ordenação por tempo com limite pequeno** aparece em toda listagem sem consulta textual:
   `updated_at DESC` (ai-memory, LangGraph), `created_at` com cursor (Letta), `valid_at DESC`
   (Graphiti), id (OpenAI, mem0 histórico). Limites padrão entre 3 e 50; máximos documentados
   de 100 a 200; sobrecoletas internas de 60 a algumas centenas. Os únicos valores acima de 4096
   são a listagem do armazenamento de entidades do mem0 (10 000) e lotes de manutenção.
4. **Listas IN/ANY** aparecem em quatro sistemas: papéis de mensagem e tags ANY (Letta), tipos
   de observação (ai-memory), grupos, tipos de aresta e uuids (Graphiti), categorias e `in`
   (mem0). OR arbitrário é oferecido por mem0 e pelo core do Graphiti, mas os caminhos
   padrão que li não o emitem.
5. **Filtro junto com a classificação.** Em todos, o filtro estruturado vai no mesmo motor que
   ordena por vetor ou FTS (pré-filtro do Qdrant, `WHERE` do pgvector e do Cypher, filtros do
   Turbopuffer, subconsulta do PostgresStore). Exceções: tags do Letta SQL e expiração do mem0,
   filtradas depois, com sobrecoleta. Um índice de atributos separado só ajudaria se o motor
   vetorial aceitasse um conjunto de ids candidatos, ou com pós-filtro e sobrecoleta.
6. **Consultas entre escopos existem** em mem0 (omissão, curinga), ai-memory (`scopes`,
   `global`), Letta (organização inteira), Graphiti (lista ou `None`) e LangGraph (prefixo
   curto ou vazio). Na maioria são autorizadas só pela aplicação.
7. **Texto** (FTS, BM25, `LIKE`, `contains`) acompanha o filtro estruturado em ai-memory, mem0,
   Letta e Graphiti. Nenhum atributo tipado cobre isso.

## O que a alternativa E cobre e o que não cobre

Premissa: E mantém o `QUERY` atual e particiona o índice por domínio de permissão (por exemplo,
um diretório de namespace). O escopo deixa de ser predicado e passa a ser a partição.

**Cobre diretamente:**

- O escopo de todos os sistemas, sem gastar termo: `user_id`/`agent_id`/`run_id` do mem0,
  `workspace/project` do ai-memory, `agent_id` do Letta, `group_id` do Graphiti, o prefixo do
  LangGraph e `session_id` do OpenAI. A partição também fecha, por construção, o canal de tempo
  descrito em `docs/spec-m3.md` §3.1, porque a varredura não toca outros domínios.
- Conjunções de escopo + 1 a 3 predicados de igualdade ou faixa: `is_latest`, `state`, `kind`
  único, `source`, `created_at` entre duas datas (dois termos), `provider/model/dim`. A busca
  principal do ai-memory, sem FTS, cabe em 4 termos se o escopo sair dos termos.
- Tags em modo ALL até 4 tags, se cada tag for um atributo próprio com `EXISTS`
  (`user.moob.tag.<nome>`), dentro do limite de 64 atributos por arquivo.
- Limites observados: quase todos os limites de resposta ficam muito abaixo de 4096.

**Não cobre, com frequência observada:**

- **IN/OR** (4 dos 6 sistemas com consulta estruturada): papéis, tags ANY, kinds, categorias,
  listas de grupos e uuids. Solução no cliente: N consultas e união; com `QUERY_BATCH`
  (`docs/spec-m3.md` §4) vira uma chamada. Listas de uuids grandes (Graphiti) ficam ruins.
- **"Ausente ou maior que"** (ai-memory `expires_at` e `valid_to`; Graphiti `IS NULL`): E não tem
  `NOT EXISTS` nem OR. Solução: o cliente grava um sentinela (`valid_to = INT64_MAX`), o que
  é mudança de convenção de dados, não do kernel.
- **NOT/ne/nin** (mem0, Graphiti `<>`, contagem de omitidos no ai-memory): não expressável;
  exige ler e filtrar no cliente.
- **Ordenação por tempo com limite** (listagens dos seis sistemas): E devolve até 4096
  em ordem arbitrária. Se o conjunto casado na partição couber em 4096, o cliente ordena e
  corta; se não couber, "os N mais recentes" fica incorreto sem estreitar a faixa de tempo em
  passos (`created_at >= t`), o que custa várias chamadas e precisa de lógica adaptativa.
  Cursor `(created_at, id)` do Letta e `OFFSET` do ai-memory/LangGraph não têm equivalente.
- **Contagens** (ai-memory paginação e mensagens pendentes; Letta tamanho e contagem por tag):
  E informa `count` até o limite e `truncated`; contagem exata acima de 4096 e `GROUP BY` não
  existem. São usos administrativos, não de recuperação.
- **Texto e vetor**: fora do escopo de E e de qualquer índice de atributos; continuam no
  cliente ou num índice em espaço de usuário.
- **Consultas entre escopos**: viram fan-out por partição e fusão no cliente (o mesmo padrão
  que o Graphiti usa no FalkorDB), com autorização garantida pelo kernel em cada partição.

## O que a alternativa B cobre

B (índice SQLite em espaço de usuário) cobre toda a superfície observada: IN/OR/NOT,
`IS NULL`, ordenação por tempo com `LIMIT/OFFSET` ou cursor, `COUNT`/`GROUP BY`, FTS5 e,
com extensão ou cálculo no processo, vetores; é exatamente o desenho do ai-memory. O custo
documentado no projeto: a autorização sai do kernel e precisa ser reimplementada na consulta
(o ai-memory precisou de `readable_repository_sql` com três subconsultas, e a garantia vale
só para quem lê pelo servidor, não para quem abre o banco); o índice fica separado dos arquivos e precisa de
reconciliação com edições feitas fora dele (o ai-memory usa watcher e varredura periódica;
`docs/research-ai-memory.md`). Com um banco por domínio de permissão, B herda o isolamento POSIX
do arquivo do banco, mas consultas entre escopos voltam a ser fan-out.

## Limitações desta leitura

- Li caminhos de consulta, não todos os adaptadores: mem0 tem 27 armazenamentos vetoriais e
  li Qdrant e pgvector; Graphiti tem vários drivers e li a construção comum de filtros e o
  decorador do FalkorDB.
- Contagens de frequência são sobre os caminhos lidos e a documentação, não sobre uso real; não
  há telemetria de quais filtros clientes reais enviam.
- A plataforma hospedada do mem0 foi lida só pela documentação; seu servidor não é público.
- Letta V1 está aposentado; Letta Code foi lido de forma superficial e não encontrei nele busca
  por atributos, o que não prova que não exista no backend de nuvem.
- A observação sobre parâmetros de data no Graphiti não foi testada.
- LlamaIndex e cognee não foram lidos por falta de tempo. Claude Code foi considerado só pela
  documentação pública.
- Nada foi executado; limites efetivos podem ser alterados por configuração.
