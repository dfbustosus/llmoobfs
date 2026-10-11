# Levantamento sistemático da literatura acadêmica para a ADR-0009

## Abstract (English)

Systematic literature survey supporting ADR-0009 (role of the in-kernel `QUERY` and the choice between
alternative E, an in-kernel index partitioned by permission domain with `QUERY` issued on a directory,
and alternative B, a per-domain user-space SQLite index fed by fanotify). It extends
`docs/research-metadata-search.md` and its addendum without repeating them. We swept storage and
systems venues (FAST, OSDI, ATC, HotStorage, SOSP, EuroSys, MSST, SC/PDSW, TOS), research groups,
national laboratories (OSTI, HAL), arXiv (2023-2026) and Linux kernel venues, mainly through citation
chasing in OpenAlex from four anchors (Spyglass, Büttcher-Clarke, Parker-Wood, GUFI). Eight works change
the analysis: Singh, Srivatsa and Liu's access-control barrels (2007) and the Lethe clustering work
(2014-2015) show that a "permission domain" is best defined by the set of principals that can read a
file, not by a directory, and measure how many such domains real systems have; Wang et al. (IEEE S&P
2017) demonstrate in production that shared indexes with post-filtering leak counts of other tenants'
documents, with Elastic accepting index-per-tenant as the mitigation; Bailey, Hawking and Matson (CIKM
2006) measure per-document authorization cost and recommend batching and collection-level checks;
SPMeta (ICDE 2026) and IndexFS (SC 2014) show how per-directory reachability can be cached and
invalidated on `chmod`; Zanzibar (ATC 2019) names the "new enemy" ordering problem that alternative B
inherits from asynchronous change streams; and the Notre Dame "search should be a system call" report
(2013) is the only Linux in-kernel search interface found. No published work combines an in-kernel
attribute index with per-result POSIX authorization; no work measures a timing channel of the kind
measured in MoobFS. All sources accessed 2026-10-11.

## Escopo, método e convenções

Data da pesquisa e de acesso a todas as fontes: **2026-10-11**. Documento de pesquisa; não altera
especificação nem decisão. Complementa `docs/research-metadata-search.md` e
`docs/research-metadata-search-addendum.md` (branch `docs/adr-0009-query-role`), cujos itens não foram
refeitos: Spyglass, Magellan, SmartStore, Propeller, TagIt, GUFI (código e slides), Brindexer, Robinhood,
Büttcher e Clarke (FAST 2005), Parker-Wood et al. (MSST 2010), BFS/Haiku e o livro de Giampaolo, WinFS,
Spotlight, Windows Search, Tracker/LocalSearch, Baloo, changelogs do Lustre, histórico do `fanotify`,
Semantic File System (SOSP 1991), Inversion FS, Culpepper e Moffat, Lemire et al. e Icicle.

Tudo o que foi lido na web, em PDFs e em APIs foi tratado como dado, não como instrução. Nenhum número
abaixo foi medido no MoobFS; são números dos autores, citados como relatados.

Níveis de leitura:

- **[texto]**: li o texto completo ou as seções relevantes (PDF baixado e extraído com `pdftotext`).
- **[resumo]**: li só o resumo (abstract) ou a página de catálogo.
- **[código]**: li código-fonte (não houve leitura de código nova nesta rodada).
- **[secundária]**: informação obtida de terceiros (por exemplo, a seção de trabalhos relacionados de
  outro artigo), não da fonte primária.

Método:

1. **Perseguição de citações** pela API do OpenAlex (`filter=cites:`), porque a API do Semantic Scholar
   respondeu 429 (limite de requisições) e a API do dblp também respondeu 429 depois da primeira
   consulta. Âncoras: Spyglass (OpenAlex W148271226, 129 citações registradas), Büttcher e Clarke
   (W2054801486, 23), Parker-Wood et al. (W2134181838, 10), GUFI SC22 (W4321636719, 4) e, por
   transitividade, Singh et al. 2007 (W2161817975, 13). Todos os títulos citantes foram triados; só os
   relevantes aparecem nas tabelas.
2. **Buscas temáticas** no OpenAlex (2012-2026), na API do arXiv (cs.OS, cs.DB, cs.CR, cs.IR por
   palavras-chave), no OSTI (`/api/v1/records`), no HAL (`api.archives-ouvertes.fr`) e na web.
3. **Leitura em profundidade** dos trabalhos que pareceram mudar a análise (marcados [texto]).

Google Scholar não foi consultado diretamente (sem API); a perseguição de citações pelo OpenAlex cumpre
o mesmo papel com cobertura menor, o que está registrado em "Ainda não verificado".

## Resumo executivo

- **"Domínio de permissão" tem definição e medida na literatura, e não é "diretório".** Singh, Srivatsa e
  Liu (2007) particionam por *expressão de controle de acesso* (o conjunto de credenciais que pode ler o
  arquivo): 2132 partições para 339 466 arquivos, 926 usuários e 1203 grupos, média de 6,31 partições
  por usuário. Lethe (2014-2015) encontra 1475 conjuntos de permissão distintos em 50 000 documentos.
  A alternativa E, como escrita (partição = diretório de primeiro nível), só fecha o canal de tempo se
  todo arquivo da partição tiver o mesmo conjunto de leitores; senão o canal reaparece dentro dela.
- **O canal lateral é conhecido, explorado e aceito como inerente ao filtro posterior.** Wang et al.
  (S&P 2017) extraíram termos e contagens de documentos alheios no GitHub e em outros serviços sobre
  Elasticsearch; a Elastic respondeu que índice por inquilino resolve, mas pode ser caro; a documentação
  atual do Elasticsearch avisa que o usuário pode "contar" documentos inacessíveis que contêm um termo.
  Nenhum trabalho encontrado trata canal de **tempo** em índice de metadados de filesystem; o mais
  próximo é o ataque de busca entre sites por tempo de resposta (Gelernter e Herzberg, CCS 2015).
