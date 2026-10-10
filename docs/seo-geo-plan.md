# Auditoria e plano de SEO/GEO do MoobFS (proposta)

Data: 2026-10-10. Estado: **proposta; nada foi implementado.** Este documento não altera
nenhum outro arquivo. Base: leitura de `README.md`, `README.pt-BR.md`, `ROADMAP.md`,
ADRs 0005-0007, `docs/spec-m2.md`, `docs/spec-m3.md`, `docs/benchmark-plan.md`,
`docs/research-originality.md` (§4, 5, 7, 9), `docs/value-assessment.md` ("Entrega" e §9),
`docs/distribution-strategy.md` §8, `docs/community-plan.md` (§8, 9, 10, 13),
`docs/history/README.md`, `site/*.html`, `CHANGELOG.md` e `LICENSE`, na branch
`community/readiness`. README, ROADMAP, CONTRIBUTING e `.github/` estavam em edição por outro
agente; foram lidos apenas como estavam e podem mudar. Metadados do GitHub vêm do enunciado
(consulta de 2026-10-10) e não foram reverificados.

Princípio: SEO aqui significa que quem procura o assunto encontre uma descrição **correta e
sem exagero**. GEO (respostas de assistentes de IA) é consequência de texto claro, factual e
citável; nenhum arquivo ou metadado garante citação. Todas as frases públicas obedecem à lista
de afirmações a evitar de `docs/research-originality.md` §7.

---

## 1. Diagnóstico do estado atual

Prioridade: A = antes de qualquer divulgação; M = logo depois; B = quando houver capacidade.

| Item | Estado | Problema | Prioridade |
| --- | --- | --- | --- |
| Nome do repositório vs. projeto | Repo `llmoobfs`; projeto, README e site dizem "MoobFS" | Duas grafias dividem buscas e citações; `git clone` e badges usam `llmoobfs`. Renomear quebra clones antigos, mas o GitHub mantém redirecionamento (a confirmar o comportamento com Pages) | M (decisão do mantenedor) |
| Descrição do repositório | "Experimental native Linux filesystem for persistent LLM memory. Read-only shared-content milestone; no FUSE." | Desatualizada: omite camada empilhada, atributos tipados, `QUERY`; "read-only milestone" contradiz M2/M3 | A |
| Topics | 5: deduplication, filesystem, kernel-module, linux, llm-memory | Poucos; `deduplication` representa mal o foco atual (dedup é do btrfs/XFS, ver `research-originality.md` §7); faltam xattr, vfs, ext4/btrfs/xfs, ai-agents | A |
| Estrutura do README | Abre com M1 somente leitura; M2 aparece em um parágrafo denso; "What is implemented" ainda é tabela do M1; sem seção "what it is not", comparação ou FAQ | Quem chega lê um projeto que não é mais o atual; o banner experimental aprovado (community-plan §9) ainda não está no topo (a confirmar após a edição em curso) | A |
| Links internos | README aponta para `docs/*` em português sem aviso do idioma (alguns avisados: "in Portuguese") | Leitor anglófono cai em páginas PT sem saber; links de `site/` para o GitHub usam caminho `llmoobfs` | M |
| Visibilidade da licença | `LICENSE` GPL-2.0-only e badge no README; GitHub detecta a licença pelo arquivo | Adequado; falta citar no About a licença só se o GitHub a exibir (a confirmar na UI) | B |
| Releases e tags | Nenhum release, nenhuma tag (`git tag` vazio) | Sem versão citável; ferramentas e assistentes não têm um ponto estável para referenciar. Mas "sem release" é honesto e coerente com o estágio | M (decisão do mantenedor) |
| Changelog | `CHANGELOG.md` com seção única "Unreleased — 2026-10-10", detalhada | Útil e honesto, porém sem datas por fase nem links para registros; vira ruído sem um primeiro marco | B |
| Documentação de contribuição | `CONTRIBUTING.md`, `GOVERNANCE.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md` existem; `CONTRIBUTING.md` e `.github/` estão sendo reescritos | Fora do escopo; este plano só depende de que o README os linke | — |
| Discussions | Desativadas; wiki desativada | Sem canal público de perguntas; perguntas viram issues. Wiki desativada é correta (duplicaria `docs/`) | M (decisão do mantenedor) |
| Status de manutenção/experimental | Texto "Experimental" presente no título e em parágrafos; sem banner de limitações no topo; um autor, um kernel | Falta a declaração visível e uniforme de "não para produção" e das limitações (community-plan §9) | A |
| Metadados do site | `<title>` "MoobFS — A filesystem for LLM memory and context."; meta description ainda descreve o marco somente leitura; nenhuma tag `og:`, `canonical` ou JSON-LD nas três páginas; sem `robots.txt` nem `sitemap.xml` em `site/` | Título promete "a filesystem" sem "experimental"; descrição desatualizada; compartilhamentos sem pré-visualização | A (descrição e título), M (resto) |
| Conteúdo do site | `index.html` ainda mostra M1 como "Implemented today: read-only M1"; `guide.html` reproduz só o M1; `plan.html` diz que M2 "não está implementado" | Contradiz `ROADMAP.md` e o README; é a página mais indexável e a mais desatualizada | A |
| GitHub Pages | Ativo em `studiomoob.github.io/llmoobfs/`, servindo `site/` | Existe e funciona; é o ativo a corrigir, não a criar. Renomear o repo mudaria a URL | M |
| Idioma | README em EN, README.pt-BR e docs técnicos majoritariamente PT | Quase todo o conteúdo técnico que responderia às buscas em inglês está em português | M |
| Evidência pública | Marcadores `MOOBFS_*_PASS` no README; CI; registros `docs/history/` | Bom ativo de credibilidade; falta uma página única em inglês que resuma o que foi provado e o que não foi | M |

