# 0018 — Reescrita da história do Git: remoção de perguntas e de dados pessoais

Data: 2026-10-10, America/Sao_Paulo. Estado: executado localmente; publicação por force-push
feita pelo usuário (ver comandos abaixo).

## Decisão

Registrar perguntas e respostas no histórico foi uma decisão errada para um projeto aberto a
contribuidores; a regra passou a ser "decisões, erros e hipóteses descartadas". Como as
versões antigas dos registros continham essas perguntas e também um e-mail pessoal e o nome de
uma conta pessoal do GitHub, o usuário autorizou explicitamente a reescrita da história, única
exceção à regra "não reescrever história compartilhada" de `AGENTS.md`, registrada ali com
esta data.

## O que foi reescrito

- Conteúdo: em cada commit onde apareciam, os trechos de pergunta dos registros 0005, 0007,
  0010, 0011, 0015 e 0017 foram substituídos pela forma de decisão; a seção de atribuição de
  commits do registro 0012 perdeu o e-mail pessoal; a regra antiga do `AGENTS.md` foi
  substituída pela atual. Nada mais mudou em nenhum commit.
- Autoria: os 16 commits com o e-mail pessoal e o commit de merge com nome acentuado passaram
  a usar `Claudio <claudio@studiomoob.com>`, a identidade da conta `claudiofp`.
- Ferramenta: `git filter-repo` com `--mailmap` e um `--blob-callback`; execução em clone
  isolado; verificação por `git grep` em todos os commits de que não restou nenhuma ocorrência
  do e-mail, da conta ou dos trechos de pergunta.

## Mapa de hashes

Os hashes citados nos documentos foram atualizados para os novos valores neste mesmo commit.
Correspondência dos mais citados (antigo → novo): 653c5b9 → 9afc275, aa018cc → 607c23e,
e7f31c7 → 8ab6ecd, 2500d59 → 1a83bf8, 42e3901 → 0fd38a5, c48cc9f → 9a37b88,
ae16320 → b2fb037, 7b6abbe → b71ae05, e949355 → 1699791, 02e5e53 → c5db4b0. Os runs do CI
anteriores à reescrita continuam no GitHub vinculados aos hashes antigos; as evidências dos
registros não dependem deles, pois cada registro traz comandos e resumos de saída.

## Varredura do repositório

Feita antes da reescrita, em arquivos e em todos os blobs do histórico: sem segredos, tokens
ou chaves; sem caminhos locais da máquina; sem artefatos de build ou rascunhos versionados;
sem blobs acima de 400 KiB; o único dado pessoal era o e-mail e a conta citados acima.

## Publicação

Sequência executada pelo usuário (o assistente a preparou em `build/publish-rewrite.sh`):
remover temporariamente a proteção de `main`, force-push do clone reescrito, recriar a proteção
com a configuração salva em `build/protection-before.json`, e realinhar o checkout local.
