# 0019 — Decisões a partir de uma revisão externa da arquitetura

Data: 2026-10-10, America/Sao_Paulo. Estado: decisões registradas; implementação nos marcos
indicados.

## Contexto

Uma revisão produzida fora do projeto propôs um "prompt mestre" com visão estratégica, análise
de projetos, oportunidades, arquitetura de referência em quatro camadas, plano de benchmark,
roadmap em quatro fases e critérios de diferenciação. Ela foi confrontada com o estado real do
repositório e com as ADRs.

## Aceito e incorporado

1. **Isolamento entre agentes e namespaces** entra no escopo do M3 como parte do modelo de
   identidade (agente, namespace, memória), expresso em atributos tipados e permissões POSIX,
   sem lógica de política no kernel. Era a lacuna real apontada pela revisão.
2. **Metodologia de benchmark** para o M4: p50/p95/p99, amplificação de escrita, tempo de
   montagem e de reconstrução do índice, aquecimento, repetições e dispersão; baselines ext4 e
   btrfs com SQLite mantido pela aplicação; clientes reais (ai-memory, mem0, Letta) como
   cargas; escalas atreladas ao orçamento do índice. Documentado em `docs/benchmark-plan.md`.
3. **Operações em lote e cursor de "mudanças desde a revisão X"** como candidatos do M3, em
   extensão compatível da UAPI v1.
4. Referências adicionadas ao levantamento: letta-code, Agent VFS e GraphRAG, classificados
   como cliente, referência de interface e fonte de dataset, respectivamente.

## Recusado, com motivo

- Persistência do índice como entrega de curto prazo: segue condicionada a medição.
- Escalas de 10 milhões de memórias e 1.000 agentes como requisito: são hipóteses do M4.
- Letta MemFS, Agent VFS e GraphRAG como concorrentes: são espaço de usuário ou pipelines.
- SDK, API HTTP e MCP como camada do repositório: trabalho do cliente; sem servidor próprio.
- PostgreSQL com pgvector como baseline do `QUERY`: compara operações diferentes.
- Validade e políticas de retenção como lógica no módulo: são atributos interpretados pelo
  cliente.
- O diagnóstico, a auditoria e a análise competitiva pedidos como "primeira ação" já existiam
  nos registros 0001 a 0016 e em `docs/ecosystem.md`.

## Mudanças

`docs/benchmark-plan.md` (novo), `ROADMAP.md` (M3 e M4), `docs/ecosystem.md`, este registro.
Nenhum código.

## Adendo (mesmo dia): segunda rodada da revisão externa

A revisão respondeu com um plano M3/M4 alinhado às ADRs. Incorporado: identidade lógica
`(namespace, agent, memory_id)` como atributos tipados comuns, gerados pelo cliente, sem
atributos reservados nem contadores no kernel (a proposta anterior de `id`/`rev`/`mrev`
atribuídos pela camada fica adiada por custo e por criar estado não reproduzível pelo
cliente); invariantes de isolamento e a correção do `QUERY`, que hoje devolve toda entrada do
índice sem verificar as permissões do chamador; `QUERY_BATCH` e `READ_BATCH` condicionados à
medição de W2 e W3; revisão lógica e histórico mantidos no cliente, cursor de enumeração só
se o M4 justificar; cargas W1 a W9, escalas S1 a S6, perfis de 3, 8 e 16 atributos,
aquecimento, repetições, ordem alternada, fórmulas de amplificação de escrita e pontos de
saturação do índice. `docs/spec-m3.md` e `docs/benchmark-plan.md` reescritos de acordo.
