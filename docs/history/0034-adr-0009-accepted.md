# 0034 — ADR-0009 aceita: `QUERY` por domínio no kernel (alternativa E)

Data: 2026-10-11. Estado: decisão registrada; especificação em proposta; nada implementado.

## Decisão

O mantenedor aceitou a alternativa E da ADR-0009 com os ajustes recomendados: índice no kernel
particionado por domínio de permissão (namespace homogêneo), `QUERY` emitido sobre o diretório
do domínio numa nova versão da UAPI, ordenação por atributo de tempo com limite ("os N mais
recentes"), interseção pequena de condições de igualdade, consulta em lote para o "ou" entre
valores e o operador "ausente ou comparação". A alternativa B (índice SQLite em espaço de usuário)
fica rejeitada como direção do filesystem e permitida como camada opcional de um cliente.

## Justificativa resumida

Medições dos registros 0028 a 0031 e as pesquisas do registro 0033: a v1 tem custo por resultado
(resolução de caminho), custo por candidato que cresce com o tamanho do conjunto e um canal de
tempo entre donos; os clientes reais isolam por escopo e ordenam por tempo com limite pequeno; a
literatura mostra que só uma busca que não percorre outros domínios fecha o canal. A E faz isso
sem motor de banco, sem processo extra, mantendo o índice atualizado na mesma chamada e as ACLs
respeitadas pelo inferior.

## Próximo passo

`docs/spec-query-v2.md` traz quatro pontos marcados como decisão para o mantenedor confirmar
antes do código: domínios por profundidade (`domain_depth`), ACLs estendidas fora da partição
nesta versão, v1 restrita a `CAP_SYS_ADMIN`, e `EXDEV` para diretórios renomeados entre domínios.
Depois, as fases Q2a a Q2e, cada uma com pull request, suíte, KASAN e medição na Azure.

Atualização no mesmo dia: o mantenedor aceitou os quatro pontos nas formas recomendadas
(`domain_depth`, ACLs estendidas fora da partição, v1 só com `CAP_SYS_ADMIN`, `EXDEV` para
diretórios entre domínios) e autorizou o início da fase Q2a.
