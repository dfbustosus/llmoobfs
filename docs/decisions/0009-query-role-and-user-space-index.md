# ADR-0009 — Papel do `QUERY` e índice estruturado em espaço de usuário

Data: 2026-10-11. Estado: **proposta**, sem implementação. Se aceita, substitui em parte a
ADR-0005 (item 3, índices mantidos pelo módulo, e a alternativa "SQLite ao lado" que ela
rejeitou) e redefine o `QUERY` dentro do item 2 da ADR-0007. A ADR-0001 (filesystem nativo
montável) continua valendo.

## Contexto

**Por que o `QUERY` existe.** A ADR-0005 tomou o Be File System como referência: atributos
tipados por arquivo, índices por atributo e consultas servidas pelo próprio filesystem. A
alternativa "metadados opacos indexados em espaço de usuário, SQLite ao lado" foi rejeitada
ali por "tornar o filesystem nativo um mero repositório de bytes", contra a ADR-0001. A ADR-0007
manteve o `QUERY` como parte do que a camada fornece e rejeitou "biblioteca ou daemon em espaço
de usuário com xattr e SQLite" por não ser filesystem montável e por tirar a propriedade de que
qualquer programa consulta memórias por interface do kernel. A escolha foi de desenho, inspirada
no BFS; nenhuma medição a sustentava, e a própria ADR-0007 condicionou o rumo ao M4.

**O que os pilotos e a série de escala mostraram** (registros 0025, 0027, 0028; série de escala
de 2026-10-11 no host da Azure):

- Consulta por uma condição seletiva: o `QUERY` empata ou ganha do SQLite com índice composto
  (sem resultados, 5 µs contra 19 µs).
- Custo por resultado: cerca de 1,7 µs contra 0,2 a 0,4 µs por linha no SQLite, por causa da
  resolução de caminho com as credenciais do chamador.
- Consultas com mais de uma condição: o `QUERY` só busca pela primeira condição e testa as
  demais em cada candidato, a cerca de 1 µs por candidato. O tempo cresce com o tamanho do
  conjunto: na forma `namespace = X AND agent = Y`, 34 µs com 1000 memórias, 1,1 ms com 10 000
  e 6,95 ms com 60 000, contra 15, 33 e 392 µs no SQLite (registro 0029), ou seja, de 2 a 18 vezes
  mais lento conforme o tamanho.
- Limite da UAPI v1: no máximo 4096 resultados por chamada, sem paginação; com 10 000 memórias
  uma das nove formas já é truncada, e com 40 000 ou mais, cinco das nove.
- Índice: 600 bytes contabilizados por memória com 8 atributos (36 MB com 60 000); reconstrução
  na montagem de cerca de 19 µs por memória.

Fechar essas diferenças dentro do kernel exigiria escolher a condição mais seletiva, cruzar
candidatos, manter estatísticas, índices compostos e paginação: um planejador de consultas. Isso
é reescrever no kernel o que o SQLite faz há duas décadas, contra o princípio do registro 0009,
e com custo maior que em espaço de usuário (risco de travar o host, superfície de ataque,
manutenção entre versões do kernel).

## O que só o kernel garante, segundo as medições e os testes

1. **Autorização por resultado com as permissões do Linux**, sem camada de autorização na
   aplicação nem processo privilegiado intermediário.
2. **Consistência sem gravação dupla**: o índice muda na mesma chamada que grava o atributo.
3. **Nada preso**: desmontado, o conteúdo continua sendo arquivos com atributos.

## Proposta

Dividir o papel em três partes, cada uma com o software mais maduro para ela:

- **Kernel (a camada):** atributos tipados validados (como hoje), o `QUERY` atual congelado no
  que já faz (até quatro termos, busca pela primeira condição, sem planejador), e duas peças
  novas, só se o M4 mostrar necessidade:
  - **fluxo de mudanças** para manter um índice externo em dia. Primeiro usar o que o Linux já
    tem: `fanotify` com `FAN_ATTRIB`, `FAN_CREATE`, `FAN_DELETE` e `FAN_MOVED_*`, reportando
    identificadores de arquivo (`FAN_REPORT_FID`). Um cursor próprio e durável (no estilo do
    journal de mudanças do NTFS) só entra se o `fanotify` não bastar, porque ele não sobrevive a
    reinício e pode perder eventos sob carga (`FAN_Q_OVERFLOW`, que exige nova varredura);
  - **autorização em lote**: uma chamada que recebe uma lista de identificadores de arquivo e
    devolve quais o chamador pode ler, com o mesmo contrato da fase 3b.
