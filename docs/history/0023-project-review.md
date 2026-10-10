# 0023 — Revisão de implementação, planos e próximo experimento

Data: 2026-10-10. Estado: concluído como análise; correções e reproduções pendentes.
Base: `36eb64b`, branch `m3/3b-query-authz`.

## Objetivo e resultado

Revisar maturidade, direção arquitetural, código e evidências antes de ampliar o
escopo. Relatório: [revisão do projeto](../project-review-2026-10-10.md).

Mantida a ADR-0007 como direção. Recomendado fechar correção e autorização de QUERY,
medir um M4 reduzido e demonstrar memória entre sessões. São recomendações, sem nova
decisão de arquitetura ou autorização de implementação/publicação.

Achados por leitura: transição para RAW deixa entrada indexada anterior; caminho
truncado antes da autorização pode desaparecer da consulta; falhas de lookup são
convertidas em ausência; checker omite falhas de travessia. Riscos a reproduzir:
identidade entre subvolumes Btrfs e uso de inferior idmapped. Registradas limitações
de varredura, orçamento, metadados portáveis e divergências de status nos documentos.

Consultadas fontes primárias Btrfs, SQLite, OverlayFS e resumos de MemGPT,
LongMemEval e A-MEM. Separadas implicações propostas de resultados medidos.

## Validação e limites

`make test BUILD=build/review-20261010`: 60 checks de atributos e 9 testes Python
aprovados. Sem nova execução do módulo/VM nesta revisão. KASAN da branch, execução
38079499011, ainda em andamento na consulta. Resultados Linux anteriores identificados
como evidência do histórico, não testes reexecutados.

Somente documentação da revisão alterada. `docs/community-plan.md`, já não versionado
na chegada, preservado. Sem push, issues públicas ou alterações no site.

## Próximo passo

Transformar R1–R3 em regressões Linux e correções pequenas; reproduzir R4/R5 antes de
afirmar suporte a essas configurações. Atualizar status após a conclusão real do KASAN.
