# ai-memory (akitaonrails) — leitura técnica e relação com o MoobFS

Data: 2026-10-10. Fonte: clone raso de `github.com/akitaonrails/ai-memory` em `385b81e`
(release v2.6.3, 2026-10-09). Licença MIT. Criado em 2026-05-21; 9,1 mil estrelas, 636 forks,
7 issues abertas na data. Rust 2024, workspace com 10 crates principais e 5 companions.
Cerca de 396 mil linhas de Rust, das quais algo entre 40% e 55% são testes conforme o crate.
Nada foi compilado ou executado; a leitura foi do código e da documentação.

## O que o projeto é

Memória de longo prazo para agentes de codificação (Claude Code, Codex, Cursor, Gemini CLI e
mais vinte harnesses), compartilhada entre agentes, máquinas e pessoas. Um único binário
roda servidor HTTP + MCP, captura eventos de ciclo de vida via hooks, sanitiza, consolida em
páginas markdown e serve busca e "handoffs" tipados entre agentes.

Pipeline: `capture → consolidate → recall → handoff`. O caminho padrão não faz nenhuma
chamada a LLM; LLM é opcional para consolidação mais rica, reranking e um passo "dream".

## Modelo de armazenamento (confirmado no código)

**Duas camadas, uma fonte de verdade.** Decisão registrada em `docs/design-decisions.md` §3
("Option B: markdown em git é fonte de verdade, SQLite é índice derivado").

| Camada | O que é | Onde no código |
| --- | --- | --- |
| `<data_dir>/wiki/` | Árvore de `.md` com front matter YAML, repositório git (libgit2 vendorizado) | `crates/ai-memory-wiki` (19 mil linhas, ~55% testes) |
| `<data_dir>/db/memory.sqlite` | Índice derivado: FTS5, entidades, links, embeddings, sessões, observações, handoffs, auditoria | `crates/ai-memory-store` (58 mil linhas, ~37% testes), 76 migrações SQL |
| `<data_dir>/raw/` | Segmentos JSONL imutáveis de workstreams gerenciados | `crates/ai-memory-workstream` |

Layout do wiki: `<workspace-uuid>/<project-uuid>/{sessions,decisions,gotchas,procedures,
concepts,notes,_rules,_slots}/*.md`, mais `_meta.md`, `index.md`, `log.md`. Caminho da
página define o tipo OKF ("Open Knowledge Format" v0.2). Front matter carrega `kind`, `tier`,
`pinned`, `expires_at`, `session_id`, `agent`, `entities`, `tags`, `last_modified_by`,
`generated.{by,at}`.

**Escrita de página** (`Wiki::write_page`, `wiki.rs:2642-2790`): sanitiza, passa por
webhooks de admissão opcionais, grava o arquivo atomicamente (tmp + rename), faz upsert no
SQLite; se o upsert falha, restaura os bytes antigos. Commit git é explícito
(`commit_all`), chamado em SessionEnd, consolidação e checkpoints. Autor do commit é sempre
`ai-memory <ai-memory@local>`; não há branches, remotes, gc ou reescrita de história.

**Versionamento** (`pages`): cada edição cria uma nova linha; a anterior recebe `is_latest=0`
e `valid_to`, a nova aponta `supersedes`. Hash SHA-256 do corpo serve apenas para detectar
mudança no mesmo caminho: **não há deduplicação de conteúdo entre caminhos** (confirmado em
`ops.rs:1700-1770`). Decaimento (`decay.rs`) é um score de retenção por página episódica;
eviction é soft delete com tombstone; purga definitiva após 180 dias sem acesso. Semânticas
e fixadas (`pinned`) não decaem. TTL por `expires_at` vence `pinned`.

**Watcher** (`watcher.rs`): `notify` com debounce de 300 ms e reconciliação completa a cada
30 s. Não distingue escrita própria de edição humana: reindexa tudo e depende do curto-circuito
por hash + front matter no store para não criar versão. A função que captura inode em
`atomic.rs` existe mas seu retorno é descartado; o mecanismo documentado de supressão por
inode não está em uso. Remoções de arquivo não são reconciliadas por padrão.

**Concorrência:** um único escritor SQLite em thread dedicada (`WriteCmd` por mpsc), WAL,
pool de leitores. Um servidor por `data_dir`, protegido por `flock` no CLI.

**Abstração de backend: não existe.** `rusqlite` chamado diretamente em 29 mil linhas de
`ops.rs` + `reader.rs`; `Wiki` é struct concreta sobre `std::fs` com raiz fixa em
`data_dir/wiki`. Os únicos pontos de extensão são as traits `LlmProvider` e `Embedder`, os
**webhooks de admissão** (`docs/admission-webhooks.md`) e a política de companions
(integrar por HTTP/MCP, nunca remendar o núcleo). Nenhuma menção a FUSE, mount, filesystem
alternativo, Postgres, LanceDB, Qdrant ou "storage layer" como plano.

## Recuperação