- **Espaço de usuário:** o índice estruturado, as consultas com várias condições, a paginação e
  o planejamento, num motor maduro (SQLite como primeira opção).
- **O cliente** consulta o índice e passa os resultados pela autorização em lote antes de usá-los.

## O ponto difícil: onde o índice mora e quem pode lê-lo

Um índice compartilhado em espaço de usuário contém metadados de todos os agentes (caminhos,
atributos). Se qualquer agente puder ler o arquivo do índice, a autorização por resultado deixa
de proteger os metadados, só o conteúdo. As saídas conhecidas são:

- **um processo indexador privilegiado** que mantém o índice e responde consultas em nome de
  cada chamador (identificando-o por `SO_PEERCRED` e autorizando pelo kernel). É um serviço, que
  o projeto evitou até aqui;
- **um índice por domínio de permissão** (por exemplo, por namespace), com as mesmas permissões
  do diretório que ele indexa. Sem serviço, mas sem consultas entre domínios;
- **manter o `QUERY` no kernel para o que precisa de isolamento** e usar o índice externo só
  dentro de um mesmo domínio.

A proposta não escolhe entre elas; o M4 mede as que forem viáveis.

Precedentes encontrados na pesquisa (`docs/research-metadata-search.md` e o adendo
`docs/research-metadata-search-addendum.md`):

- **GUFI** (Los Alamos, BSD-3, ativo): um banco SQLite por diretório, com dono e modo do diretório
  de origem; a ferramenta de consulta roda com o uid real do usuário num servidor dedicado (acesso
  por ssh a um shell restrito) e o kernel recusa abrir o que ele não pode ler, sem filtro
  privilegiado. Atributos que nem todo leitor pode ver vão para bancos por uid e por gid. Limites
  verificados: não replica ACLs, e a atualização é por varredura completa periódica (a cada 4
  horas no LANL), não por eventos. Em 2026 o LANL apresentou o mesmo modelo para recuperação de
  contexto de IA com controle de acesso ("Access Controlled AI Context with GUFI").
- **Windows Search:** um índice por máquina, num serviço do sistema, com arquivos de índice
  legíveis só pelo sistema e resultados cortados pela ACL de cada arquivo na consulta. É a forma
  "indexador privilegiado que responde em nome do chamador".
- **GNOME LocalSearch (Tracker):** um índice por usuário, num serviço do próprio usuário,
  alimentado por `fanotify` sem privilégio, com `inotify` como alternativa.
- **Parker-Wood et al. (MSST 2010):** particionar por mudança de dono ou de permissão gerou 29 479
  partições com média de 234 entradas num servidor de 6,9 milhões de arquivos: "um índice por
  domínio" pode significar muitos índices pequenos.

Aplicado aqui, a forma mais simples é um índice por namespace, com as permissões do diretório do
namespace, escrito por um indexador privilegiado alimentado pelo `fanotify` que nunca responde
consultas; cada agente lê só os índices que poderia ler. Pendências: ACLs (nem o GUFI resolve),
montagens com idmap, e a janela entre um `chmod` e o índice refletir a mudança.

## Alternativas

- **A. Manter como está.** O `QUERY` atual, sem crescer. Simples; perde em consultas com várias
  condições e com muitos resultados, e o limite de 4096 fica como restrição declarada.
- **B. Esta proposta.** Índice e planejamento em espaço de usuário; kernel com autorização e
  fluxo de mudanças.
- **C. Planejador no kernel.** Estatísticas, índices compostos, otimizador. Rejeitada: é um
  banco de dados no kernel.
- **C'. Interseção pequena no kernel, sem planejador.** As entradas já estão ordenadas por
  (tipo, valor, inode) e a busca por chave completa já existe (`first_after`). Duas ou mais
  condições de igualdade podem ser cruzadas avançando em paralelo pelas listas (leapfrog), sem
  alocação. Ataca exatamente o caso medido que cresce com o tamanho do conjunto; não muda o custo
  por resultado. É compatível com A e com B; precisa de especificação de ordem e retomada antes
  de código. O próprio BFS do Haiku não cruza listas: escolhe uma condição por uma pontuação
  barata (tamanho do índice) e testa as demais em cada candidato, como o MoobFS faz hoje com a
  primeira.
- **D. Portar o motor de consultas do BFS do Haiku** (OpenBFS, licença MIT). Rejeitada: C++ e API
  do kernel do Haiku, ligado ao formato em disco do BFS, e resolve o mesmo problema da
  alternativa C, só que com código alheio ao Linux.