Pontos fortes a preservar: linguagem de limitações explícita, separação proposta/implementado/testado,
reconhecimento de anterioridade (SFS, BFS, YoloFS) e licença clara.

---

## 2. Descrição otimizada para o GitHub

Limite do campo: 350 caracteres. Contagens abaixo são aproximadas (conferir ao colar).

**Recomendada (~290 caracteres):**

> Experimental Linux kernel module (GPL-2.0, no FUSE): a stacked filesystem layer over ext4, btrfs or XFS that adds typed xattr attributes, an in-memory index and a QUERY ioctl authorized by the caller's POSIX credentials. Intended as storage for LLM-agent memory. Research project, not for production.

**Alternativa A (~245 caracteres, mais curta, foco no mecanismo):**

> Experimental stacked Linux filesystem layer (kernel module, no FUSE) over ext4/btrfs/XFS: typed extended attributes plus an in-memory index and a QUERY ioctl that honors POSIX permissions. Built to explore storage for LLM-agent memory. Not production-ready.

**Alternativa B (~265 caracteres, foco no uso por agentes):**

> Research project: kernel-level metadata queries for AI-agent memory stored as ordinary files. A stacked Linux filesystem module over ext4, btrfs or XFS; data stays readable without the module. Experimental, tested on one kernel in a VM, no performance numbers yet.

Critérios: diz "experimental"; nomeia o mecanismo real (`user.moob.*`, índice, `QUERY`);
não usa "first", "fastest", "secure", "production"; "no FUSE" fica porque é restrição de
desenho (AGENTS.md). A alternativa B reproduz as limitações centrais e é a mais conservadora.

Homepage do repositório: manter `https://studiomoob.github.io/llmoobfs/`.

Título e description do site (proposta, `site/index.html`):

- `<title>`: `MoobFS — Experimental stacked Linux filesystem layer for LLM-agent memory`
  (~75 caracteres; acima do ideal de ~60 para SERP, aceitável; alternativa curta:
  `MoobFS — experimental Linux filesystem layer for agent memory`).
- `<meta name="description">`: `Experimental Linux kernel module stacked over ext4, btrfs or XFS: typed xattr attributes, an in-memory index and a permission-aware QUERY ioctl for LLM-agent memory. Research project, not for production.`

---

## 3. Topics recomendados

Máximo do GitHub: 20. A coluna "Volume de busca" não traz números (nenhum foi medido);
"especulativo" significa que não sei se o termo é usado em volume relevante no GitHub.
Antes de aplicar, conferir a contagem de repositórios de cada topic na página
`github.com/topics/<nome>` (a confirmar), e preferir os que já têm comunidade ativa.

| # | Topic | Justificativa (uma linha) | Volume/uso |
| --- | --- | --- | --- |
| 1 | `linux-kernel` | Módulo de kernel; o topic é convencional para código do kernel | uso comum (a confirmar contagem) |
| 2 | `kernel-module` | Já presente; descreve o artefato `moobfs_layer.ko` | uso comum |
| 3 | `filesystem` | Já presente; categoria principal | uso comum |
| 4 | `vfs` | O projeto implementa operações do VFS Linux | de nicho, mas preciso |
| 5 | `stacked-filesystem` | Termo técnico exato do desenho (ADR-0007), como overlayfs/ecryptfs | especulativo (provavelmente poucos repositórios) |
| 6 | `xattr` | Atributos tipados vivem em xattr | de nicho, preciso |
| 7 | `extended-attributes` | Forma por extenso do anterior, para quem não conhece a abreviação | especulativo |
| 8 | `posix` | Autorização do `QUERY` segue permissões POSIX | uso comum |
| 9 | `ext4` | Inferior validado | uso comum |
| 10 | `btrfs` | Inferior validado | uso comum |
| 11 | `xfs` | Inferior validado | uso comum |
| 12 | `ai-agents` | Público-alvo declarado (memória de agentes) | uso comum |
| 13 | `llm` | Contexto de uso | uso muito amplo, baixa especificidade |
| 14 | `agent-memory` | Termo específico do problema | especulativo (crescendo; sem número) |
| 15 | `llm-memory` | Já presente; mantém continuidade | especulativo |
| 16 | `metadata` | A função central é consulta de metadados | uso amplo, baixa especificidade |
| 17 | `indexing` | Índice de atributos em memória | uso amplo (alternativa a `index`) |
| 18 | `c` | Linguagem do módulo e das ferramentas | uso muito comum |
| 19 | `qemu` | Testes de VM são parte central da evidência e atraem revisores de kernel | de nicho |
| 20 | `gpl-2-0` | Sinaliza licença compatível com o kernel | especulativo (o GitHub já mostra a licença) |

