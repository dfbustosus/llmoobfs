# ADR-0002 — Histórico técnico versionado

Data: 2026-10-10. Estado: aceito; solicitado pelo usuário.

## Decisão

Usar Git para o histórico exato de código/documentos, ADRs para decisões e registros cronológicos para pedidos, mudanças, experimentos e resultados. Registrar alternativas e justificativas em sínteses técnicas verificáveis.

## Consequências

Sessões futuras leem `AGENTS.md` e o histórico antes de trabalhar. Decisões superadas continuam disponíveis com referência à sucessora. Não versionar segredos ou binários reproduzíveis grandes. A proposta original é preservada.

O usuário está criando o repositório GitHub. A inicialização e commits locais podem avançar; a publicação remota aguardará o endereço e instrução do usuário. Um repositório remoto não substitui uma cópia de segurança independente.
