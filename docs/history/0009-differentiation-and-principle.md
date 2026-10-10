# 0009 — O que o MoobFS traria de diferente, e o princípio de não concorrer com o maduro

Data: 2026-10-10, America/Sao_Paulo.

## Pedido

Responder o que o filesystem traria de benefício frente a outros; registrar e publicar. Em
seguida o usuário enunciou um princípio: bibliotecas e filesystems maduros, mantidos por
milhões de usuários, não devem ser concorrência mas base; a inovação possível está nos
recursos que uma memória de LLM usaria. Exemplo dado: "concorrer com o git é loucura".

## Análise registrada

**Sem concorrente direto em filesystems Linux:**

- Consulta por atributo tipado dentro do kernel (o BFS fez em 1997; nenhum FS Linux repetiu).
- Formato pequeno, auditável de ponta a ponta, com validação integral e entradas inválidas
  testadas uma a uma, desenhado para registros de memória.

**Igual ou pior que o existente, portanto não é diferencial:** dedup e reflink (btrfs, XFS,
ZFS), journaling (ext4, XFS), escala, desempenho e maturidade. Busca semântica fica fora por
decisão.

**Correção datada (2026-10-10):** a resposta inicial listou "trocar git por revisões nativas"
como hipótese de benefício. Isso contradiz o princípio acima e é retirado. Git permanece a
ferramenta de versionamento de conteúdo; revisões por memória no M3, se existirem, devem
complementar (identidade estável, atributos por revisão, consulta temporal), não substituir o
git, e isso precisa ser reavaliado na ADR do M3.

## Princípio adotado

Confirmado pelo usuário e aceito como regra de projeto: usar o que é maduro (git, ext4/btrfs/XFS,
jbd2, iomap, SQLite, xfstests), e concentrar a inovação nos recursos específicos de memória para
LLMs: atributos tipados consultáveis, verificação integral, identidade e proveniência de
memórias, consulta temporal.

## Implicação em aberto para a ADR-0006

A especificação do M2 (ADR-0006) descreve um volume completo: alocador de blocos, diretórios,
tabela de inodes, integração com jbd2. Pelo princípio acima, isso reconstrói o que ext4 e btrfs
já fazem. A alternativa coerente é um **filesystem empilhado nativo** (padrão de overlayfs e
ecryptfs, ambos in-tree, `=m` na VM; cerca de 12,5 mil e 8,2 mil linhas no v6.12) sobre um FS
maduro: blocos, journal, diretórios, reflink, dedup e checksums vêm do FS inferior (btrfs ou
XFS); a camada MoobFS adiciona só o que é específico de memória. Continua sendo módulo nativo
montável, sem FUSE (ADR-0001 preservada). A decisão de substituir a ADR-0006 por essa forma é
do usuário e fica registrada como pergunta pendente; nenhuma das duas foi implementada.

## Mudanças

Este registro, índice do histórico e `CHANGELOG.md`. Nenhum código.

## Próximo passo

Decidir entre "volume completo" (ADR-0006 como está) e "camada empilhada sobre FS maduro"
(nova ADR substituindo a 0006). Só então iniciar a fase 2a.