`memory_query` funde até quatro fluxos por RRF: FTS5 sobre título+corpo, casamento de
entidades do front matter, vizinhança de links e cosseno sobre `page_embeddings`. Embeddings
locais por padrão desde 2.0 (`all-MiniLM-L6-v2`, 384 dims, candle, ~87 MB). Vetores são BLOBs
em linhas comuns, cosseno em força bruta; `sqlite-vec` é adiado com critérios numéricos
(`docs/vector-backend-policy.md`). Pós-fusão, um multiplicador de autoridade [0,55; 1,50]
favorece regras, decisões e procedimentos. `as_of` consulta versões históricas
("bi-temporal-lite", tempo de ingestão). 23 ferramentas MCP.

Benchmark publicado: LongMemEval-S hit@5 0,823 com embeddings locais, 0,668 sem LLM; harness
reproduzível em `evals/` e `docs/benchmarks/`. Teto de escrita medido: ~700 observações/s.

## Relação com o MoobFS

**O ai-memory está uma camada acima.** Ele é exatamente o papel que a ADR-0005 chama de
"cliente": captura, seleção semântica, montagem de contexto. Sua fonte de verdade é um
diretório de arquivos. O lugar natural do MoobFS é embaixo de `wiki/`.

**O que já está resolvido lá e não devemos duplicar:** sanitização de segredos, protocolo de
handoff, consolidação, busca híbrida, decaimento, multiusuário, integração com 20+ harnesses.

**O que o git + SQLite já dão e o MoobFS precisa superar para justificar-se ali:**

- Objetos git são endereçados por conteúdo: já há dedup de blobs idênticos e histórico de
  revisões, com commit por sessão. A cadeia `supersedes` no SQLite duplica isso no índice.
- FTS, grafo e vetores vivem no SQLite derivado e reconstruível.

**O que o MoobFS poderia oferecer de diferente:**

- Dedup de conteúdo **entre caminhos e sem commit**, que o ai-memory não faz no store e o
  git só faz no momento do commit.
- Revisões e snapshots **nativos**, sem a dupla contabilidade git + `supersedes`.
- Front matter (`tier`, `pinned`, `expires_at`, `type`, `session_id`, `agent`, `entities`)
  mapeia diretamente nos atributos tipados da ADR-0005; `QUERY` por atributo substituiria
  parte do índice de entidades e dos filtros de tier/TTL hoje em SQL.

**Incompatibilidades atuais, que mudam decisões do M2:**

1. O ai-memory escreve com `std::fs` (tmp + rename + fsync), libgit2 escreve `.git/`, SQLite
   exige WAL e locks POSIX. O M1 é somente leitura; a proposta original do M2 prevê escrita
   **só por ioctl `PUT`**. Para hospedar `wiki/` inalterado, o M2 precisa de escrita POSIX
   comum (`write`, `rename`, `fsync`, `unlink`), ou o ai-memory precisa de um adaptador.
2. `confinement.rs` rejeita symlinks e reparse points e valida `.git` dentro da raiz; um
   mount precisa se comportar como diretório comum.
3. Limites do M1 (128 arquivos, 64 KiB, namespace plano, ASCII) são insuficientes: páginas
   usam UTF-8 em caminhos aninhados por UUID e corpos de até 16 KiB por observação, mas
   sessões longas e páginas consolidadas passam disso.

**Ponto de integração sem tocar no núcleo:** os webhooks de admissão são chamados antes de
toda escrita, remoção, movimentação e handoff, e podem observar ou espelhar a página para um
sistema externo (exemplo `git-mirror` na documentação). Um webhook `moobfs-mirror` que faça
`PUT` de cada página com atributos derivados do front matter permitiria testar o MoobFS como
armazém de memórias reais **sem escrita POSIX e sem alterar o ai-memory**. É o caminho mais
curto para o M4 ter dados e cargas reais.

## Sequência recomendada

1. Especificação do M2 inclui requisito explícito: hospedar `wiki/` + `.git` + SQLite de um
   cliente real que usa `std::fs`. Isso implica escrita POSIX, não só ioctl.
2. Protótipo de webhook de admissão espelhando páginas em MoobFS (M2 mínimo) para coletar
   corpus e medir dedup real entre páginas.
3. M4 usa o harness de `evals/` e o teto de ~700/s como baselines; comparar `wiki/` em
   ext4 versus MoobFS com cargas idênticas.
4. Contato com o mantenedor só com resultado concreto em mãos. Como primeiro contato de boa
   fé, três divergências doc/código encontradas nesta leitura podem virar issues pequenas e
   verificáveis: inode de `atomic.rs` nunca consultado pelo watcher; `generated.by =
   human:<user>` documentado em `okf.md` mas não implementado (watcher usa `author_id: None`);
   descrição do crate store cita "sqlite-vec" enquanto a política o adia.

## Arquivos para ler primeiro no repositório deles

`docs/ARCHITECTURE.md`, `docs/design-decisions.md`, `docs/admission-webhooks.md`,
`docs/companion-crates.md`, `docs/okf.md`, `docs/vector-backend-policy.md`,
`crates/ai-memory-wiki/src/{wiki,git,watcher,atomic,confinement}.rs`,
`crates/ai-memory-store/src/ops.rs` (upsert em 1700-1870), `migrations/V01__init.sql`.