## Motores maduros considerados para o índice em espaço de usuário

Nenhum motor de consulta maduro roda dentro do kernel Linux: módulos não podem usar bibliotecas
de espaço de usuário (libc, alocador, threads, ponto flutuante), e as estruturas do kernel
(rbtree, xarray, maple tree, rhashtable) são blocos de construção, não motores de consulta. A
prática da indústria para indexar metadados de arquivos é índice em espaço de usuário mais fluxo
de mudanças do kernel: Spotlight com FSEvents no macOS, Windows Search (ESE) com o journal USN
do NTFS, GNOME Tracker (SQLite) e KDE Baloo (LMDB) com inotify no Linux.

| Motor | O que oferece | Encaixe aqui |
| --- | --- | --- |
| SQLite | SQL, índices compostos, planejador, transações, testes extensos | Primeira opção: cobre consultas com várias condições sem escrever planejador |
| LMDB | Árvore B+ mapeada em memória, leitores sem lock, muito madura (OpenLDAP, Baloo) | Leitura mais rápida, mas é chave e valor: índices compostos e planejamento ficam por nossa conta |
| RocksDB, LevelDB | Árvores LSM, escrita intensa | Sem vantagem para leitura por atributo; mais operação |
| DuckDB | Consultas analíticas em colunas | Pensado para agregação, não para buscas pontuais de baixa latência |
| Xapian, Tantivy | Busca textual | Outro problema; fica com o cliente (ADR-0005, item 5) |

## O que a pesquisa acrescenta

- **Restringir antes de calcular.** Büttcher e Clarke (FAST 2005) mostram que filtrar por
  permissão depois de consultar vaza informação por contagens e pontuações. O `QUERY` não devolve
  contagem nem pontuação de quem o chamador não pode ler, mas o canal de tempo foi **medido**
  (registro 0030): cerca de 1,15 µs por candidato invisível, de 18 µs com nenhum a 28 ms com
  20 000, com dispersão abaixo de 3%. Um agente estima quantas memórias alheias casam com um
  predicado e testa a existência de um valor. Só uma consulta que não percorre entradas de
  outros domínios fecha o canal: é o argumento mais forte a favor do índice particionado por
  domínio, no kernel ou fora dele.
- **O fluxo de mudanças já existe** (registro 0030): `fanotify` com marca de filesystem no
  inferior e `inotify` em pastas da camada recebem todas as mudanças feitas pela camada. A
  camada recusa marcas `fanotify` com identificadores de arquivo por não ter operações de
  exportação.
- **Não existe cursor de mudanças durável no Linux.** As propostas de journal de mudanças
  discutidas no LSFMM de 2018 e 2019 não viraram código; do trabalho de Goldstein entrou só o
  `FAN_PRE_ACCESS` (6.14). O Lustre tem changelog durável, mas ler e limpar exige root. Antes de criar um, usar o `fanotify` e
  reconstruir o índice externo na montagem e em `FAN_Q_OVERFLOW`; um passo barato é expor no
  `GET_INFO` uma época de montagem e um contador monotônico de mudanças, para o consumidor saber
  que perdeu continuidade (exige especificação da UAPI).
- **O Landlock não é consultado** (reproduzido, registro 0030): sob Landlock o `QUERY` devolve
  arquivos que o processo não pode abrir, mas só nomes e valores de atributos que o processo já
  obteria listando pastas e lendo atributos, porque o Landlock não controla leitura de xattr.
- **`setxattr` gera `FS_ATTRIB`** (`fsnotify_xattr` no 6.12), confirmado na camada e no inferior
  (registro 0030).

## O que o M4 precisa medir para decidir

- B com SQLite contra A nas nove formas do W3, por escala, incluindo o custo da autorização em
  lote e o atraso entre gravar um atributo e o índice externo refletir a mudança.
- Se o `fanotify` entrega todos os eventos de atributos feitos pela camada (a confirmar: as
  chamadas passam pelo VFS da camada e do inferior) e o custo de reconstruir o índice após
  `FAN_Q_OVERFLOW`.
- Qual das três formas de guardar o índice é viável sem enfraquecer o isolamento.

## Consequências se aceita

- O `QUERY` atual deixa de crescer; melhorias só de eficiência, sem planejador.
- A ADR-0008 (reaproveitar a autorização por diretório) passa a valer também para a autorização
  em lote.
- A spec do M3 (§5, cursor de mudanças) é revista: o fluxo de mudanças deixa de ser "adiado sem
  demanda" e passa a ser parte necessária da arquitetura, começando pelo `fanotify`.