- **Autorizar por coleção, em lote, e cachear alcance por diretório** são as técnicas medidas para baixar
  o custo por resultado: Bailey et al. (CIKM 2006, 0,11 ms por documento local, 6,4 ms por consulta LDAP,
  recomendação de lote), IndexFS (concessões temporárias de permissão por componente de caminho) e SPMeta
  (ICDE 2026, bits de alcance por diretório e classe u/g/o, 99,7% dos caminhos reais elegíveis).
- **Índices externos herdam o problema de ordem entre mudança de ACL e consulta** ("new enemy",
  Zanzibar, ATC 2019). A alternativa B precisa de um marcador de consistência, não só de reconstrução
  após `FAN_Q_OVERFLOW`.
- **Bancos vetoriais refizeram em 2024-2026 a mesma troca** (índice por papel contra índice comum com
  filtro): HoneyBee mede 13,5x menos latência que row-level security do PostgreSQL com 1,24x de memória.
- **Nenhum trabalho publicado põe um índice de atributos no kernel Linux com autorização POSIX por
  resultado.** O único precedente de interface de busca no kernel Linux é um relatório técnico de Notre
  Dame (2013), sem índice. A combinação do MoobFS continua sem precedente direto.

## 1. Conferências e periódicos

Varredura por citação a partir das âncoras e por buscas temáticas. A maioria dos trabalhos de FAST,
SC, MSST e TPDS que citam Spyglass trata de escala e organização semântica, não de permissão.

| Trabalho | Mecanismo | Permissões | Resultados relatados | Implicação para E ou B | Nível e URL |
| --- | --- | --- | --- | --- | --- |
| Singh, Srivatsa, Liu. "Efficient and Secure Search of Enterprise File Systems", IEEE ICWS 2007 (versão longa GIT-CERCS-07-07) | Ver 1.1 | Partição por expressão de credenciais; sem filtro em tempo de consulta | Ver 1.1 | Define domínio por conjunto de leitores | [texto] https://cercs.gatech.edu/www.cercs.gatech.edu/tech-reports/tr2007/git-cercs-07-07.pdf ; DOI 10.1109/ICWS.2007.80 |
| Bailey, Hawking, Matson. "Secure Search in Enterprise Webs: Tradeoffs in Efficient Implementation for Document Level Security", ACM CIKM 2006 | Ver 1.2 | Checagem por documento no momento da consulta | Ver 1.2 | Lote e checagem por coleção | [texto] https://david-hawking.net/pubs/cikm127_bailey.pdf |
| Johnson, Keeton, Morrey, Soules et al. (HP). "From research to practice: experiences engineering a production metadata database for a scale out file system", USENIX FAST 2014 | StoreAll com Express Query: banco de metadados baseado no LazyBase (pipeline) ao lado de um FS distribuído; API REST de busca e marcação | Não consta no resumo | Relato de experiência; sem números no resumo | Precedente industrial de "banco ao lado do FS" com consistência por pipeline (próximo de B) | [resumo] https://dl.acm.org/doi/10.5555/2591305.2591324 |
| Ren, Gibson. TableFS, USENIX ATC 2013 | FS empilhado sobre FS local; todos os metadados numa tabela LSM (LevelDB); versão FUSE | Herda do FS (não é busca) | 50% a 1000% sobre ext4/XFS/btrfs em cargas de metadados (resumo) | Precedente de camada empilhada que reorganiza metadados; não trata consulta nem autorização | [resumo] OpenAlex "TABLEFS" |
| Ren, Zheng, Patil, Gibson. IndexFS, SC 2014 | Particionamento incremental por diretório; cache de componentes de caminho e suas permissões no cliente com concessões (leases) curtas | Mutação de permissão espera as concessões expirarem | Até duas ordens de grandeza sobre PVFS/Lustre/HDFS em metadados (resumo) | Mecanismo de cache de travessia autorizada aplicável a E ("autorizar uma vez até o diretório") | [texto parcial] https://www.pdl.cmu.edu/PDL-FTP/FS/IndexFS-SC14.pdf |
| Zheng et al. DeltaFS Indexed Massive Directories, ACM TOS 2020 | Reorganização em fluxo da saída de simulações, indexada | Não trata | Leituras 1740x mais rápidas (resumo) | Sem relação com permissão; descartado | [resumo] https://doi.org/10.1145/3415581 |
| Naps, Mokbel, Du. "Toward efficient search for ultrascale storage systems", PDSW 2011; Pantheon, 2011 | Propõe mover componentes de SGBD para dentro do sistema de armazenamento | Não consta | Posição, sem números no resumo | Argumento a favor de E (índice dentro), sem avaliação | [resumo] https://doi.org/10.1145/2125636.2125638 |
| Parker-Wood et al. "Examining extended and scientific metadata for scalable index designs", SYSTOR 2013 | Caracteriza metadados científicos; sugere armazenamento em colunas | Não trata | Qualitativo (resumo) | Pouca relação | [resumo] https://escholarship.org/uc/item/30b0g251 |
| Ohara. HCTrie, IEEE MSST 2013 | Trie de hipercubo para centenas de dimensões | Não trata | 5 milhões de entradas com 100 atributos; supera MySQL em faixas com menos de 100 dimensões (resumo) | Estrutura multiatributo; não atende o caso de igualdade conjuntiva do MoobFS melhor que interseção de listas | [resumo] https://doi.org/10.1109/MSST.2013.6558431 |
| Frank et al. Percival, IEEE MSST 2015 | Busca sobre dados com divisão de segredo, via hash salgado em HSM | Custódios cegos para consulta e resultado | Menos de 1 s por busca em ~1 milhão de arquivos (resumo) | Outro modelo de ameaça (servidor não confiável); descartado | [resumo] https://doi.org/10.1109/MSST.2015.7208296 |
| Ismail, Ronström, Haridi, Dowling. ePipe, IEEE/ACM CCGrid 2019 | Barramento que produz fluxo de mudanças consistente e ordenado do HopsFS para outros motores (Elasticsearch) | Não consta no resumo | Até 56x de vazão sobre HDFS INotify; atraso médio de 330 ms até o Elasticsearch na carga do Spotify (resumo) | Mostra o que B exige do fluxo de mudanças: ordem e entrega garantidas, que o `fanotify` não dá | [resumo] https://doi.org/10.1109/CCGRID.2019.00020 |
| Koyama, Tatebe. "Lustre Query", ACM HPDC 2026 | Monitoramento offline a partir de backups do MDT | Ferramenta de administrador | Não verificado | Descartado | [resumo] https://doi.org/10.1145/3806645.3816126 |
| Bjornson. "A Comparison of Three Open-source File Indexers: Robinhood, GUFI, XDU", 2026 | Comparação com Starfish, `find` e `du` | Não consta no resumo | Texto bloqueado ao download | Possível fonte de números independentes do GUFI | [resumo] https://doi.org/10.1145/3785462.3815846 |

