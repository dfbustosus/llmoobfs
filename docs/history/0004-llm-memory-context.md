# 0004 — Memória e contexto de LLMs como objetivo explícito

Data: 2026-10-10, America/Sao_Paulo.

## Pedido

O usuário observou que a apresentação enfatizava deduplicação e compartilhamento,
sem deixar suficientemente claro o foco em memória/contexto para LLMs. Pediu
ajustar README e site, distinguir objetivo de implementação, registrar e publicar.

## Decisão editorial e justificativa

Apresentar MoobFS primeiro como filesystem nativo experimental voltado à memória
persistente e ao contexto consumidos por agentes baseados em LLMs. O compartilhamento
de bytes continua como fundamento técnico demonstrado pelo M1. Manter a explicação
anterior como abertura preservaria a ambiguidade de um filesystem genérico; por isso
a intenção do projeto agora aparece antes do mecanismo.

Explicitar responsabilidades: o filesystem armazena e expõe arquivos; a aplicação
do agente seleciona material e monta o contexto. Recuperação semântica e integração
com agentes continuam como trabalho futuro. Nenhuma mudança no formato, na arquitetura
adotada ou no código nativo; ADRs 0003 e 0004 continuam válidas. Não são prometidos
recursos novos, ampliação da janela de contexto nem ganhos de desempenho.

## Mudanças

- `README.md` e `README.pt-BR.md`: objetivo na abertura e seção separada para M1.
- `site/index.html`: título, metadados e hero voltados a memória/contexto de LLMs;
  objetivo, responsabilidades e status implementado separados explicitamente.
- `site/guide.html`: situa a reprodução como teste da base de armazenamento,
  sem confundi-la com demonstração de consumo de memórias por uma LLM.
- `PRODUCT.md`: mantém esse posicionamento nas instruções de continuidade do site.
- `CHANGELOG.md` e índice do histórico atualizados.

## Validação e publicação

`python3 scripts/check-site.py` passou para as duas páginas; `git diff --check`
passou. Prévia inspecionada no navegador em desktop e tela estreita: título legível,
navegação para o guia funcionando e sem overflow horizontal medido nas duas páginas.
O código nativo não mudou; testes locais de kernel não foram repetidos para esta
alteração editorial. O CI obrigatório do PR e o workflow Pages registram suas
execuções no GitHub, vinculadas aos commits publicados.

A publicação foi explicitamente autorizada no pedido. A entrega usa PR para `main`
e o workflow existente de GitHub Pages; o histórico anterior permanece preservado.