Removidos em relação ao atual: `deduplication` (dedup é do btrfs/XFS; manter induziria a
afirmação a evitar "Reflink/dedup do MoobFS"). Não usar: `memory` (ambíguo com RAM),
`rag`, `semantic-search`, `vector-database`, `ai-memory` (o projeto não faz busca semântica nem
vetores; seriam topics enganosos). Reservar 1-2 vagas (por exemplo, `gpl-2-0` ou `indexing`)
para trocar depois de ver as páginas de topics.

---

## 4. Reorganização proposta do README

Cabeçalhos em inglês (política de idioma, community-plan §13). Ordem e função de cada seção:

| # | Cabeçalho | Propósito (uma linha) |
| --- | --- | --- |
| 1 | `# MoobFS` + subtítulo + links + badges | Nome, frase de uma linha (a descrição da §2) e navegação (PT, site, contributing, roadmap) |
| 2 | `> Experimental research project. Not for production use.` (banner) | **Texto exato aprovado em community-plan §9**, com as sete limitações; verificar cada linha contra o repositório antes de publicar ("três defeitos" a confirmar contra os registros 0011-0015) |
| 3 | `## At a glance` | Cinco linhas: camada empilhada, atributos tipados, índice em RAM, `QUERY` com autorização POSIX, dados legíveis sem o módulo; um bloco de status (inferiores e kernel testados) |
| 4 | `## What MoobFS is, and what it is not` | Duas listas curtas; "is not": busca semântica, vetores, OR/texto livre, substituto de SQLite/git/ai-memory, FUSE, produção, dono do dedup/reflink |
| 5 | `## How it works` | Fluxo escrita e consulta em um diagrama (reaproveitar `site/plan.html`; criar versão em texto/Mermaid no repositório) com link para `docs/architecture-overview.md` |
| 6 | `## What is implemented, with evidence` | Tabela capacidade / estado (implementado, testado em qual kernel e inferior) / link para registro ou teste; substitui a tabela do M1; M1 vira nota de rodapé de regressão |
| 7 | `## Limitations` | Lista objetiva: um kernel, índice só em RAM, root inicial, sem userns/idmap, UAPI v1 pode mudar, sem desempenho medido, sem auditoria externa, TOCTOU e falso negativo após rename de diretório |
| 8 | `## Compared with plain files, SQLite and agent-memory frameworks` | Tabela curta derivada de `docs/value-assessment.md` ("Comparação honesta") e a regra "hoje, SQLite ou arquivos são a recomendação"; frameworks tratados como clientes |
| 9 | `## Quick start (disposable VM)` | `make vm-test` e marcadores esperados; aviso de não carregar em host; link para `docs/testing.md` |
| 10 | `## Documentation map` | Tabela: pergunta do leitor -> documento -> idioma (EN/PT) |
| 11 | `## Prior work and positioning` | Reconhecimento de SFS, BFS, overlayfs, ecryptfs, YoloFS (texto de `research-originality.md` §9, versão em inglês) |
| 12 | `## Contributing and security` | Link para CONTRIBUTING, pedidos de revisão (community-plan §8), SECURITY (contato e Advisories) |
| 13 | `## License` | GPL-2.0-only, link para `LICENSE` |

Regras: o README permanece em inglês; `README.pt-BR.md` espelha a estrutura (esta proposta não
o edita). Remover o conteúdo extenso do M1 do README e movê-lo para `docs/` ou `site/guide.html`
(já existe). Não usar "first", "fastest", "most secure". Limitar o README a ~200 linhas.

---

## 5. Estrutura de documentação SEO/GEO

### 5.1 Consultas-alvo e o que já as responde

| Consulta-alvo | Documento existente que a responde | Idioma | Lacuna |
| --- | --- | --- | --- |
| "Linux filesystem for AI agents" | `README.md` (parcial, desatualizado), `docs/spec-m2.md`, `ADR-0007` | EN parcial / PT | Nenhuma página em inglês que explique a arquitetura atual |
| "persistent memory for AI agents" | `README.md` objetivo; `docs/research-ai-memory.md` | EN / PT | Seção do README usa linguagem do M1; falta posicionamento frente a mem0/Letta/ai-memory em inglês |
| "kernel-level metadata indexing" | `docs/spec-m2.md`, `ADR-0005`, `layer/index.c` | PT | Sem texto em inglês |
| "POSIX filesystem for agent memory" | `docs/spec-m3.md` §3 e §3.1 | PT | Sem texto em inglês; contrato TOCTOU só em PT |
| "stacked filesystem for LLM memory" | `ADR-0007` | PT | Idem |
| "AI agent memory infrastructure" | `docs/value-assessment.md`, `docs/distribution-strategy.md` | PT | Sem resumo em inglês; comparações só em PT |
| "structured metadata queries on Linux" | `docs/spec-m2.md` (QUERY), `docs/spec-m3.md` | PT | Sem exemplo curto de uso do `QUERY` em inglês |

