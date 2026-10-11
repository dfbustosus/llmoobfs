# 0027 — `QUERY`: busca a partir do primeiro candidato e resultados copiados um a um

Data: 2026-10-11. Branch: `m3/query-fast-path`. Estado: implementado, validado na VM e medido no
host da Azure (piloto 3).

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

Mesclar após o KASAN do pull request; depois, a campanha completa em discos sem bursting.

## Medição no host da Azure: piloto 3 (2026-10-11)

Mesmo host, kernel, discos, semente, comando e harness do piloto 2; o módulo foi compilado do
commit `b85d041` (harness do PR 20 com esta branch). Evidências em
`bench/results/2026-10-11-azure-pilot3-s1/`; pacote bruto na pre-release
`bench-2026-10-11-azure-pilot3-s1`. As 30 equivalências passaram (kernel, SQLite e oráculo).

W3 por forma, mediana, ext4 (btrfs e XFS seguem o mesmo padrão: Q8 4,9 µs, Q3 34 µs, Q7 de
1052 a 1100 µs):

| Forma | Resultados | SQLite | `QUERY` piloto 2 | `QUERY` piloto 3 | Usuário comum, piloto 3 |
| --- | --- | --- | --- | --- | --- |
| Q8 | 0 | 19 µs | 54 µs | 4,9 µs | 4,8 µs |
| Q3 | 3 | 15 µs | 141 µs | 34 µs | 34 µs |
| Q4 | 23 | 26 µs | 178 µs | 73 µs | 74 µs |
| Q5 | 70 | 43 µs | 248 µs | 144 µs | 150 µs |
| Q2 | 100 | 56 µs | 323 µs | 217 µs | 224 µs |
| Q6 | 212 | 99 µs | 502 µs | 403 µs | 412 µs |
| Q9 | 291 | 78 µs | 608 µs | 544 µs | 561 µs |
| Q1 | 390 | 91 µs | 843 µs | 744 µs | 770 µs |
| Q7 | 513 | 224 µs | 1136 µs | 1049 µs | 1086 µs |

Leitura: o custo fixo e a varredura linear desapareceram. Sem resultados, o `QUERY` responde em
menos tempo que o SQLite do baseline (que prepara a instrução a cada chamada). O restante é
quase linear no número de resultados, com cerca de 2 µs por resultado contra 0,2 a 0,4 µs do
SQLite, e corresponde a montar o caminho, resolvê-lo com as credenciais do chamador, verificar a
permissão e copiar 280 bytes para o usuário. Consultas seletivas ficam de 2 a 3 vezes mais
lentas que o SQLite; consultas que devolvem centenas de resultados, de 4 a 8 vezes. W1, W2 e W4
não mudaram em relação ao piloto 2 (W1 9,84 ms com camada contra 9,86 ms sem; W2 12,9 contra
9,3 µs; W4 24,5 contra 10,1 µs).

Candidatos para reduzir o custo por resultado, ainda sem decisão: autorizar cada diretório
uma vez por consulta em vez de resolver o caminho completo por resultado (muitos resultados
compartilham diretórios), e evitar a cópia do caminho com `kstrdup` sob o lock. Qualquer um
mexe no contrato de autorização da fase 3b e exige decisão própria, testes de isolamento e
KASAN antes de entrar.
