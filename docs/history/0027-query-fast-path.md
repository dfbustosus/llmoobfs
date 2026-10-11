# 0027 — `QUERY`: busca a partir do primeiro candidato e resultados copiados um a um

Data: 2026-10-11. Branch: `m3/query-fast-path`. Estado: implementado e validado na VM; medição
pendente no host da Azure.

## Origem

O piloto 2 do M4 (registro 0025, `bench/results/2026-10-11-azure-pilot2-s1/`) mostrou o `QUERY`
de 2 a 9 vezes mais lento que o SQLite com índice composto, por forma, e a leitura de
`do_query` decompôs o custo em três partes: um custo fixo de cerca de 50 µs por chamada, uma
varredura linear das entradas do primeiro termo e cerca de 2 µs por resultado devolvido.

## Decisões

1. **Sem vetor do tamanho do limite.** `do_query` alocava com `kvcalloc` e zerava um vetor de
   `limit` resultados (até 4096 × 280 bytes, cerca de 1,1 MB) em toda chamada. Agora cada
   resultado autorizado é montado num único buffer e copiado direto para o vetor do chamador
   com `copy_to_user`, fora do lock do índice. A UAPI não muda: o vetor, `count` e `truncated`
   têm o mesmo significado. Em erro (`EFAULT`, `ENOMEM`) o conteúdo do vetor do chamador é
   indefinido, como já era por omissão de contrato.
2. **Busca a partir do primeiro candidato e parada no último.** As entradas de um atributo
   ficam ordenadas por (tipo, valor, inode). `seek_first` posiciona a varredura na primeira
   entrada que pode casar (igualdade, maior, maior ou igual e prefixo: primeira com valor não
   inferior ao termo; menor e menor ou igual: primeira do tipo do termo) e `past_end` encerra
   a varredura quando nenhuma entrada adiante pode casar (tipo maior que o do termo, valor além
   do limite, fim da sequência de strings com o prefixo). Existência continua percorrendo tudo.
   A retomada entre lotes de 256 candidatos usa a chave de retomada como antes.
3. **O custo por resultado fica.** Resolver o caminho com as credenciais do chamador e verificar
   a permissão é o contrato da fase 3b (registro 0020); nada muda nisso.

## Testes

Novo bloco em `vm/suite.sh`: um mesmo atributo (`mix`) com strings, int64, double, bool e time
em 14 arquivos, e 23 consultas cobrindo todos os operadores, inclusive limites, resultados
vazios e prefixos que esgotam a sequência, cada uma igual ao oráculo `moobfs-index`, mais
contagens exatas. Os testes existentes continuam cobrindo a retomada entre lotes (600
candidatos), o isolamento, a concorrência e as nove formas do W3 contra o SQLite e o oráculo.
`make vm-test` (Debian 6.12.111): `ISOLATION_PASS` e `BENCH_LAYER_PASS` em ext4, btrfs e XFS,
`MOOBFS_ACCEPTANCE_PASS`, `MOOBFS_LAYER_PASS`. KASAN: pelo workflow do pull request.

## Próximo passo

Repetir o piloto no host da Azure com esta versão e comparar por forma com o piloto 2.