Conclusão: o conteúdo existe e é bom, mas está em português e disperso. A lacuna é de
**tradução/síntese em inglês**, não de volume de páginas. Sem páginas por palavra-chave.

### 5.2 Páginas novas (no máximo três, em inglês)

| Página | Onde | Origem | Observação |
| --- | --- | --- | --- |
| `docs/architecture-overview.md` | `docs/` (versionada com o código; o site pode linkar) | ADR-0005/0007, spec-m2, spec-m3 §3 | Camadas, fluxo de escrita e consulta, `user.moob.*`, índice, `QUERY`, contrato de autorização e TOCTOU, "nothing trapped". Responde às consultas 1, 3, 4, 5, 7 numa só página, com um título sem repetir palavras-chave |
| `docs/when-not-to-use.md` (título: "Comparison and when not to use MoobFS") | `docs/` | `value-assessment.md` "Entrega" e §9 | Tabela comparativa, casos em que pode valer e casos em que não vale, frase "today use SQLite or plain files"; responde às consultas 2 e 6 |
| `docs/faq.md` | `docs/`, com versão renderizada em `site/faq.html` | Seção 6 abaixo | Perguntas curtas e citáveis; cada resposta cita o arquivo de origem |

Regras: cada afirmação cita o arquivo; os números e estados são copiados, não reescritos; o
rótulo "proposta / implementado / testado" segue AGENTS.md; nenhuma página duplica o README.
Traduzir as ADRs/specs inteiras fica fora do plano (decisão da §13 do community-plan: sob
demanda externa).

### 5.3 GitHub Pages vs. página no domínio StudioMoob

| Critério | GitHub Pages (atual) | Página em domínio StudioMoob |
| --- | --- | --- |
| Estado | Existe, serve `site/`, gratuito | Não existe para o projeto (a confirmar) |
| Custo | Zero; sem hospedagem | Domínio/hospedagem já pagos pela empresa (a confirmar); esforço de manter duas fontes |
| Proximidade do código | Mesmo repositório; alterações no mesmo PR | Duplicação ou deploy adicional |
| Autoridade de domínio | Subdomínio `github.io` compartilhado; autoridade própria baixa | Domínio da empresa pode ter mais autoridade (**a confirmar**, não medido) e dá URL estável independente do nome do repositório |
| Risco | Renomear o repo muda a URL | Dependência de outro sistema; risco de divergir do repositório |
| Credibilidade técnica | Natural para um projeto de código aberto | Pode reforçar a marca da empresa, mas parece marketing se não for estritamente técnico |

Recomendação (sem implementação imediata): **manter e corrigir o GitHub Pages**. Reavaliar um
domínio próprio (por exemplo um subdomínio apontando para o Pages, com `CNAME`) só depois do M4,
se a decisão sobre o nome do repositório criar necessidade de URL estável. Um `CNAME` evita
duplicar conteúdo e preserva links. Não criar uma segunda página com o mesmo texto.

---

## 6. FAQ técnica (English)

Respostas em inglês para `docs/faq.md`; entre colchetes, a origem. Nenhuma contém "first",
"fastest" ou "most secure".

**1. What is MoobFS?**
MoobFS is an experimental GPL-2.0 Linux kernel module, stacked over ext4, btrfs or XFS, that
passes POSIX operations to the lower filesystem and adds typed `user.moob.*` attributes, an
in-memory attribute index and a `QUERY` ioctl. It is a research project with no release.
[`README.md`; `docs/research-originality.md` §9]

**2. What problem does it target?**
It explores storing LLM-agent memories as ordinary files while letting a program ask which
files have given attribute values, with the operating system's permissions deciding what each
caller sees. The narrow hypothesis is attribute queries with per-result POSIX authorization,
no separate database server and no second authorization policy. That hypothesis is not yet
shown to cost less than SQLite plus application-level checks. [`docs/value-assessment.md`, "Conclusão fundamentada"]

**3. How does the stacking work?**
The module mounts with `mount -t moobfs_layer -o lower=<dir>` and delegates file I/O to the
lower directory through the kernel's backing-file helpers, in the same family of designs as
overlayfs and ecryptfs. Attributes are real xattrs on the lower filesystem, validated by the
layer on write. [`docs/decisions/0007-stacked-memory-layer.md`; `README.md`]