### 1.1 Singh, Srivatsa e Liu: barris de controle de acesso (2007)

**Mecanismo.** [texto] A "pesquisabilidade" é definida como permissão de leitura **e** execução em todo
o caminho e leitura no arquivo; diretórios só com execução contam como não pesquisáveis (mais restritivo
que Büttcher e Clarke, que exigem só execução). Os arquivos são agrupados em *access-control barrels*
(ACB): cada barril contém exatamente os arquivos com a mesma expressão de credenciais (por exemplo,
"uid bob" ou "grupo students"), e cada arquivo pertence a um só barril. Um grafo de hierarquia de acesso
liga cada usuário aos barris que ele alcança; a consulta é feita em todos esses barris e os resultados
são combinados com algoritmos de busca distribuída. Não há filtro por permissão na consulta. Duas
otimizações reduzem o número de barris: duplicar índices de arquivos em barris dominantes, ou fundir
barris aceitando violação controlada da propriedade de não vazamento.

**Permissões e vazamento.** O texto mostra o ataque de contagem sobre o índice único com filtro
(contagem de arquivos com "bankruptcy") e prova que não dá para ter menos barris sem duplicar índices ou
violar a propriedade. ACLs POSIX entram como expressões de credenciais. O limite teórico de barris por
usuário é exponencial no número de grupos do usuário.

**Resultados.** Sistema real com 926 usuários, 1203 grupos, 339 466 arquivos, 23 741 diretórios:
2132 barris; barris por usuário com máximo 25, média 6,31, mediana 4,26 (segunda empresa: 21, 5,78,
3,96). Pré-processamento 38,7 s (hierarquia) e 263,1 s (barris), percorrendo só 60% da árvore porque
subárvores com credencial restritiva são atribuídas inteiras. Indexação 38% mais rápida que o índice
central (2902 s contra 4640 s), dominada pelo barril "todos". Consulta média: 131,12 ms (central) contra
112,89, 130,68 e 149,90 ms combinando 2, 5 e 10 barris.

**Implicação.** Para E, a partição natural não é o diretório de primeiro nível, mas o conjunto de
leitores; o diretório só serve se o projeto garantir (por política de criação) que tudo dentro dele tem
o mesmo conjunto de leitores. Para B, um banco SQLite por barril em vez de por namespace é o desenho
com prova de minimalidade; o número de bancos que um agente abre (média 6 neste dado) é o custo.

### 1.2 Bailey, Hawking e Matson: custo da segurança por documento (CIKM 2006)

**Mecanismo.** [texto] Modelo de busca com segurança por documento (DLS) em duas implementações (Sytadel, um
sistema de gestão de conteúdo, e o buscador Panoptic do CSIRO, depois Funnelback): candidatos percorridos linearmente, cada um checado contra o sistema de origem.
**Resultados.** Checagem local custou cerca de 0,11 ms por documento; uma consulta LDAP a outro servidor,
6,4 ms. Com orçamento de 1 s e 0,25 s de sobrecarga, o modelo admite 5454 checagens locais ou 93
remotas. Contar o total exato de resultados visíveis é o que torna o custo inaceitável; parar ao
completar uma página (20 resultados) estabiliza a consulta em 75 a 90 ms. **Recomendações dos autores:**
organizar documentos para que a maior parte da checagem seja por coleção, abrir mão de contagens exatas,
cachear e **checar em lote** (uma requisição com 100 a 1000 URIs).

**Implicação.** O custo medido no MoobFS (cerca de 1,7 µs por resultado) já é duas ordens abaixo da
checagem local de 2006, mas a forma do problema é a mesma: a alternativa E é exatamente "checagem por
coleção" (autorizar a travessia até o diretório uma vez), e a autorização em lote de B é a recomendação
de lote. O trabalho também confirma que não expor contagem total é escolha de desempenho, além de
segurança.

## 2. Grupos de pesquisa

| Grupo | O que foi encontrado | Relevância |
| --- | --- | --- |
| UC Santa Cruz SSRC/CRSS | Parker-Wood, dissertação "Improving File Management Through Provenance And Rich Metadata" (2014), cujo resumo cita "secure, effective, and efficient search" [resumo] https://escholarship.org/uc/item/6pc1t0nw ; LiFS (Ames et al., MSST 2005) [resumo]; Percival (MSST 2015) [resumo]; SYSTOR 2013 (tabela 1) | A dissertação provavelmente estende o MSST 2010; o PDF respondeu 403 e não foi lido |
| CMU Parallel Data Lab | TableFS, IndexFS, DeltaFS (tabela 1); Patil et al. "In Search of an API for Scalable File Systems: Under the Table or Above it?" (registro no repositório da CMU; local e ano originais não verificados) [resumo]; Soules e Ganger, atribuição de atributos por contexto (CMU-PDL-04-105) [resumo] | Só o cache de permissões com concessões do IndexFS é relevante |
| UW-Madison ADSL | Nada sobre indexação ou busca de metadados com permissões nas buscas feitas | Sem resultado |
| MIT PDOS e sucessores do SFS | Semantic File System (já coberto); HAC, Gopal e Manber (OSDI 1999, Arizona) [resumo]; Logic File System, Padioleau e Ridoux (IRISA/INRIA, 2002-2005) [resumo]; hFAD, Seltzer e Murphy (HotOS 2009, Harvard) [resumo] | Nenhum resumo trata permissões por resultado |
| Stony Brook FSL | VT-trees (FAST 2013) e trabalhos de atualização de metadados citam Spyglass, mas tratam de estruturas de armazenamento | Sem resultado |
| Microsoft Research | Bhatt et al. "Enterprise AI Must Enforce Participant-Aware Access Control" (arXiv 2509.14608, 2025; MSR Índia) [resumo] | Ver seção 4 |
| Google | Goods (SIGMOD 2016) [texto]; Zanzibar (USENIX ATC 2019) [texto] | Ver 2.1 e 2.2 |

