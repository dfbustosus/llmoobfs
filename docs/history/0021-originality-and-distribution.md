# 0021 — Auditoria de anterioridade e estratégia de distribuição

Data: 2026-10-10. Branch: `m3/3b-query-authz`. Estado: concluído (documentos de pesquisa e
proposta; nada implementado).

## Objetivo

Duas pesquisas pedidas antes de qualquer divulgação pública: (1) o que o MoobFS pode e não
pode reivindicar como original, com anterioridade identificada por leitura de código e artigos;
(2) como o módulo seria disponibilizado e operado por usuários reais e agentes de IA, sem
alterar o escopo do ADR-0007.

## Resultados

- [`docs/research-originality.md`](../research-originality.md): matriz de anterioridade (SFS 1991,
  BFS/Haiku, overlayfs/ecryptfs, TagFS/TMSU, LiFS, YoloFS, SMFS, VexFS, Factory VFS, Agent FS,
  RAGFS, duas patentes), comparação por característica, lista do que não é inédito, candidatos
  a diferenciador com classificação (comprovado / provável / não verificado), frases a evitar e
  posicionamento público proposto em português e inglês. Seis repositórios foram clonados e
  lidos; nada foi compilado nem executado.
- [`docs/distribution-strategy.md`](../distribution-strategy.md): arquitetura em três camadas
  (Core, Workspace, Integração), respostas às nove perguntas operacionais, CLI única `moobfs`
  (proposta), fluxo completo, modelo de permissões POSIX, empacotamento por DKMS com matriz de
  kernels, riscos e plano incremental (laboratório → preview técnico → versão pública). Cada item
  marcado como EXISTE, FUTURO ou DEPENDE DO M4.

## Decisões

1. A anterioridade mais próxima em forma é o YoloFS (módulo de kernel empilhado para agentes,
   ioctl na raiz, 2026); em função, o BFS. O MoobFS não reivindica "primeiro filesystem para
   memória de LLM" nem "consulta por atributo inédita". O diferenciador defensável é a
   combinação: índice de atributos tipados em kernel sobre FS maduro sem alterá-lo, `QUERY`
   filtrado pelas credenciais POSIX do chamador, e dados legíveis sem a camada. Classificação:
   "provável" até busca ampliada (artigo do YoloFS inteiro, `vfs.cpp` do Haiku, LKML, patentes).
2. Nenhuma afirmação de desempenho, segurança ampla ou prontidão antes do M4 e dos itens S1–S6 e
   S8–S10 do roteiro de segurança. A lista de frases a evitar passa a valer para README, site e
   qualquer texto público.
3. Patentes citadas (HP US7617250B2, Dell US20230401171A1) ficam descritas tecnicamente; não há
   parecer jurídico e uma avaliação especializada é exigida antes de qualquer release pública.
4. Distribuição: um mount por host por filesystem inferior, namespaces como diretórios; mounts
   por agente só quando o isolamento de memória do índice ou a administração justificar.
   Contêineres recebem bind mount do subdiretório com uids alinhados; `QUERY` exige a raiz do
   superbloco, portanto um subdiretório bind-montado não consulta (limitação registrada, quatro
   alternativas listadas, todas FUTURO ou contorno).
5. Instalação pública só por DKMS sobre kernels da matriz testada; qualquer outro kernel é
   "não suportado" até passar na suíte completa e no KASAN.
6. Nada da estratégia é implementado agora: sem servidor HTTP, sem MCP embutido, sem banco
   vetorial no kernel, sem persistência de índice antes do M4, sem alteração de ADR.

## Correções feitas na revisão

Os dois documentos foram escritos antes da última execução da suíte e diziam que os testes de
ACL e de idmap/userns estavam pendentes; corrigidos para refletir o registro 0020 (passaram na
VM em 2026-10-10). Pendências reais da fase 3b: KASAN sobre a branch (execução 38076335715
disparada) e `chmod`/`rename`/`unlink` concorrentes com consultas.

## Limitações

- Datas de entrada do overlayfs e ecryptfs no kernel e a licença do OpenBFS não foram
  reverificadas; artigo do YoloFS e LiFS lidos parcialmente.
- A receita de clone em 2.4 e o bind da raiz em leitura em 2.5 da estratégia não foram
  executados; `index_budget` contra contabilidade de cgroup não verificado.

## Próximo passo

Resultado do KASAN na branch; teste de `chmod`/`rename` concorrentes com `QUERY`; PR de
`m3/3b-query-authz` para `main`; depois o harness do M4 (`bench/`).