**4. What does `QUERY` do?**
`QUERY` is an ioctl on the mount root that returns files matching up to four attribute
predicates joined by AND (operators `=`, `<`, `<=`, `>`, `>=` and prefix) with a result limit.
Answers were checked against a user-space oracle on ext4, btrfs and XFS. [`docs/spec-m2.md`; `CHANGELOG.md` phase 2d]

**5. What does `QUERY` not do?**
It has no OR, no sorting or aggregation, no full-text search and no vector or semantic
search. The agent application selects relevant memories and builds the model's context; the
filesystem does not extend a context window. [`docs/value-assessment.md`; `docs/research-originality.md` §7]

**6. How is `QUERY` authorized?**
For each candidate, the kernel resolves the recorded path with the caller's credentials and
requires `inode_permission(MAY_READ)` on the resolved inode, so mode bits and ACLs delegated to
the lower filesystem apply per result. Unauthorized results are omitted silently, and
`count`/`truncated` refer only to visible results. This was tested with two uids on three
lower filesystems; no external audit exists. [`docs/spec-m3.md` §3.1; `docs/history/0020`]

**7. What is the TOCTOU contract?**
A result is not a grant of access. Authorization is checked when the query runs; permissions or
names may change afterwards, so a client must call `open()` normally and accept `EACCES` or
`ENOENT`. There is no guarantee of the absence of a time-of-check/time-of-use window in UAPI
v1. A known limitation is possible false negatives after a directory rename until the next
mount. [`docs/spec-m3.md` §3.1]

**8. What is implemented today?**
A stacked pass-through layer validated over ext4, btrfs and XFS with `fsx`, typed attributes,
the in-memory index, the `QUERY` and `GET_INFO` ioctls, delegated reflink and dedupe, and
acceptance runs with git, concurrent WAL-mode SQLite, `dm-log-writes` power-loss replay and
`dm-flakey` error injection. Phase 3b (authorization in `QUERY`) closed with a KASAN run.
[`README.md`; `ROADMAP.md`; `CHANGELOG.md`]

**9. What is not implemented?**
Persistent index (it lives in RAM and is rebuilt at mount), user-namespace and idmapped
mounts (rejected), batch reads, a change cursor, revisions, packaging, a stable UAPI, a CLI,
and the full xfstests suite. Performance is not measured. [`docs/spec-m3.md`; `docs/distribution-strategy.md` §2.9; `docs/research-originality.md` §9]

**10. How does it differ from SQLite next to plain files?**
SQLite offers full SQL, composite indexes and decades of maturity; MoobFS offers a single
store of files plus xattrs and authorization by the OS per result. Writes to SQLite and to the
files are two operations; MoobFS writes a file and an xattr, but file/metadata atomicity is
the client's convention. Today SQLite or plain files are the recommendation for anything
outside a lab. [`docs/value-assessment.md` "Comparação honesta"]

**11. How does it differ from plain files and from FUSE-based systems?**
Plain files need `find` plus `getfattr`, a linear scan, with no query call. FUSE systems run
filesystem logic in a user-space process; MoobFS is a native kernel module, which gives
different trade-offs (a kernel bug can crash the host). No speed comparison has been
measured. [`AGENTS.md`; `docs/value-assessment.md`; `docs/benchmark-plan.md`]

**12. Is it a competitor to mem0, Letta or ai-memory?**
No. They are memory frameworks and clients; MoobFS is a possible storage layer under them.
The M4 benchmark plan treats real clients (ai-memory, mem0, Letta) as workloads. Prior art in
agent filesystems such as YoloFS (2026) is acknowledged. [`docs/benchmark-plan.md`; `docs/research-originality.md` §3, §9]

**13. What is proven and what is still a hypothesis?**
Proven in the author's VM runs: answer correctness against the oracle, two-uid isolation on
three lower filesystems, git/SQLite acceptance, and a KASAN/lockdep run on kernel 6.12.112.
Hypotheses pending M4: any speed or efficiency advantage, behavior near the index budget,
scale and long-running operation. There is no independent reproduction yet.
[`docs/research-originality.md` §5; `docs/value-assessment.md` "Medições do M4"; `docs/benchmark-plan.md`]

**14. Which kernels are supported?**
One test target: Debian kernel 6.12.111 in a QEMU VM, plus a 6.12.112 KASAN debug kernel on
demand. Kernel APIs change between versions; compatibility with other kernels is not
promised. [`README.md`; `docs/community-plan.md` §9]

**15. Is it production-ready?**
No. It is a research project with no release; a module bug can cause a kernel panic or data
loss, and the project's own tests found three kernel bugs during development (count to be
confirmed against records 0011 to 0015). [`docs/value-assessment.md`; `docs/community-plan.md` §9]

**16. How can I try it?**
With Docker, `make vm-test` builds the tools and module and boots a disposable QEMU VM with the
pinned kernel; no host module loading is needed. Expect passing markers such as
`MOOBFS_LAYER_PASS`. Do not load the module on a machine you care about. [`README.md`; `docs/testing.md`]