### 2.1 Goods (Halevy et al., SIGMOD 2016)

[texto] https://storage.googleapis.com/gweb-research2023-media/pubtools/pdf/45390.pdf (DOI
10.1145/2882903.2903730). Catálogo de metadados de conjuntos de dados da Google (armazenamento chave-valor
mais índice invertido). **Permissões:** o texto diz que o catálogo indexa mais de 26 bilhões de conjuntos
"embora inclua apenas" os que são legíveis por todos os engenheiros, e que o tamanho deve mais que dobrar
quando os de acesso restrito forem indexados. **Implicação:** o maior catálogo de metadados publicado
evitou o problema indexando só o domínio público. Para E e B, um domínio "compartilhado por todos" pode
ter índice único sem risco; o problema real fica nos domínios privados.

### 2.2 Zanzibar (Pang et al., USENIX ATC 2019)

[texto] https://www.usenix.org/system/files/atc19-pang.pdf. Sistema global de ACLs da Google (Drive,
Calendar, YouTube e outros), sobre Spanner. **Mecanismo relevante:** (1) o problema do **"new enemy"**:
avaliar uma ACL fora de ordem em relação a mudanças de ACL ou de conteúdo, por exemplo aplicar a ACL
antiga a conteúdo novo; resolvido com *zookies*, fichas que carregam um carimbo de tempo do conteúdo e
obrigam a checagem a usar um instantâneo pelo menos tão novo; (2) **Leopard**, índice que achata grupos
aninhados em conjuntos para checagens rápidas, alimentado por um fluxo ordenado de mudanças (Watch);
(3) a API Expand existe para que clientes construam **índices de busca com controle de acesso**, e o
texto diz que servir resultados de busca exige dezenas a centenas de checagens, com pontos quentes em
grupos comuns. **Resultados:** percentil 95 abaixo de 10 ms e disponibilidade acima de 99,999% em três
anos (resumo do artigo). **Implicação:** B herda o "new enemy": entre um `chmod` e o índice externo
refletir a mudança, o índice pode devolver nomes e atributos ao leitor antigo. A autorização em lote no
kernel resolve o conteúdo, não os metadados copiados no índice. O "contador monotônico de mudanças" já
proposto na ADR-0009 para o `GET_INFO` é o equivalente local de um zookie; a literatura sugere que ele
seja exigido na consulta, não só exposto. E não tem esse problema (índice e permissão mudam na mesma
chamada).

## 3. Laboratórios nacionais e institutos

| Fonte | O que foi encontrado | Nível e URL | Implicação |
| --- | --- | --- | --- |
| LANL, artigo GUFI SC22 | Continua sem cópia aberta: a página do SC22 e o LARO só têm resumo; o resumo da página do USRC/LANL diz que os bancos são "sharded" de modo a impor permissões POSIX e que o "rollup" sobe informação de subárvores com mesmas permissões | [resumo] https://usrc.lanl.gov/emc3-project-gufi.php | Nada novo além do adendo |
| LANL, patente "Grand unified file indexing", US 11 048 699 (2021; Grider, Manno, Poole, Bonnie, Inman) | Árvore que replica a hierarquia, um armazenamento de metadados por diretório, "controles de acesso eficientes em espaço de usuário", tabelas de resumo para podar travessia | [resumo] https://www.osti.gov/biblio/1824127 | O desenho do GUFI está patenteado; o código é BSD-3 (adendo). Licença de patente não verificada |
| LANL, LA-UR-22-21380 (slides DisrupTECH, 2022) | Material de divulgação; afirma que o GUFI "preserva e honra" permissões | [texto] https://www.osti.gov/biblio/1846115 | Sem dados técnicos novos |
| LANL, Jacobi, "Applied Parallel Metadata Indexing" (2012) | Arquivo GPFS de 4+ PB e 170 milhões de arquivos; metadados importados para MongoDB e indexados por atributo; **uma tabela por usuário com só os registros que ele pode ler**, acesso por credenciais; interface FUSE | [resumo] https://www.osti.gov/servlets/purl/1048692 | Precursor de "um índice por domínio" no próprio LANL, com domínio = usuário |
| LANL, "Access Controlled AI Context with GUFI" (LA-UR-26-24285) | Já lido no adendo; o OSTI não devolveu registro para esse título nesta data | — | — |
| SNL, Lofstead, "Metadata Driven Data Management" (2019) e Lawson e Lofstead, "SystemX" (HPDC 2020) | Registros sem resumo no OSTI | [resumo] https://www.osti.gov/biblio/1641766 ; https://www.osti.gov/biblio/1764669 | Não verificado |
| LLNL, Ames, Gokhale, Maltzahn, QFS/Quasar (2009-2010) | FS rico em metadados com modelo de grafo e linguagem de consulta derivada de XPath; ingestão superior e consulta comparável ao padrão "FS + banco relacional" | [resumo] https://www.osti.gov/biblio/975221 | Precedente de consulta dentro do FS; resumo não fala de permissões |
| ORNL | TagIt (já coberto); "Scientific user behavior and data-sharing trends in a petascale file system" (SC 2017) cita Spyglass; MOSIQS (IEEE Access 2021) indexa metadados em memória persistente | [resumo] | Sem tratamento de permissões nos resumos |
| ANL, NERSC | Nada específico nas buscas OSTI e OpenAlex | — | Sem resultado |
| CEA (HAL) | Buscas no HAL por "metadata search file system", "indexation métadonnées système de fichiers" e "robinhood policy engine" não trouxeram trabalho novo sobre busca com permissões (só "Multicriteria File-Level Placement Policy for HPC Storage", 2025, sobre posicionamento) | https://api.archives-ouvertes.fr | Sem resultado |
| INRIA | Logic File System (IRISA) | [resumo] | Sem tratamento de permissões no resumo |
| Jülich, HLRS | Nada encontrado | — | Sem resultado |

