# 0031 — Tempo de remontagem com 25 000 a 60 000 memórias

Data: 2026-10-11. Estado: medido (nível de piloto).

## Objetivo

Completar o W7 que ficou sem valor nas escalas grandes da série do registro 0029 e medir também
o W6 (cache frio), para informar a decisão, deixada ao M4 pela ADR-0007, de persistir ou não o
índice em disco.

## Resultado (host da Azure, ext4 em P6, evidências em `bench/results/2026-10-11-azure-remount/`)

| Memórias | W7, cache quente (mediana de 5) | W6, cache frio (mediana de 3) | Índice contabilizado |
| --- | --- | --- | --- |
| 25 000 | 0,47 s | 49 s | 14,9 MB |
| 40 000 | 0,79 s | 75 s | 23,9 MB |
| 60 000 | 1,18 s | 126 s | 35,8 MB |

Com cache quente, reconstruir o índice custa cerca de 19,7 µs por memória, linear, como nas
escalas menores (registro 0029). Com cache frio, cerca de 2 ms por memória: a varredura lê o
inode e os atributos de cada arquivo com leituras aleatórias, e o disco P6, provavelmente já sem
créditos de burst depois da série, entrega poucas centenas por segundo. O W6 depende do disco e
é um limite superior para este disco, não uma propriedade só da camada.

## Leitura

- Montar depois de reiniciar a máquina, com 60 000 memórias, levou mais de dois minutos neste
  disco, e o tempo cresce linearmente com o número de memórias. Até lá, nenhuma consulta é
  respondida.
- É o primeiro número concreto para a pergunta deixada aberta pela ADR-0007 sobre persistir o
  índice: com índice persistido, a montagem fria leria um arquivo sequencial de cerca de 36 MB
  em vez de 60 000 leituras aleatórias. O custo seria manter esse arquivo coerente com o
  inferior (inclusive depois de mudanças feitas fora da montagem e de quedas), o que precisa de
  decisão própria.
- Na alternativa E da ADR-0009 (índice particionado por domínio), a reconstrução poderia ser
  feita por domínio e sob demanda, na primeira consulta a cada domínio, o que reduz o tempo até
  a primeira resposta sem persistir nada; vale medir antes de persistir.

## Limitações

Um host, um disco com bursting em estado desconhecido, três medições frias por escala. Repetir
com disco de IOPS fixos antes de qualquer conclusão sobre persistência.