**17. How is data read without the module ("nothing trapped")?**
Memories are ordinary files and xattrs on the lower filesystem. After unmounting, `grep`,
`getfattr`, `git` and `rsync` read them directly. This follows from the design and was
exercised with git and `fsx`; reading the lower directory after a specific unmount sequence is
rated "probable" in the originality audit. [`docs/decisions/0007`; `docs/research-originality.md` §5]

**18. What license applies?**
GPL-2.0-only for the whole project. [`LICENSE`; `README.md`]

**19. How do I contribute?**
Start with `CONTRIBUTING.md`: reproducible bug reports, kernel or format review and
reproducibility improvements are welcome, with a `Signed-off-by` on each commit (DCO) and KASAN
required for changes under `layer/` or `vm/`. Review requests with bounded questions are listed
in the community plan. [`CONTRIBUTING.md`, still being rewritten; `docs/community-plan.md` §8 and §13]

**20. How do I report a security issue?**
Do not open a public issue. Use the private GitHub Security Advisory channel or the contact in
`SECURITY.md`; the planned response times are acknowledgement in 7 days and assessment in 30.
[`SECURITY.md`; `docs/community-plan.md` §13]

---

## 7. Estratégia de divulgação legítima

Regra geral: divulgar **resultados reproduzíveis**, não promessas. Cada publicação linka o
commit exato, o comando e os logs. Antes do M4, nenhuma comparação de desempenho.

| Canal | Quando | Enquadramento honesto | Observações |
| --- | --- | --- | --- |
| Pedidos de revisão (issues `kernel-expert`/`security`) | Já (após o merge da fase 3b, ação 15-18 do community-plan) | Pergunta delimitada, pacote para o revisor (community-plan §8) | Sem reivindicar auditoria; crédito ao revisor |
| `r/linux_kernel` | Só com algo discutível tecnicamente (a confirmar regras do subreddit e se aceitam projetos próprios) | "Experimental stacked fs, here are the limits, looking for review" | Evitar postar se as regras proibirem autopromoção |
| kernelnewbies (lista/IRC) | Perguntas de aprendizado sobre APIs do VFS, quando houver dúvida real | Pergunta específica, não anúncio | A confirmar a política atual da lista |
| LKML / linux-fsdevel | **Somente** com pergunta técnica concreta para os mantenedores (por exemplo, uso de `vfs_path_lookup` no ioctl); código fora da árvore e módulo experimental não justificam RFC | Pergunta curta, patches/links reproduzíveis, ciente de que o módulo está fora da árvore | Seguir as regras da lista; nunca divulgação |
| LWN | Não solicitar cobertura; só um comentário factual onde o assunto já estiver em pauta | — | Não escrever "pitch" |
| Hacker News "Show HN" | Após M4 publicado ou uma demonstração reproduzível clara; enquadramento "experimental, one kernel, no benchmarks yet (or: here are the raw numbers)" | O autor responde em tempo real, admite limites | Título sem superlativos; sem pedir votos |
| Artigos técnicos | Após o M4, com dados brutos, scripts e o commit; antes, um texto sobre a abordagem de teste (`dm-log-writes`, KASAN) é possível | Descrever método, falhas encontradas e limitações | Publicar os números mesmo se desfavoráveis (previsto em ADR-0007) |
| Participação em projetos relacionados | Quando houver relevância técnica: YoloFS (comparação de abordagem), ai-memory, Letta, mem0 (integração como cliente; issue só se houver problema reproduzível de interoperabilidade) | Contribuição ou pergunta concreta, sem anúncio | Respeitar as regras de cada projeto; não abrir issues de divulgação |
| Revisores externos | Convites diretos a pessoas escolhidas por afinidade técnica, com o pacote da §8 do community-plan | Sem pressão; "revisado" nunca significa "aprovado" | Registrar achados em issue/advisory e diário |
| Documentação de contribuição e issues "good first" | Contínuo | Issues sem kernel (community-plan §4) | Mantém qualificados os contribuidores |

Excluído explicitamente: estrelas ou forks falsos ou comprados; links pagos; spam ou postagem
automatizada indiscriminada; contas múltiplas; pedir estrelas em massa; `llms.txt` ou
metadados artificiais apresentados como garantia de citação por IA (se um dia for adicionado,
será apenas um índice de documentos, sem promessa); texto escrito para enganar assistentes;
afirmações da lista de `research-originality.md` §7. Em respostas de IA, o único método
legítimo é tornar a documentação correta, estável e citável.

---

## 8. Plano de implementação priorizado

Esforço: S (até 2 h), M (meio dia a 2 dias), L (mais de 2 dias). Itens marcados
"decisão" dependem do mantenedor.