## 4. arXiv e busca com controle de acesso

### 4.1 Tabela

| Trabalho | Mecanismo | Permissões | Resultados | Implicação para E ou B | Nível e URL |
| --- | --- | --- | --- | --- | --- |
| Wang, Grubbs, Lu, Bindschaedler, Cash, Ristenpart. "Side-Channel Attacks on Shared Search Indexes", IEEE S&P 2017, pp. 673-692 | Ver 4.2 | Filtro depois da pontuação | Ver 4.2 | Partição é a mitigação aceita | [texto] http://seclab.illinois.edu/wp-content/uploads/2018/01/sp17stress.pdf ; DOI 10.1109/SP.2017.50 |
| Micheli, Margaritis, Anastasiadis. Lethe: "Efficient Multi-User Indexing for Secure Keyword Search" (2014, local de publicação não verificado); IEEE BigData Congress 2014; Transactions on Data Privacy 8(1) 2015, 29-53 | Ver 4.3 | Índice compartilhado só por quem pode ler tudo nele | Ver 4.3 | Quantos domínios existem e o custo de fundi-los | [texto] https://csl.cse.uoi.gr/wp-content/uploads/lethe-tdp2015.pdf |
| Zerr, Demidova, Olmedilla, Nejdl, Winslett, Mitra. Zerber, EDBT 2008 | r-confidencialidade: grau de vazamento sobre termos de documentos inacessíveis; divisão de segredo e fusão de termos | Confiança limitada num índice central | Inserção e busca com bom desempenho em dados reais (resumo) | Wang et al. dizem não estar claro que garantia a variante estatística dá | [resumo] https://doi.org/10.1145/1353343.1353380 |
| Bjørklund, Götz, Gehrke. "Search in social networks with access control", 2010 (local não verificado); Bjørklund et al. "Workload-aware indexing for keyword search in social networks", CIKM 2011 | Organização dos metadados de controle de acesso escolhida por modelos de custo da carga; operador HeapUnion | Cada usuário vê um subconjunto único | Até 2x no operador e até 3x sobre trabalhos anteriores (resumo de 2011) | Se E crescer para "QUERY em vários domínios", a escolha de organização por custo é o precedente | [resumo] https://doi.org/10.1145/1868366.1868370 ; https://doi.org/10.1145/2063576.2063656 |
| Gelernter, Herzberg. "Cross-Site Search Attacks", ACM CCS 2015 | Usa o **tempo** de resposta de buscas, medido do navegador, para extrair informação privada | Contorna a política de mesma origem | Ataques eficazes com ferramentas melhores, apesar da variância de rede (resumo) | Único ataque por tempo a índice de busca encontrado; confirma que canal de tempo em busca é explorável | [resumo] https://doi.org/10.1145/2810103.2813688 |
| Zhong, Lentz, Narodytska, Szekeres, Rong. HoneyBee, arXiv 2505.01538, 2025 | Partições sobrepostas guiadas pela estrutura RBAC, com réplicas escolhidas por otimização (memória, latência, recall) | Índice por papel ou índice comum com row-level security | Até 13,5x menos latência que RLS do PostgreSQL (pgvector, HNSW) com 1,24x de memória; partição por papel custou 7x a memória do índice único; 90,4% menos memória extra que índices por papel | Mesma troca de B e E em outro tipo de índice; replicar entre domínios é opção medida | [texto parcial] https://arxiv.org/abs/2505.01538 |
| Han, Chakraborty, Mehrotra. Veda/EffVeda, arXiv 2605.01342, 2026 | Particiona por combinação de papéis em blocos disjuntos, organizados num reticulado com cópia e fusão sob orçamento de memória | Consulta cobre só nós autorizados; nós "impuros" podados por limite de distância | Maior vazão com recall alto, perto do orçamento (resumo) | Formaliza "domínio = combinação de papéis", o mesmo que os barris de 2007 | [resumo] https://arxiv.org/abs/2605.01342 |
| Jin, Wu, Hu, Maggs, Zhang, Zhuo. Curator, arXiv 2401.07119, 2024 | Árvore de agrupamento por inquilino codificada como subárvore de uma árvore comum | Inquilino | Desempenho de índice por inquilino com memória de índice único com filtro (resumo) | Mostra que índice comum com estrutura por domínio é viável; análogo a E com partições compartilhando a estrutura | [resumo] https://arxiv.org/abs/2401.07119 |
| Yalamarthi, Pappachan. "Policy-aware Vector Search", arXiv 2606.19803, 2026 | Visão: formaliza controle de acesso fino em bancos vetoriais e compara estratégias | Fino, por política | Preliminares | Confirma que o tema segue aberto em 2026 | [resumo] https://arxiv.org/abs/2606.19803 |
| Bhatt et al. (Microsoft). "Enterprise AI Must Enforce Participant-Aware Access Control", arXiv 2509.14608, 2025 | Todo conteúdo usado em treino, recuperação ou geração deve ser autorizado para **todos** os participantes da interação | Determinística, não probabilística | Ataques de exfiltração demonstrados; implantado no Microsoft Copilot Tuning (resumo) | O `QUERY` usa as credenciais de um processo; um agente que responde a vários usuários precisaria da interseção, que nem E nem B oferecem | [resumo] https://arxiv.org/abs/2509.14608 |
| Jayaraman et al. "Permissioned LLMs", arXiv 2505.22860, 2025 | Controle de acesso sobreposto às respostas de um LLM ajustado (PEFT por domínio) | Por domínio de dados | Métricas DDI e UGI em cinco conjuntos (resumo) | Outro nível (modelo, não índice); descartado para a ADR | [resumo] https://arxiv.org/abs/2505.22860 |
| Johnson et al. AIM, arXiv 2609.12320, 2026 | Memória de agentes multiusuário que classifica informação em privada ou pública e aplica controle no índice | Privado do dono ou público | 96,0% de acurácia na classificação de visibilidade (resumo) | Mesmo modelo de dois domínios de Goods; no MoobFS a classificação seria por modo do arquivo, não por modelo | [resumo] https://arxiv.org/abs/2609.12320 |
| Li et al. BudgetLeak, arXiv 2511.12043, 2025 | Inferência de pertinência em RAG pelo orçamento de geração | — | Supera bases (resumo) | Canal lateral em RAG, mas de outra natureza; descartado | [resumo] https://arxiv.org/abs/2511.12043 |

