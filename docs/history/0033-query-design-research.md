# 0033 — Pesquisas para a decisão sobre o papel do `QUERY` (ADR-0009)

Data: 2026-10-11. Estado: concluído (pesquisa; ADR-0009 segue como proposta).

## Objetivo

Reunir evidência externa para a ADR-0009, pedida pelo mantenedor: o que a literatura e os
projetos maduros fizeram com índices de metadados e permissões, e quais consultas os clientes de
memória reais fazem.

## Documentos

- `docs/research-metadata-search.md` e o adendo `docs/research-metadata-search-addendum.md`:
  estado da arte em busca de metadados de arquivos com permissões (GUFI, BFS e o livro de
  Giampaolo, Windows Search, Spotlight, WinFS, Tracker/LocalSearch, Baloo, Robinhood e changelogs
  do Lustre, Parker-Wood, `fanotify`), com cada fonte marcada como código, texto, resumo ou
  secundária.
- `docs/research-academic-survey.md`: varredura de conferências (FAST, ATC, SOSP, MSST, SC,
  CIKM, S&P, ICDE), grupos (UC Santa Cruz, CMU PDL, Notre Dame, Google, Microsoft) e laboratórios
  (LANL, OSTI) com rastreamento de citações pelo OpenAlex; lista as fontes sem resultado.
- `docs/research-client-query-patterns.md`: leitura do código de ai-memory, mem0, Letta,
  Graphiti, LangGraph/LangMem e OpenAI Agents SDK, com os commits lidos.

## Decisões refletidas na ADR-0009

1. A alternativa E (índice particionado por domínio no kernel, sem SQLite) foi acrescentada a
   pedido do mantenedor, com tabela comparando E e B.
2. A partição que fecha o canal de tempo é o conjunto de leitores, não o diretório: E precisa
   partir por (namespace, leitores) ou declarar namespaces homogêneos e fazer valer isso.
3. Os clientes reais isolam por uma a três igualdades ou prefixo de namespace, ordenam por tempo
   com limite pequeno e filtram dentro do próprio motor vetorial; E cobre o escopo e "os N mais
   recentes do domínio", e tem duas lacunas pequenas ("ou" entre valores; "ausente ou maior que").
4. Se B for adiante, o índice externo precisa de um contador monotônico de mudanças para evitar o
   problema do "novo inimigo" (Zanzibar).

## Limitações

Partes das fontes só em resumo (marcadas nos documentos); Semantic Scholar e dblp limitaram as
requisições, e o rastreamento de citações usou o OpenAlex; o artigo do GUFI (SC22) não estava em
acesso aberto. A numeração deste registro foi escolhida como 0033 porque 0032 já está em uso na
branch `community/dx-batch1`, de outra sessão de trabalho.