| # | Ação | Esforço | Depende de | Decisão do mantenedor |
| --- | --- | --- | --- | --- |
| 1 | Atualizar a descrição do repositório (§2) e os topics (§3) no GitHub | S | — | Escolha entre recomendada/A/B; remover `deduplication` |
| 2 | Atualizar `<title>`, meta description e o bloco de status de `site/index.html` (e `guide.html`/`plan.html`) para M2/M3; adicionar banner experimental | M | Texto aprovado de community-plan §9; README reescrito | — |
| 3 | Reorganizar o README conforme §4 (inclui o banner) | M | Fim da edição em curso do outro agente | — |
| 4 | Espelhar a estrutura em `README.pt-BR.md` | S | 3 | — |
| 5 | Criar `docs/architecture-overview.md` (EN) | M | Fatos de ADR-0005/0007, spec-m2, spec-m3 | — |
| 6 | Criar `docs/when-not-to-use.md` (EN) a partir de `value-assessment` | M | — | — |
| 7 | Criar `docs/faq.md` (EN) com a §6 revisada contra o código atual | M | 5, 6 (para links) | — |
| 8 | Acrescentar ao site: `faq.html`, `og:`/`canonical`, `robots.txt`, `sitemap.xml`, link para a arquitetura | M | 2, 7; URL final do site (depende de 12) | — |
| 9 | Ativar Discussions (categorias: Q&A, Ideas, Show and tell) ou manter só issues | S | Capacidade de moderar | **Decisão: ligar ou não**; sugestão: ligar após o merge da fase 3b se houver tempo para responder, caso contrário manter desligado e reconhecer o canal no CONTRIBUTING |
| 10 | Criar a primeira tag/release `v0.1.0-experimental` (pré-release no GitHub) com notas ligadas a `CHANGELOG.md` | M | Merge da fase 3b; CHANGELOG com seção datada | **Decisão: criar ou não.** Pró: ponto citável. Contra: sugere estabilidade e UAPI v1 pode mudar; distribution-strategy coloca versão pública após M4. Se criar, marcar como "pre-release" e usar o nome com "experimental" |
| 11 | Ajustar `CHANGELOG.md`: seção "Unreleased" separada por fase com links aos registros | S | 10 | — |
| 12 | Renomear o repositório `llmoobfs` para `moobfs` | M | Checar disponibilidade do nome na organização; verificar redirecionamento de clone, Pages, badges e links | **Decisão: renomear ou manter.** Renomear alinha nome e projeto, e o GitHub redireciona repositório e clones, mas a URL do Pages muda (a confirmar redirecionamento). Fazer antes de divulgar, uma única vez; atualizar todos os links e badges |
| 13 | Medir o tráfego base (Insights/Traffic, Pages com Search Console) antes de qualquer divulgação | S | 2, 8 | — |
| 14 | Divulgação seletiva (§7), começando por pedidos de revisão | M | 1-8; fase 3b fechada | — |
| 15 | Publicar o benchmark M4 e o artigo técnico | L | M4 concluído | Se publica números desfavoráveis (já previsto) |
| 16 | Reavaliar domínio próprio/`CNAME` | S | 12, 15 | Sim/não |

Ordem sugerida: 1, 12 (se for fazer), 3, 4, 2, 5-7, 8, 9, 10, 11, 13, 14, 15, 16. Os itens 1 e 2
corrigem informação desatualizada e não dependem de decisão estratégica.

---

## 9. Critérios de sucesso em 30, 60 e 90 dias

O objetivo é **credibilidade e contribuidores qualificados**, não visitas. Nenhuma meta abaixo
é garantia; as faixas são hipóteses de ordem de grandeza para um projeto novo, de nicho
(kernel), sem divulgação paga; ajustar após medir a linha de base da ação 13.

### Métricas disponíveis no GitHub

| Métrica | Onde | Uso |
| --- | --- | --- |
| Visitantes e clones únicos, 14 dias | Insights > Traffic | Detectar picos após uma publicação; dados com retenção curta, exportar manualmente a cada 14 dias |
| Referrers e conteúdo popular | Insights > Traffic | Ver quais páginas (README, docs, site) atraem e de onde |
| Estrelas e forks | Página do repositório | Sinal fraco; interpretar apenas com o perfil de quem interage |
| Issues e PRs de contas novas | Issues/PRs | Sinal forte: pedidos de revisão respondidos, PRs de documentação ou testes |
| Resultados do CI e execuções do KASAN | Actions | Qualidade contínua |

### Métricas que exigem ferramentas externas

| Métrica | Ferramenta | Observação |
| --- | --- | --- |
| Impressões, consultas e posição das páginas do Pages | Google Search Console (verificação do site, a confirmar viabilidade para `github.io`) e Bing Webmaster | Dados só aparecem semanas após a indexação |
| Presença em respostas de IA | Amostragem manual mensal de ~10 perguntas (as consultas da §5.1) em alguns assistentes, registrando a data, o modelo e se há citação correta | Sem promessa de cobertura; os resultados variam entre execuções |
| Citações em textos técnicos | Busca manual por menções ao repositório (links) e alertas | Registrar o contexto e se a descrição está correta |

### Faixas por período