### 4.2 Wang et al.: ataques laterais a índices compartilhados (S&P 2017)

**Mecanismo do sistema atacado.** [texto] Elasticsearch e Solr multi-inquilino guardam documentos de
muitos usuários em índices compartilhados (fragmentos), com frequência de documento (DF) por termo
calculada sobre todos; o controle de acesso é um filtro (por id de inquilino na consulta ou alias
filtrado) aplicado depois. **Ataque (STRESS):** a pontuação TF-IDF de um documento do atacante depende do
DF global; o atacante mapeia quantos fragmentos existem, coloca documentos próprios em cada um e, por
co-residência, descobre termos de documentos alheios ou **quantos documentos alheios contêm um termo**.
Demonstrado em GitHub, Xen.do e outros; uma variante só com a ordem dos resultados funciona sem
pontuação. **Contramedidas discutidas:** índice por usuário ("received wisdom" o considera caro em
escala); visão por usuário de Büttcher e Clarke; DFs "cegos" (só documentos públicos) ou DFs de um
corpus público (aumento de latência de cerca de 1% em microbenchmarks com Lucene 6.3.0, qualidade de
ranking igual). **Resposta da Elastic:** índice por inquilino para implantações pequenas, admitindo que
pode ser proibitivo nas grandes; desligar pontuação quando aceitável. **Documentação atual** [texto]
https://www.elastic.co/docs/deploy-manage/users-roles/cluster-or-deployment-auth/controlling-access-at-document-field-level :
a segurança por documento não afeta as estatísticas globais de pontuação, e o usuário ainda pode
descobrir termos que só existem em documentos inacessíveis e contar quantos os contêm.

**Implicação.** O `QUERY` não devolve pontuação nem contagem, mas o canal de tempo medido no MoobFS
(cerca de 1,15 µs por candidato invisível) entrega a mesma informação (contagem de entradas alheias que
casam com um predicado) por outro meio. A literatura de busca não oferece mitigação por tempo constante;
a mitigação aceita é não percorrer entradas de outros domínios, isto é, particionar (E, ou B com um
banco por domínio).

### 4.3 Lethe: quantos domínios existem e quanto custa fundi-los (2014-2015)

**Mecanismo.** [texto] Um índice compartilhado é confidencial se for usado só por usuários autorizados
a buscar **todos** os documentos nele. Documentos são agrupados em "famílias" pelo conjunto de
pesquisadores autorizados; famílias parecidas são agrupadas (DBSCAN ou K-medoids); cada grupo tem um
índice para a interseção dos pesquisadores e índices privados ou adicionais para as diferenças, com um
limiar de duplicação. **Resultados** (DocuShare: 50 000 documentos, 1475 famílias; Elasticsearch
0.90.1): na configuração sugerida, 11,6 índices consultados por pesquisador e 7,8 índices atualizados por
documento; baixar o limiar triplica o custo de busca (11,6 para 35,8) e reduz o de atualização só 41%.
Tempo de resposta de 17,8 a 21,8 ms com 1 a 16 clientes nessa configuração; vazão de 37,0 para 339,8
consultas/s; configurações com muita duplicação ficaram abaixo de 19% das consultas em 50 ms por não
caberem em memória.

**Implicação.** Mesmo com partição por permissão, um usuário real consulta da ordem de dez índices. Para
E, isso favorece um `QUERY` sobre um diretório por vez, deixando a combinação de domínios ao cliente;
para B, dez bancos SQLite abertos por consulta é o custo esperado se os domínios forem conjuntos de
leitores e não namespaces.

### 4.4 Outros sistemas industriais de segurança por documento

| Sistema | Mecanismo | Implicação | Nível e URL |
| --- | --- | --- | --- |
| OpenSearch DLS | Consulta DSL por papel, combinada por OU entre papéis; modos "Lucene-level" (mais eficiente), "filter-level" e adaptativo; DLS não restringe escrita | Filtro dentro do motor; a página pede consultas simples para limitar impacto, sem números | [texto] https://docs.opensearch.org/latest/security/access-control/document-level-security/ |
| Azure AI Search, controle por documento | Filtro por string de identidade ou ACL "POSIX-like" do ADLS Gen2 copiada no índice e comparada ao token Entra na consulta; mudanças de ACL na origem só valem depois de nova sincronização (execução do indexador ou push) | Vinculação antecipada com janela de desatualização declarada; é o modelo de B sem correção no kernel | [texto] https://learn.microsoft.com/en-us/azure/search/search-document-level-access-overview |
| PostgreSQL row-level security | Políticas por linha; restrições de unicidade e chaves estrangeiras ignoram RLS; a documentação manda evitar "covert channel"; funções `leakproof` podem ser avaliadas antes da política | Um índice relacional com filtro por linha tem canais laterais documentados; SQLite não tem RLS | [texto] https://www.postgresql.org/docs/current/ddl-rowsecurity.html |
| Vespa | Busca não encontrou documentação oficial de segurança por documento | Não verificado | — |

## 5. Indexação ou consulta no lado do kernel Linux