| Período | Qualidade (prioridade) | Alcance (informativo) | Racional |
| --- | --- | --- | --- |
| 30 dias | Metadados, README, site e FAQ corrigidos e coerentes entre si; 0 afirmações da lista §7 do `research-originality.md`; 1 ou 2 pedidos de revisão publicados; resposta a toda issue em até ~7 dias | Estrelas: de poucas unidades a ~20 (faixa especulativa, a maioria vindo de contatos diretos); visitas únicas semanais em dezenas | Sem divulgação ampla, o tráfego tende a vir de contatos do autor; uma contagem baixa não indica falha |
| 60 dias | Pelo menos 1 revisão externa substantiva ou 1 PR externo (documentação, teste ou reprodução); Pages indexado (aparece no Search Console); 1 reprodução independente do `make vm-test` registrada ("a confirmar" se ocorrer) | Estrelas ~20 a ~80 se houver um post técnico bem recebido (hipótese); 0 a 3 contribuidores externos | O principal limitador é o tempo de revisores de kernel, não a descoberta |
| 90 dias | Plano de benchmark M4 executado ou com data de publicação; decisão sobre release tomada e registrada; amostra mensal de IA mostra descrições corretas (ou nenhuma citação, que é aceitável) | Estrelas ~50 a ~250 apenas se houver publicação de resultados do M4 (muito incerto); 1 a 5 contribuidores externos | Interesse depende de resultados mensuráveis; um M4 desfavorável também é resultado válido (ADR-0007) |

Alertas de que algo está errado: picos de estrelas sem issues ou clones, referrers
suspeitos, contas novas sem histórico pedindo promoção ou oferecendo serviços. Não agir sobre
estrelas artificiais; se detectadas, ignorá-las e não amplificá-las.

---

## 10. Arquivos e configurações que precisariam ser alterados

Lista consolidada; **nenhum foi alterado por este documento**. Os arquivos marcados (*) estão
em edição por outro agente: coordenar antes.

| Arquivo | Mudança |
| --- | --- |
| `README.md` (*) | Reorganização da §4; banner aprovado; links para `docs/architecture-overview.md`, `docs/when-not-to-use.md`, `docs/faq.md`; sem alegações da lista §7 |
| `README.pt-BR.md` (*) | Espelhar a estrutura; manter os mesmos limites |
| `ROADMAP.md` (*) | Estados "concluído com evidência / planejado / dependente de validação" (community-plan §10); sem datas; links para os registros |
| `CONTRIBUTING.md` (*) e `.github/` (*) | Ligação com Discussions (se ativadas), pedidos de revisão e a política de idioma; sem mudança deste plano |
| `site/index.html` | Novo `<title>` e meta description (§2); status "M2/M3" em lugar do M1; banner experimental; `og:title`, `og:description`, `canonical` |
| `site/guide.html`, `site/plan.html` | Descrições e avisos atualizados; guia de reprodução do layer (hoje cobre só M1); `plan.html` deixa de dizer que M2 não está implementado |
| `site/faq.html` (novo) | Renderização do FAQ da §6 |
| `site/robots.txt`, `site/sitemap.xml` (novos) | Descoberta de páginas pelo Pages; URL final depende da decisão de renomear |
| `docs/architecture-overview.md` (novo, EN) | §5.2 |
| `docs/when-not-to-use.md` (novo, EN) | §5.2 |
| `docs/faq.md` (novo, EN) | §6, revisado contra o código antes de publicar |
| `docs/history/` e `CHANGELOG.md` | Registro desta rodada (quando implementada) e, se houver, a nota da primeira tag; histórico de decisão sobre renomear e Discussions |
| `docs/decisions/` | Uma ADR curta só se o nome do repositório mudar, ou se a política de releases for definida (ID sequencial; não alterar as existentes) |

Configurações do GitHub (fora do repositório, feitas pelo mantenedor):

| Configuração | Mudança proposta |
| --- | --- |
| Description | Texto da §2 (uma das três opções) |
| Topics | Lista da §3, até 20 |
| Website | Manter `https://studiomoob.github.io/llmoobfs/` (ou a nova URL após renomear) |
| Discussions | **Decisão do mantenedor** (ação 9); se ligadas, categorias Q&A, Ideas, Show and tell |
| Wiki | Manter desativada |
| Releases | **Decisão do mantenedor** sobre `v0.1.0-experimental` (ação 10), como pre-release |
| Nome do repositório | **Decisão do mantenedor** (ação 12) |
| Social preview | Imagem 1280x640 sem alegações (a confirmar se já existe) |
| Security | Manter Security Advisories privadas habilitadas (contato em community-plan §13) |

Pendências "a confirmar": comportamento exato do redirecionamento de Pages após renomear;
viabilidade do Search Console para `github.io`; regras vigentes dos subreddits e listas;
contagem atual de repositórios por topic; existência de imagem de pré-visualização social;
número de defeitos de kernel (registros 0011 a 0015); texto atual do README após a edição
em curso.