| Trabalho | Mecanismo | Permissões | Resultados | Implicação | Nível e URL |
| --- | --- | --- | --- | --- | --- |
| Kokoszka, Donnelly, Thain. "Search Should Be a System Call", Notre Dame TR-2013-03 | Ver 5.1 | Erros de permissão relatados por caminho no buffer de resultados | Redução de 5% a 95% das chamadas de sistema (resumo) | Único precedente de busca como chamada do kernel Linux | [texto] http://ccl.cse.nd.edu/research/papers/search-tr.pdf |
| Wang, Meng, Li, Wu (China Telecom). SPMeta, "Accelerating Metadata Management of DFS via Speculative Permission Checking", IEEE ICDE 2026 | Ver 5.2 | POSIX u/g/o; ACLs não mencionadas | Ver 5.2 | Corta o custo por resultado sem particionar | [texto] https://cis.temple.edu/~jiewu/research/publications/Publication_files/ICDE2026-SpeculativePermissionChecking.pdf |
| LSFMM+BPF 2026, Szeredi, "Caching for extended attributes" (LWN, Jake Edge, 2026-06-02) | Cache comum de xattrs (provavelmente tabela hash) para FUSE e outros, consolidando `simple_xattr` e o cache do NFS | Não discutido | Sem objeções; detalhes em aberto | Infraestrutura de cache, não de índice; nenhuma discussão de busca por valor | [secundária] https://lwn.net/Articles/1074919/ |
| LSFMM+BPF 2024, Overstreet, "New APIs for filesystems" (LWN) | Pedido de interface iteradora para xattrs, no estilo `getdents()` | — | Discussão | Iterar xattrs é pré-requisito de varredura em B; ainda não existe | [secundária] https://lwn.net/Articles/975444/ |
| LSFMM+BPF 2026, "An update on fanotify" | Já coberto no documento principal | — | — | — | [secundária] https://lwn.net/Articles/1075829/ |
| Basani, "Filesystem-Native Metadata Indexing for Modern Workloads" (2026, depósito Zenodo, periódico indicado pelo OpenAlex: 工程科学与技术) | Utilitário de indexação de metadados dentro do volume, por pequena interface de chamadas de sistema, transacional com o FS | Menciona "indicadores de controle de acesso" como campo indexável | Sem números no resumo | Proposta próxima de E, sem avaliação verificável nem código encontrado; tratar com cautela | [resumo] https://doi.org/10.5281/zenodo.21925184 |

### 5.1 Busca como chamada de sistema (Notre Dame, 2013)

[texto] Propõe `search` (com variantes de biblioteca `opensearch`/`readsearch`) que recebe caminhos e
padrões e devolve um buffer de entradas (caminho, `stat` opcional, erro), como `getdents`. Três
implementações de cerca de 500 linhas cada: no kernel Linux (camada portátil de chamadas, usando
operações abstratas do VFS de leitura de diretório e `stat`; os autores dizem que uma versão completa
acrescentaria uma operação de busca ao VFS para delegar a drivers otimizados), num sandbox `ptrace`
(Parrot) e num FS distribuído em espaço de usuário (Chirp). Sem índice: é varredura feita no kernel.
Permissões: a busca roda com as credenciais do chamador; diretório sem permissão de listagem gera um erro
registrado no resultado (por exemplo, `EPERM` em "Private"), não aborta a busca. Recursão limitada a 16
níveis (`EOVERFLOW`) pela pilha do kernel. **Implicação:** confirma que a forma "consulta emitida a partir
de um diretório, autorizada como travessia normal" (a de E) é natural no VFS; também mostra que a ideia
não foi adiante no mainline (nenhuma discussão no LKML foi encontrada).

### 5.2 SPMeta: alcance especulativo por diretório (ICDE 2026)

[texto] Observa que, em cinco namespaces reais de IA e HPC, mais de 99,7% dos caminhos seguem um
"padrão descendente de permissão" (bits que não aumentam de dono para grupo para outros ao longo do
caminho). Guarda, por diretório, um bit por classe (usuário, grupo, outros) dizendo se aquela classe
certamente alcança o diretório; com isso a checagem de caminho vira O(1) na maioria dos casos, com recurso
à checagem completa quando o padrão não vale. Um `chmod` invalida em faixa os estados de alcance da
subárvore; renomeação usa realocação de subárvore num trie (MART). Resultados: chamadas remotas e
consultas a banco de O(N) para O(1) na maioria das operações; 1,44x a 2,86x de vazão agregada sobre o
estado da arte. ACLs e grupos suplementares não são mencionados no texto extraído.
**Implicação:** o custo de 1,7 µs por resultado do `QUERY` vem da resolução de caminho com as
credenciais do chamador; um cache de alcance por diretório, invalidado por `chmod`/`chown`/`rename`,
ataca esse custo dentro da alternativa A ou E sem mudar a UAPI. Precisa de cuidado com ACLs, que o
artigo não trata.

## Trabalhos que mudam a análise da ADR-0009

1. **Singh, Srivatsa e Liu (2007) e Lethe (2014-2015): o domínio é o conjunto de leitores.** A ADR
   define E por "diretório de primeiro nível escolhido na montagem". Os dois trabalhos mostram que a
   partição que fecha o vazamento é por expressão de credenciais, com prova de minimalidade (2007) e com
   medida de quantos domínios aparecem (2132 para 339 mil arquivos; 1475 para 50 mil documentos). Se um
   namespace do MoobFS tiver arquivos com conjuntos de leitores diferentes (um `0600` de outro uid
   dentro de um namespace legível pelo grupo), o canal de tempo reaparece dentro da partição. Saídas: (a)
   E com partição por (namespace, conjunto de leitores) e QUERY percorrendo só as partições do chamador,
   ou (b) declarar como invariante que um namespace é homogêneo e recusar (ou contabilizar) arquivos que
   a violem. A ADR precisa escolher.
2. **Wang et al. (S&P 2017) e a documentação do Elasticsearch: o vazamento por contagem é inerente ao
   filtro posterior e foi explorado em produção.** Isso fortalece o argumento da ADR de que só não
   percorrer outros domínios fecha o canal, e mostra que a indústria convive com o vazamento por custo.
   Para o MoobFS, que mediu o canal, documentar a escolha (fechar com partição ou aceitar como limitação)
   passa a ter precedente explícito.
3. **Bailey, Hawking e Matson (CIKM 2006): checagem por coleção e em lote.** Dá base medida às duas
   peças novas da ADR (autorização uma vez até o diretório em E; autorização em lote em B) e à decisão de
   não expor contagem total.
4. **SPMeta (ICDE 2026) e IndexFS (SC 2014): o custo por resultado pode cair sem particionar.** Cache de
   alcance por diretório e classe, invalidado por mudança de permissão, é uma alternativa C'' menor que
   E para o problema de 1,7 µs por resultado. Não resolve o canal de tempo nem o crescimento por
   candidato; por isso complementa E, não a substitui.
5. **Zanzibar (ATC 2019): o "new enemy" é o risco de B.** A janela entre `chmod` e o índice externo não é
   só atraso: é servir metadados a quem perdeu acesso. A literatura resolve com marcador de consistência
   exigido na consulta. Se B for adiante, o contador monotônico do `GET_INFO` deve ser comparado pelo
   cliente antes de usar o índice, e a autorização em lote deve recusar quando o índice estiver atrás.
   Isso pesa a favor de E no critério "índice atualizado na hora".
6. **Goods (SIGMOD 2016) e AIM (2026): separar o domínio público.** Um índice único para o que é legível
   por todos e partições só para o privado reduz o número de domínios na prática; vale para E e para B.

Não mudam a análise, apesar de próximos: HoneyBee, Veda e Curator (confirmam a troca em bancos vetoriais,
sem tratar canais laterais); Search Should Be a System Call (precedente de interface, sem índice nem
avaliação de segurança); "Participant-aware access control" (aponta uma lacuna, a de agentes que atendem
vários usuários, fora do escopo da ADR-0009 mas digna de registro no roteiro de segurança).

## Fontes varridas sem resultado relevante

- **Citantes de Spyglass no OpenAlex (129 registros):** triados todos; sem tratamento de permissões além
  de Parker-Wood (MSST 2010, já coberto) e GUFI. Exemplos descartados: SmartCuckoo (ATC 2017), VT-trees
  (FAST 2013), Haystack (OSDI 2010), "Optimizing Every Operation in a Write-optimized File System" (FAST
  2016), "Writes Wrought Right" (TOS 2017), D2-Tree (ICDCS 2018), MIQS (SC 2019), GraphMeta (CLUSTER
  2016), "The case for sampling on very large file systems" (MSST 2014), "Using Property Graphs for Rich
  Metadata Management in HPC Systems" (PDSW 2014), VSFS (PDSW 2014), FAST (SC 2014, busca aproximada),
  "Just-in-Time Analytics on Large File Systems" (TC 2011), DelveFS (CLUSTER 2020), SANE e Rapport
  (Nebraska), MBFS (J. Supercomputing 2015), "An Integrated Indexing and Search Service for Distributed
  File Systems" (TPDS 2020, versão do TagIt), "Efficient Metadata Indexing for HPC Storage Systems"
  (CCGrid 2020, Brindexer, já coberto).
- **Citantes de GUFI SC22 (4):** SwiftKV (2025), Lustre Query (HPDC 2026), Bjornson (2026), SPMeta
  (ICDE 2026). Só SPMeta é relevante.
- **Citantes de Büttcher e Clarke (23) e de Parker-Wood (10):** além dos itens das tabelas, trabalhos de
  busca sobre dados cifrados (Information Systems 2018; TIFS 2015), SeDas (TMAG 2013), "Using provenance
  to efficiently improve metadata searching performance" (FGCS 2014), "Privacy preserving document
  indexing infrastructure for a distributed environment" (PVLDB 2008), "Scalable full-text search for
  petascale file systems" (PDSW 2008), "Bring Precision and Access Control to Business Document Search"
  (SNPD 2008) e "Access control for sharing semantic data across desktops" (2007): só títulos e resumos;
  nenhum traz mecanismo além dos já descritos.
- **Grupos:** UW-Madison ADSL, Stony Brook FSL, Jülich, HLRS, ANL, NERSC: nada sobre busca de metadados
  com autorização.
- **HAL (CEA, INRIA):** três consultas, nada relevante.
- **OSTI:** "GUFI" (30 registros, só slides, patente e o pôster/divulgação); "Access Controlled AI
  Context" e "GUFI retrieval augmented": sem registro nesta data.
- **arXiv:** consultas por "access control" com "retrieval-augmented", "permission" com RAG, "side
  channel" com RAG/vector database/search index, "permissioned LLM", "role-based access control" com
  "vector database", "access control" com "agent memory". Itens fora de escopo descartados: RAGent
  (geração de políticas), PrediQL, SmartLLM, FragFuse e SAMEP (protocolos de agentes), "Agent-Assisted
  Side-Channel Attacks on Non-Prefix KV Cache in RAG" (canal em cache de KV do modelo, não no índice).
- **Kernel:** OpenAlex por "extended attributes indexing Linux kernel", "in-kernel file search index
  Linux VFS", "attribute-based file system query kernel module", "tag-based file system Linux kernel":
  nada além de S-VFS (IEICE 2007, só título) e de trabalhos sem relação. LWN: índice do LSFMM+BPF 2026
  sem sessão de busca ou índice de xattrs.
- **Vespa:** nenhuma página oficial sobre segurança por documento encontrada na busca.

## Ainda não verificado

- **Google Scholar, Semantic Scholar e dblp não foram usados de fato** (sem API e respostas 429); a
  cobertura de citações depende do OpenAlex, que registra menos citações que o Scholar. Uma segunda
  rodada com dblp por venue (FAST, OSDI, ATC, HotStorage, SOSP, EuroSys, TOS, 2015-2026) pode achar
  artigos que não citam as quatro âncoras.
- **Proceedings de HotStorage, EuroSys e OSDI** não foram percorridos página a página.
- **Texto completo** da dissertação de Parker-Wood (2014; o PDF respondeu 403), da tese de Büttcher
  ("Multi-user file system search", 2007), de Bjørklund et al. (2010, 2011), de Zerber (2008), de
  Bjornson (2026; bloqueado) e de Johnson et al. (FAST 2014).
- **Artigo GUFI SC22** continua sem cópia aberta encontrada.
- **SPMeta e ACLs/grupos suplementares:** o texto extraído não os menciona; não verificado se a versão
  publicada os trata.
- **HoneyBee:** só abstract, introdução e trechos foram lidos; não verifiquei como medem atualização de
  permissões.
- **Licença da patente US 11 048 699** frente ao código BSD-3 do GUFI.
- **Mitigação de canal de tempo em índices** (tempo constante, ruído, limites de taxa): nenhuma fonte
  encontrada propõe isso para busca de metadados; a afirmação de ausência vale só para as buscas feitas.
- **Vespa** e **Solr**: mecanismo de segurança por documento não verificado nesta rodada.
