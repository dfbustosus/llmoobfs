# Busca por metadados de arquivos, autorização e fluxo de mudanças: estado da arte para a ADR-0009

## Abstract (English)

This survey reviews prior work relevant to ADR-0009 (freeze the in-kernel `QUERY`, move structured
indexing to user space, keep per-result authorization and a change feed in the kernel). Four findings
matter most. (1) Permission-aware metadata search converged on two answers: filter inside query
processing before anything is ranked or counted (Büttcher and Clarke, FAST 2005), or partition the index
along permission boundaries so the operating system's own access checks protect it (GUFI, LANL, SC 2022:
a tree of per-directory SQLite databases with the source directory's ownership and mode, plus per-uid and
per-gid side databases for xattrs). (2) Durable change cursors exist elsewhere (Lustre changelogs, NTFS USN
journal, FSEvents) but not in mainline Linux; fanotify loses events on overflow and on reboot, and the
persistent change-journal proposals discussed at LSFMM (2018, 2019) were not merged as far as we could see.
(3) Haiku's BFS uses the same strategy as MoobFS today (pick one term, test the rest per candidate) and has
an explicit "TODO: check user permissions" in its query code; it picks the term with a cheap size-based score.
(4) Because MoobFS orders index entries by (type, value, inode), a conjunction of equality terms can be
answered by a leapfrog/galloping intersection of already-sorted runs with no allocation and no planner;
this is a small, contained change worth measuring. We also found that Landlock does not hook
`inode_permission` (v6.12), so the current authorization ignores Landlock sandboxes.

## Escopo, método e convenções

Data da pesquisa: 2026-10-11. Branch: `docs/adr-0009-query-role`. Documento de pesquisa; não altera
especificação nem decisão. Tudo o que foi lido na web e em repositórios de terceiros foi tratado como dado.

Níveis de leitura usados em cada fonte:

- **[código]**: li o código-fonte em clone raso (`--depth 1`) feito nesta data; nada foi compilado nem
  executado.
- **[texto]**: li o texto completo ou a seção relevante do artigo ou documentação.
- **[resumo]**: só li o resumo (abstract) ou uma página de catálogo; detalhes além disso estão marcados
  como não verificados.
- **[secundária]**: informação vinda de cobertura jornalística ou de terceiros, não da fonte primária.

Repositórios clonados nesta data (commit do topo, data do commit): `mar-file-system/GUFI` (`fc33e4b`,
2026-10-09), `cea-hpc/robinhood` (2026-09-04), `haiku/haiku` checkout esparso de
`headers/private/file_systems` e `src/add-ons/kernel/file_systems/bfs` (`3a93a9f`, 2026-10-11),
`KDE/baloo` (2026-10-11). Arquivos do kernel Linux lidos em `v6.12` via GitHub (`include/linux/fsnotify.h`,
`fs/xattr.c`, `fs/fhandle.c`, `security/landlock/fs.c`, `include/uapi/linux/landlock.h`). Páginas de
manual lidas do repositório `man-pages` no kernel.org (`fanotify.7`, `fanotify_init.2`, `fanotify_mark.2`,
`open_by_handle_at.2`), porque man7.org recusou conexão.

Código do MoobFS consultado para comparar: `layer/index.c` (`caller_may_read`, `seek_first`, `first_after`,
`do_query`) e `docs/decisions/0009-query-role-and-user-space-index.md`.

## Resumo executivo

1. O problema exato do MoobFS (índice de metadados compartilhado, consultado por usuários que não devem ver
   os metadados uns dos outros) é conhecido desde 2005. Büttcher e Clarke mostraram que filtrar depois de
   consultar (pós-processamento, "late binding") vaza informação por estatísticas e contagens; a correção é
   restringir os candidatos ao que o usuário pode ver **antes** de calcular qualquer coisa.
2. O `QUERY` atual filtra depois (resolve caminho e chama `inode_permission` por resultado), mas não devolve
   contagem global nem ranking, então o ataque de Büttcher e Clarke não se aplica diretamente. Resta um canal
   de tempo plausível (não medido): o custo cresce com candidatos de outros agentes, a cerca de 1 µs cada.
3. A literatura de HPC (Spyglass, SmartStore, Magellan, Propeller, TagIt, Brindexer, Robinhood, Icicle)
   resolve escala, não isolamento; quase todos são ferramentas de administrador ou declaram segurança como
   trabalho futuro. A exceção é o GUFI, que é o precedente mais próximo do que a ADR-0009 precisa.
4. O GUFI usa a hierarquia de diretórios como partição de permissões: um banco SQLite por diretório, com o
   dono e o modo do diretório de origem, e o próprio kernel nega a abertura dos bancos que o usuário não pode
   ler. Atributos estendidos que nem todos podem ler vão para bancos separados por uid e por gid.
5. Sistemas que juntaram índice e filesystem dentro do kernel ou do formato (BFS, WinFS, Inversion) ou
   ficaram restritos a um usuário, ou foram cancelados, ou foram retirados. Os que vingaram (Spotlight,
   Windows Search, Tracker, Baloo) mantêm o índice em espaço de usuário alimentado por um fluxo de mudanças.
6. Cursores duráveis de mudança existem em Lustre (changelog com leitores registrados), NTFS (USN com
   identificador de journal) e macOS (FSEvents com IDs por volume). O Linux só tem `fanotify`, que perde
   eventos em estouro de fila e em reinício; as propostas de journal persistente (LSFMM 2018, 2019) não
   viraram código no mainline até onde verifiquei.
7. O `fanotify` sem privilégio (desde 5.13) exige `FAN_REPORT_FID` e só marca inodes; marcar o filesystem
   inteiro exige `CAP_SYS_ADMIN`. Um indexador de volume inteiro é, portanto, um processo privilegiado.
8. O BFS do Haiku escolhe, numa conjunção, o termo de menor "score" (tamanho do índice em blocos dividido
   por heurísticas do operador) e testa os demais por candidato; não intersecta, e não checa permissões.
9. Como o índice do MoobFS ordena entradas por `(type, value, ino)`, termos de igualdade produzem sequências
   já ordenadas por inode. A interseção por saltos (leapfrog/galloping) dessas sequências é pequena, sem
   alocação, e ataca exatamente a forma medida que cresce com o tamanho (`namespace = X AND agent = Y`).
10. Achado lateral: o Landlock (v6.12) não registra o hook `inode_permission`; a autorização atual do
    `QUERY` e uma autorização em lote baseada no mesmo mecanismo ignoram sandboxes Landlock.

## 1. Pesquisa em busca por metadados de filesystem

| trabalho | ano | mecanismo | como trata permissões | lição para o MoobFS | fonte |
| --- | --- | --- | --- | --- | --- |
| Spyglass (Leung, Shao, Bisson, Pasupathy, Miller), USENIX FAST '09 | 2009 | Índice fora do FS, particionado por subárvore (~100 mil arquivos por partição); por partição, uma árvore K-D multiatributo e "signature files" (bit array de ~2 KB por atributo, valor com hash); coleta incremental por diferença de snapshots WAFL; versões de índice | Não trata: o texto diz que não aborda interface de busca nem segurança; propõe como trabalho futuro excluir partições inacessíveis | Particionar por namespace é ao mesmo tempo otimização e fronteira natural de permissão; filtros de bits evitam varrer partições sem chance de casar | [texto] https://static.usenix.org/events/fast09/tech/full_papers/leung/leung_html/index.html |
| Magellan (Leung, Adams, Miller), relatório técnico UCSC-SSRC-09-07 | 2009 | Busca integrada ao servidor de metadados: novo layout de inode, estruturas multiatributo, roteamento de consultas, novo journaling | Não verificado (só li o resumo) | Precedente de "índice dentro do FS" motivado por consistência sem gravação dupla, o mesmo argumento da ADR-0005 | [resumo] https://www.ssrc.ucsc.edu/pub/leung-ssrctr09-07.html |
| SmartStore (Hua, Jiang, Zhu, Feng, Tian), SC '09 | 2009 | Agrupa arquivos correlacionados por semântica de metadados (LSI) para limitar consultas complexas a poucos grupos | Não verificado (só resumo) | Pouca relação: otimiza consultas por proximidade em sistemas enormes, não isolamento | [resumo] https://www.usenix.org/conference/fast-09/smartstore-new-metadata-organization-paradigm-semantic-awareness (pôster FAST '09); versão TPDS 2012 citada no mesmo resultado |
| Propeller (Xu, Jiang, Tian, Huang), IEEE ICDCS 2014, DOI 10.1109/ICDCS.2014.46 | 2014 | Particiona índices pela causalidade de acesso; indexação em tempo real | Não verificado (só resumo) | Mostra que índices "em tempo real" exigem fluxo de mudanças confiável | [resumo] https://ranger.uta.edu/~jiang/publication/Conferences/2014/2014-ICDCS-Propeller-%20A%20Scalable%20Real-Time%20File-Search%20Service%20in%20Distribute%20Systems.pdf |
| TagIt (Sim, Kim, Vazhkudai, Vallée, Lim, Butt), SC17, DOI 10.1145/3126908.3126929; versão TPDS 2020 | 2017 | Indexação e marcação integradas a FS distribuído sem compartilhamento (GlusterFS e CephFS na versão TPDS) | Não verificado (só resumo) | Argumenta contra índice desacoplado; o ganho (até 10x) vem de evitar a cópia externa | [resumo] https://www.osti.gov/biblio/1423035 |
| GUFI (Manno, Lee, Challa, Zheng, Bonnie, Grider, Settlemyer), SC22 pp. 57:1-57:14; código BSD-3 (LANL LA-CC-15-039) | 2022 | Árvore de bancos SQLite que espelha a árvore de diretórios; consulta em paralelo por largura; "rollup" achata subárvores quando as permissões coincidem; atualização incremental por varredura comparada; xattrs `user.*` opcionais | Diretório do índice herda dono e modo do diretório de origem; usuário comum consulta com as próprias credenciais e o kernel nega bancos inacessíveis. Xattrs que nem todos os leitores do diretório podem ler vão para bancos externos por uid (`uid:nobody`) e por gid (`nobody:gid`) | Precedente direto para o índice por domínio de permissão da ADR-0009 (seção 6) | [código e docs] https://github.com/mar-file-system/GUFI (`docs/latex/sections/design.tex`, `rollup.tex`, `xattrs.tex`); [resumo] artigo: https://sc22.supercomputing.org/proceedings/tech_paper/tech_paper_pages/pap199.html |
| Brindexer (Paul et al., Virginia Tech e Cray), CCGrid 2020 | 2020 | Particionamento por níveis, índice em SGBD relacional, changelog do Lustre para mudanças, reindexação periódica | Não verificado; o resumo diz que o público principal são administradores | Mesmo padrão "SGBD + changelog"; não encontrei repositório público | [resumo] https://par.nsf.gov/servlets/purl/10167824 |
| Robinhood Policy Engine (CEA/DAM), CeCILL-C (compatível com LGPL), ativo (último commit 2026-09-04, ~226 estrelas) | 2004- | Réplica dos metadados em MySQL/MariaDB (há também `sqlite_wrapper.c` no código); varredura inicial e leitura contínua de changelogs do MDT do Lustre com `llapi_changelog_start` / `llapi_changelog_clear` | Ferramenta de administrador; não encontrei filtro por usuário no que li (não verificado em profundidade) | O changelog só é confirmado (`clear`) depois de aplicado ao banco: é o contrato de cursor durável que o Linux não tem | [código] https://github.com/cea-hpc/robinhood (`src/chglog_reader/chglog_reader.c`, `src/list_mgr/`) |
| Changelogs do Lustre | 2008- | Registro persistente no MDT; cada leitor se registra (`changelog_register`, ID `clN`) e confirma até um número de registro (`changelog_clear`); registros ficam até todos os leitores confirmarem | Leitura de changelog é operação de administrador (não verifiquei o modelo exato) | Leitor esquecido enche o MDT (relato de bilhões de registros acumulados): cursor durável tem custo operacional | [secundária] https://wiki.lustre.org/Lustre_Rsync_Replication ; https://www.eofs.eu/wp-content/uploads/2024/02/13_olaf_weber_lad18-experience_building_dmf7_on_lustre-olaf_weber-1.0.pdf |
| Icicle (Pan, Chard, Oh et al.), arXiv:2604.10295 | 2026 | Kafka + Flink; ingestão por snapshot e por eventos (Lustre, IBM Storage Scale); dois índices (descoberta e agregados) | O resumo não menciona controle de acesso | Confirma a direção da área: índice externo alimentado por eventos; ainda sem isolamento | [resumo] https://arxiv.org/abs/2604.10295 |

Observação sobre Spyglass e o caso do MoobFS: o Spyglass usa árvores K-D por partição para consultas com
várias condições, isto é, uma estrutura multidimensional em vez de um planejador. O custo de manter K-D trees
balanceadas sob atualização contínua não foi verificado aqui; o Spyglass atualiza em lotes por snapshot.

## 2. Busca de arquivos ciente de permissões

| trabalho | ano | mecanismo | como trata permissões | lição para o MoobFS | fonte |
| --- | --- | --- | --- | --- | --- |
| Büttcher e Clarke, "A Security Model for Full-Text File System Search in Multi-User Environments", USENIX FAST '05, pp. 169-182 | 2005 | Índice textual único para o sistema; modelo de segurança UNIX | Arquivo "pesquisável" se há caminho da raiz com permissão de execução em todos os diretórios e leitura no arquivo; ler o diretório pai não é exigido (para não expor nomes). Pós-processamento vaza: com ~2 consultas que devolvem escore, um atacante obtém quantos arquivos contêm um termo; com só a ordem, busca binária sobre arquivos que ele mesmo cria. A versão segura restringe cada lista de ocorrências ao conjunto visível antes do processamento, e as estatísticas passam a ser só do visível | O modelo de visibilidade é o mesmo que o MoobFS implementa (`vfs_path_lookup` exige execução nos diretórios; `inode_permission(MAY_READ)` no arquivo). Contagens, totais e escores globais são o vetor de vazamento; o `QUERY` atual não os expõe, mas qualquer índice externo com contagem, `COUNT(*)` ou paginação por total exporia | [texto] https://static.usenix.org/event/fast05/tech/full_papers/buettcher/buettcher_html/index.html |
| idem, custo medido | 2005 | Corpus TREC4+5-CR (528 mil arquivos) | Integração otimizada: 12-17% mais lenta com tudo visível e 20-39% mais rápida com 10% visível; gerenciador de segurança com ~32 bytes por inode em memória, apontado pelos autores como problema sem solução convincente | Restringir antes fica mais barato quando cada usuário vê pouco, que é o caso de memórias por agente | idem |
| Parker-Wood, Strong, Miller, Long, "Security Aware Partitioning for Efficient File System Search", IEEE MSST 2010, DOI 10.1109/MSST.2010.5496990 | 2010 | Particionamento do índice que integra segurança, em O(n), comparado a seis métodos (incluindo os de SmartStore e Spyglass) | Partições alinhadas às permissões; consultas tocam só partições que o usuário pode ver (detalhe do algoritmo não verificado) | Agrupar por fronteira de permissão é barato e dá desempenho bom; agrupamento sofisticado não compensa (segundo o resumo) | [resumo] https://www.crss.ucsc.edu/pub/parkerwood-msst10.html |
| Early binding e late binding em busca corporativa | 2006- | Early: ACL indexada junto do documento e aplicada como filtro dentro da consulta. Late: candidatos verificados contra o sistema de origem depois | Early dá contagens e facetas corretas, mas fica velho quando a ACL muda (reindexação). Late fica correto, mas é caro e distorce contagens. Muitos produtos combinam os dois | O MoobFS hoje é late binding por resultado, com a vantagem de checar no próprio kernel (sem cópia de ACL). Um índice externo particionado por domínio é early binding pela própria fronteira de arquivo | [secundária] https://cloud.googleblog.com/2006/12/document-level-security-with-enterprise.html ; https://www.cmswire.com/knowledge-findability/managing-enterprise-search-security/ ; documentação ServiceNow (via busca, não lida integralmente) |
| GUFI, regras de "roll in" e "rollup" | 2017- | Ver seção 1 | Rollup só se todos os filhos têm permissões compatíveis com o pai (o+rx; ou mesmos u/g e mesmos modos; etc.). Xattr fica no banco do diretório só se todos os que leem o diretório podem lê-lo (arquivo o+r; ou u/g coincidentes; ou diretório gravável por quem lê, porque esse pode dar chmod) | Regras explícitas e verificáveis de quando metadados de vários arquivos podem dividir o mesmo arquivo de índice | [código e docs] `docs/latex/sections/rollup.tex`, `xattrs.tex` |

Vazamento por contagem e tempo no MoobFS (análise, não medida): o `QUERY` só marca `truncated` depois de
autorizar, então o sinalizador não revela itens alheios. O tempo, porém, cresce com os candidatos do primeiro
termo, inclusive os que o chamador não pode ver (~1 µs por candidato nas medições da ADR-0009). Um agente que
mede a latência de `agent = X AND namespace = Y` pode estimar quantas memórias alheias casam com o primeiro
termo. É o análogo, por tempo, do ataque de Büttcher e Clarke. Severidade não avaliada.

## 3. Indexação no kernel ou integrada ao filesystem: o que deu certo e o que não deu

| trabalho | ano | mecanismo | como trata permissões | lição para o MoobFS | fonte |
| --- | --- | --- | --- | --- | --- |
| BFS (BeOS) e Haiku; livro de Giampaolo, "Practical File System Design with the Be File System", Morgan Kaufmann | 1998/1999 | Atributos por arquivo; índices B+ por nome de atributo no próprio volume; consultas e "live queries" servidas pelo FS | Não checa. Em `QueryParser.h` (Haiku, licença MIT) há o comentário "TODO: check user permissions here - but which one?!", que discute checar execução em cada diretório pai e conclui que é caro | O desenho de referência da ADR-0005 nunca resolveu permissões; o MoobFS já está além do BFS nisso | [código] `headers/private/file_systems/QueryParser.h` (Haiku `3a93a9f`), linhas ~944-951; livro não lido nesta pesquisa |
| BFS, escolha do índice numa conjunção | idem | `Equation::CalculateScore`: score = tamanho do índice em blocos (`IndexGetSize`), dividido por 8 (ou pelo tamanho da chave) para `=`, `>`, `>=`, por 2 para outros, por um fator do prefixo para padrões; `!=` varre tudo. `Operator::Score` em AND devolve o menor dos dois; em OR, o maior. `Query::Rewind` empilha só o ramo de menor score de cada AND e os dois ramos de cada OR | Não se aplica | Não há interseção nem estatística de valores: um ramo é iterado pelo índice e os demais são testados por candidato (`Match`), como no MoobFS. A diferença é que o BFS escolhe o ramo por um custo barato em vez de usar sempre o primeiro | [código] `QueryParser.h` (funções citadas); `src/add-ons/kernel/file_systems/bfs/Query.cpp` (`IndexGetSize` divide o tamanho do nó do índice pelo tamanho de bloco) |
| WinFS (Microsoft) | 2003-2006 | Armazenamento relacional sobre SQL Server para o Windows | Não verificado | Cancelado como produto separado em junho de 2006; partes absorvidas no SQL Server e ADO.NET. Fonte primária (blog da equipe) não localizada; razões técnicas detalhadas não verificadas | [secundária] https://redmondmag.com/articles/2006/06/27/microsoft-repurposes-winfs-for-future-products.aspx ; https://www.eweek.com/database/why-microsoft-pulled-winfs-as-a-stand-alone-product/ |
| Inversion File System (Olson), USENIX Winter 1993, pp. 205-218 | 1993 | Arquivos guardados como objetos no POSTGRES; transações e "time travel" | Do SGBD (não verificado) | Desempenho de 30% a 80% do NFS da época; o FS foi retirado do POSTGRES95, sobrando só "large objects" | [resumo] https://dsf.berkeley.edu/papers/S2K-93-28.pdf ; https://usenix.org/conference/usenix-winter-1993-conference/design-and-implementation-inversion-file-system |
| Semantic File Systems (Gifford, Jouvelot, Sheldon, O'Toole), SOSP '91, DOI 10.1145/121132.121138 | 1991 | Transdutores extraem atributos; diretórios virtuais interpretam o caminho como consulta conjuntiva | Não verificado | Precedente de consulta por caminho; o índice era mantido fora do armazenamento | [resumo] https://www.mit.edu/~Saltzer/publications/recguides/sfs.html |
| Spotlight e FSEvents (Apple) | 2005- | Índice por volume mantido por daemon (`mds`); FSEvents grava eventos em `.fseventsd` na raiz de cada volume, com IDs que sobrevivem a reinício quando usados com o UUID do dispositivo; sinalizadores avisam quando IDs antigos deixam de valer | O daemon de índice roda com privilégio e filtra por usuário (provável; não verifiquei em documentação da Apple). Büttcher e Clarke conjecturam que o ranking do Spotlight estaria sujeito ao ataque | Índice em espaço de usuário + fluxo de mudanças persistente é o modelo que vingou | [texto parcial] https://developer.apple.com/library/mac/documentation/Darwin/Conceptual/FSEvents_ProgGuide/UsingtheFSEventsFramework/UsingtheFSEventsFramework.html |
| Windows Search e journal USN do NTFS | 2000- | Journal USN por volume; leitura por `FSCTL_READ_USN_JOURNAL` com (journal ID, USN inicial); o FS só devolve registros válidos para o ID dado | Ler o journal exige administrador. O filtro por usuário dos resultados do Windows Search não foi verificado | A documentação diz que o journal existe também para recuperar a indexação sem reindexar o volume depois de falhas; o ID muda quando os registros podem estar inutilizáveis (consumidor deve então revarrer) | [texto] https://learn.microsoft.com/en-us/windows/win32/fileio/change-journals ; https://learn.microsoft.com/en-us/windows/win32/fileio/using-the-change-journal-identifier |
| GNOME Tracker / LocalSearch + TinySPARQL | 2006- | Daemon por sessão (`tracker-miner-fs-3`); banco SQLite (TinySPARQL); atualização em tempo real | Índice por usuário, na sessão do usuário (só indexa o que ele lê) | Índice por usuário é seguro e simples, mas duplica trabalho quando vários usuários veem os mesmos arquivos (o custo apontado por Büttcher e Clarke). O mecanismo de monitoramento (inotify ou fanotify) não foi verificado | [secundária] https://mankier.com/package/localsearch |
| KDE Baloo (LGPL/GPL, ativo, último commit 2026-10-11) | 2014- | LMDB (`mdb_env_open` em `src/engine/database.cpp`); monitoramento por inotify (`src/file/kinotify.cpp`) | Índice por usuário | Mesma lição do Tracker; o inotify exige um watch por diretório e também perde eventos em estouro | [código] https://github.com/KDE/baloo |
| btrfs `find-new` e números de geração | 2008- | Geração por transação; `btrfs subvolume find-new <subvol> <gen>` lista arquivos alterados depois da geração dada e imprime a última geração | Ferramenta de administrador (ioctl de busca em árvore exige privilégio; não verificado nesta pesquisa) | Contador monotônico persistente já basta para "o que mudou desde X" sem journal de eventos; só existe no btrfs | [secundária] https://btrfs.readthedocs.io/en/latest/_sources/btrfs-subvolume.rst.txt |

Por que uns vingaram e outros não, segundo as fontes lidas: os que sobreviveram mantêm o índice fora do
formato em disco e fora do kernel, e pedem ao kernel apenas um fluxo de mudanças (FSEvents, USN, inotify). Os
que embutiram o índice no armazenamento ou ficaram presos a um sistema (BFS) ou foram cancelados e
absorvidos por um SGBD (WinFS, Inversion). As razões declaradas do cancelamento do WinFS são de produto, não
técnicas; não encontrei fonte primária com análise técnica.

## 4. Recursos do kernel Linux para fluxo de mudanças e autorização em lote

| trabalho | ano | mecanismo | como trata permissões | lição para o MoobFS | fonte |
| --- | --- | --- | --- | --- | --- |
| `fanotify` com `FAN_ATTRIB` | 5.1 | Evento de mudança de metadados; exige grupo com identificação por handle (`FAN_REPORT_FID`). `setxattr`/`removexattr` disparam `fsnotify_xattr`, que gera `FS_ATTRIB` | Ver linha "sem privilégio" | Mudança de atributo `user.moob.*` feita pelo VFS gera evento. Para o MoobFS empilhado, o evento chega ao inode da camada quando o usuário grava pela camada; se também chega ao inode inferior depende de a camada chamar `vfs_setxattr` no inferior (não verificado) | [código] `include/linux/fsnotify.h` (v6.12, `fsnotify_xattr`), `fs/xattr.c` (3 chamadas); [texto] `fanotify_mark.2` |
| `FAN_REPORT_FID`, `FAN_REPORT_DIR_FID`, `FAN_REPORT_NAME`, `FAN_REPORT_DFID_NAME`, `FAN_REPORT_TARGET_FID` | 5.1, 5.9, 5.9, 5.9, 5.17 | Eventos trazem handle do objeto, do diretório pai e o nome da entrada | Sem handle o ouvinte só tem identificador opaco; reabrir pelo handle exige privilégio (abaixo) | Com `DFID_NAME` o indexador recebe (diretório, nome) e consegue atualizar caminhos sem `open_by_handle_at` em muitos casos | [texto] `fanotify_init.2` |
| Estouro de fila e persistência | - | Limite por grupo em `/proc/sys/fs/fanotify/max_queued_events` (antes do 5.13, fixo em 16384); excedido, eventos são descartados e um `FAN_Q_OVERFLOW` é sempre gerado. `FAN_UNLIMITED_QUEUE` exige `CAP_SYS_ADMIN`. Eventos consecutivos do mesmo objeto e processo podem ser fundidos. Nada persiste após reinício | - | O consumidor precisa de um caminho de revarredura completa em dois casos: estouro e reinício. Isso já vale para o índice em RAM do próprio MoobFS, reconstruído na montagem | [texto] `fanotify.7`, `fanotify_init.2` |
| `fanotify` sem privilégio | 5.13 (e 5.10.220) | Permitido sem `CAP_SYS_ADMIN`, com limites: sem fila e marcas ilimitadas, sem eventos de permissão, exige `FAN_REPORT_FID`, só marca inodes (marcas de mount e de filesystem proibidas), sem pid de outros processos. Desde 6.16 (segundo LWN), quem tem privilégio dentro de um user namespace pode marcar superblocos e namespaces de montagem ali | - | Um agente comum pode vigiar seus próprios diretórios, mas um indexador do volume inteiro precisa de privilégio (ou de ser dono do user namespace da montagem, a partir de 6.16) | [texto] `fanotify_init.2` linhas ~513-564; [secundária] https://lwn.net/Articles/1075829/ (Jake Edge, 2026-06-08) |
| `name_to_handle_at` / `open_by_handle_at` | 2.6.39; flags `AT_HANDLE_FID` 6.5, `AT_HANDLE_MNT_ID_UNIQUE` 6.12, `AT_HANDLE_CONNECTABLE` 6.13 | Handle persistente do objeto; pode ficar velho (`ESTALE`) | `open_by_handle_at` exige `CAP_DAC_READ_SEARCH`. Desde o commit 620c266f (Christian Brauner, maio de 2024; presente no v6.12 que li), há caminho alternativo restrito a diretórios (`O_DIRECTORY`), para quem tem `CAP_SYS_ADMIN` no user namespace do superbloco ou da montagem e `CAP_DAC_READ_SEARCH` no próprio namespace; nesse caso `vfs_dentry_acceptable` sobe pelos pais checando se o chamador alcança o objeto, com comentário que admite ser "racy" e caro | Uma autorização em lote por handle não pode usar `open_by_handle_at` para não privilegiados. Ela teria de decodificar o handle dentro da camada e checar o caminho como a fase 3b já faz. O kernel já tem precedente de "subir pelos pais checando alcance" | [texto] `open_by_handle_at.2`; [código] `fs/fhandle.c` v6.12 (`may_decode_fh`, `vfs_dentry_acceptable`); [secundária] commit 620c266f via https://github.com/ctrliq/kernel-src-tree/commit/620c266f394932e5decc4b34683a75dfc59dc2f4 |
| Journal de mudanças persistente para Linux (Amir Goldstein) | 2018-2019 | LSFMM 2018: journal no estilo NTFS para backup; Jan Kara sugeriu evento de "intenção de mudar" registrado antes da mudança. LSFMM 2019: implementação sobre overlayfs em produção (CTERA), pedido de gancho pré-escrita e filtro por subárvore; Ts'o e Kara apontaram complexidade de travamento | - | Sem consenso nem código no mainline até onde li; em 2024-2026 o foco foi eventos pré-conteúdo para HSM (`FAN_PRE_ACCESS` integrado em 2025). Um cursor durável hoje seria código próprio | [secundária] https://lwn.net/Articles/755277/ (2018-06-04); https://lwn.net/Articles/789038/ (2019-05-22); https://lwn.net/Articles/981392/ (2024-07-16) |
| Landlock | 5.13 (ABI 1) a 6.13+ | LSM de sandbox sem privilégio; direitos de FS (`READ_FILE`, `READ_DIR`, `EXECUTE`, `REFER`, `TRUNCATE`, `IOCTL_DEV`...) | Em v6.12 os hooks de FS são `file_open`, `path_*`, `sb_*`, `file_ioctl` etc.; **não** há `inode_permission` nem hook de leitura de xattr. `IOCTL_DEV` só cobre dispositivos | `inode_permission` não consulta o Landlock. Um agente em sandbox Landlock que não pode abrir `/memorias/b` ainda recebe `/memorias/b/x` no `QUERY` se as permissões POSIX permitirem, e também lê os xattrs. A autorização em lote herdaria a mesma lacuna | [código] `security/landlock/fs.c` v6.12 (lista `LSM_HOOK_INIT`), `include/uapi/linux/landlock.h`; [texto parcial] https://docs.kernel.org/6.6/userspace-api/landlock.html |

## 5. Técnicas de execução de consulta para um índice conjuntivo pequeno

Estado atual do MoobFS, verificado em `layer/index.c`: uma rbtree por nome de atributo, com entradas
ordenadas por `(type, value, ino)`; `seek_first` posiciona no início do intervalo do primeiro termo e
`first_after` faz busca por chave completa `(type, value, ino)` em O(log n). Logo, para um termo de
igualdade, as entradas que casam formam uma sequência contígua **já ordenada por inode**, e é possível saltar
dentro dela para o primeiro inode maior ou igual a k em O(log n).

| trabalho | ano | mecanismo | como trata permissões | lição para o MoobFS | fonte |
| --- | --- | --- | --- | --- | --- |
| Interseção de listas ordenadas (Culpepper e Moffat, ACM TOIS 29(1), DOI 10.1145/1877766.1877767) | 2010 | Consulta conjuntiva = interseção k-way de conjuntos ordenados de inteiros; comparam métodos sobre representações comprimidas e não comprimidas; propõem híbrido com bitvectors para listas densas | - | Com duas sequências de igualdade ordenadas por inode, a interseção por saltos custa por volta de O(m log(n/m)), com m o tamanho da menor, em vez de O(tamanho da primeira) | [resumo] https://people.eng.unimelb.edu.au/ammoffat/abstracts/cm10acmtois.html |
| Interseção SIMD e "galloping" (Lemire, Boytsov, Kurz) | 2014-2020 | Galloping (busca exponencial) vetorizado sobre listas comprimidas; dobra a velocidade de interseções conjuntivas | - | O ganho de SIMD não se aplica a rbtree (ponteiros), mas o galloping sim: cada salto é um `first_after` | [resumo] https://arxiv.org/abs/1401.6399 (o periódico, Software: Practice and Experience, não foi confirmado nesta pesquisa) |
| Escolha do termo mais seletivo com estimativa barata (BFS) | 1999- | Score por tamanho do índice e operador; sem estatística por valor | - | Variante mínima para o MoobFS: manter, por nome de atributo, o número de entradas (contador já barato de manter na inserção e remoção) e começar pelo termo cujo índice é menor. Ajuda quando os nomes têm tamanhos muito diferentes; não ajuda em `namespace = X AND agent = Y` se os dois nomes estão em todas as memórias | [código] `QueryParser.h` |
| Chave composta | - | Índice sobre `(atributo1, atributo2)`, como o índice composto do SQLite usado nos pilotos | - | Resolve a forma medida em O(log n + resultados), mas exige declarar quais pares indexar (esquema, memória extra, decisão de formato). É o primeiro passo de um planejador; a ADR-0009 alternativa C já o rejeita | ADR-0009 |
| Signature files / filtros de Bloom (Spyglass) | 2009 | Bit array por partição e atributo; teste negativo descarta a partição; sem falsos negativos | - | Útil quando há partições a pular (por exemplo, por namespace). Num índice único em RAM, a interseção ordenada é exata e mais simples; o filtro só compensaria com índices por domínio | [texto] Spyglass, FAST '09 |
| Bitmaps por valor | - | Um bitmap de inodes por valor; AND de bitmaps | - | Rápido para valores de baixa cardinalidade (namespace, agente), mas custa memória por valor e manutenção a cada `setxattr`; não verificado em nenhuma fonte desta pesquisa para o contexto de kernel | - |

Custo estimado da interseção por saltos no MoobFS (proposta, não implementada, não medida): para dois termos
de igualdade com sequências de tamanhos a e b, o pior caso é cerca de 2·min(a, b) buscas O(log n) na rbtree.
Na forma `namespace = X AND agent = Y` com 10 000 memórias, se cada sequência tem algumas centenas de
entradas, são algumas centenas de buscas em vez de varrer todos os candidatos do primeiro termo. É mais
simples que um planejador: uma regra fixa ("se houver dois ou mais termos `EQ`, intersectar suas
sequências; os demais termos continuam como filtro por candidato"), sem estatísticas, sem alocação e sem
mudança de UAPI. A ordem de saída passaria a ser por inode dentro da interseção, o que muda a semântica do
cursor de retomada (`moobl_resume`) e precisa de especificação.

## Arquiteturas recorrentes

1. **Índice em espaço de usuário + fluxo de mudanças do kernel + revarredura como rede de segurança.**
   Spotlight/FSEvents, Windows Search/USN, Tracker e Baloo com inotify, Robinhood e Brindexer com changelog
   do Lustre, Icicle. O kernel ou o FS fornece "o que mudou desde X"; o índice e o planejamento ficam num
   motor maduro. Todos têm um caminho de revarredura para quando o fluxo falha (ID de journal trocado,
   overflow, leitor perdido).
2. **Particionar o índice pela fronteira de permissão.** GUFI (por diretório, com o dono e o modo do
   diretório), Parker-Wood et al. (partições cientes de segurança), a proposta do Spyglass para excluir
   partições, e os índices por usuário do Tracker e do Baloo. A permissão do arquivo do índice faz o papel
   de "early binding" sem copiar ACLs para dentro do índice.
3. **Restringir antes de calcular.** Büttcher e Clarke e a prática de early binding: nada (escore,
   contagem, faceta, total de páginas) pode ser calculado sobre itens que o chamador não vê. Late binding
   só é seguro se a resposta não expõe agregados.
4. **Índice dentro do armazenamento, com um plano de consulta mínimo.** BFS (um ramo por AND, escolhido por
   score barato) e Magellan. Funciona para um usuário ou administrador; nenhum dos exemplos lidos resolve
   permissões por resultado.

## Implicações para a ADR-0009

**Qual padrão se encaixa.** A ADR-0009 é o padrão 1 com uma exigência a mais, o isolamento entre agentes,
que só o padrão 2 (ou um serviço privilegiado) resolve. A literatura confirma que a parte cara (planejador,
índices compostos, paginação) é feita fora do kernel em todos os casos que vingaram. Nenhum trabalho lido
mantém autorização POSIX por resultado dentro do kernel para um índice externo; nesse ponto o MoobFS não tem
precedente direto.

**Como um esquema no estilo GUFI resolveria o vazamento de metadados.** Proposta para o M4 medir, não
decisão:

- Um banco SQLite por domínio de permissão. No MoobFS o domínio natural é o diretório de namespace (ou do
  agente), que já tem dono e modo próprios. O arquivo do índice fica num diretório com o mesmo dono, grupo,
  modo e, se houver, ACL do diretório indexado. Quem não lê o diretório não abre o índice: o kernel faz a
  autorização na abertura, uma vez, e não por resultado.
- Arquivos com permissões diferentes das do diretório (o caso que o GUFI resolve com bancos por uid e por
  gid) precisam de regra explícita. No MoobFS a regra mais simples é proibir por convenção que uma memória
  tenha permissão mais restrita que seu diretório, ou indexá-la num banco à parte com o dono do arquivo,
  como faz o GUFI.
- Consultas entre domínios: o cliente abre todos os bancos de domínio que consegue abrir e consulta cada um
  (o GUFI faz isso em paralelo, por largura). O número de domínios por agente define o custo.
- Quem escreve os bancos: um indexador privilegiado alimentado por `fanotify` com marca de filesystem
  (exige `CAP_SYS_ADMIN`). Ele **escreve** com privilégio, mas não **responde** consultas: os leitores são
  processos comuns que abrem SQLite com as próprias credenciais. Isso evita o serviço de consulta com
  `SO_PEERCRED` que a ADR-0009 lista como primeira saída, mantendo um processo privilegiado só do lado da
  escrita.
- Limites: ACLs POSIX, idmap e user namespaces precisam ser copiados para os arquivos de índice e mantidos
  quando mudam (`FAN_ATTRIB` no diretório); uma mudança de permissão no diretório tem que chegar ao banco
  antes da próxima consulta, ou o índice vaza durante a janela. Essa janela não existe no `QUERY` atual.
- A autorização em lote do kernel continua útil como segunda barreira (por exemplo, para arquivos dentro do
  domínio com permissão mais restrita), e como único mecanismo se o índice for compartilhado.

**Se um cursor durável se justifica.** Pelas fontes, não como primeiro passo. O índice do próprio MoobFS
já é reconstruído na montagem, então um índice externo pode seguir a mesma regra: revarrer na montagem e
após `FAN_Q_OVERFLOW`, e seguir o `fanotify` no resto do tempo. O Linux não tem journal persistente
genérico, e as tentativas de Goldstein mostram que um journal correto diante de queda exige ganchos de
"intenção de mudar" com problemas de travamento. Um passo intermediário barato, inspirado no ID de journal
do NTFS e na geração do btrfs: expor em `GET_INFO` uma época da montagem (muda a cada montagem e a cada
reconstrução do índice) e um contador monotônico de mudanças de atributos. Com isso o consumidor detecta que
perdeu continuidade e revarre, sem o kernel guardar eventos. É proposta; exige especificação de UAPI. O
custo de revarrer 10 000 ou 100 000 memórias é o número que decide; não foi medido.

**Se a interseção no kernel é uma melhoria pequena e contida.** Sim, no recorte de termos de igualdade, e
vale manter como candidata dentro do "melhorias só de eficiência, sem planejador" da ADR-0009. O índice já
ordena por `(type, value, ino)` e já tem busca por chave completa (`first_after`), então a interseção por
saltos de sequências `EQ` reaproveita o que existe. Ela ataca o caso medido que cresce com o tamanho do
conjunto (34 µs com 1 mil, 1,1 ms com 10 mil). Não resolve o custo por resultado (~1,7 µs, resolução de
caminho), nem intervalos, nem paginação. A escolha de termo por tamanho do índice, no estilo BFS, é ainda
menor, mas não ajuda quando todos os nomes estão em todas as memórias. Não recomendo bitmaps, chaves
compostas nem estatísticas por valor no kernel: aí começa o planejador que a ADR rejeita. Pontos a
especificar antes: ordem de saída, retomada e a interação com o limite de 4096.

**Lacunas de autorização a registrar na ADR, independentes da escolha.** (a) Landlock: `inode_permission`
não o consulta, então o `QUERY` e uma autorização em lote no mesmo molde ignoram sandboxes Landlock (lido no
código v6.12; não testado no MoobFS). Opções: documentar como limite, ou checar também com o hook que o
Landlock usa (`security_file_open` exige um `struct file`, o que é caro por resultado; não avaliado). (b)
Canal de tempo: custo proporcional a candidatos alheios. A interseção reduz a amplitude do sinal, mas não o
elimina. (c) Qualquer índice externo que exponha `COUNT`, total de páginas ou ranking precisa restringir
antes (Büttcher e Clarke).

## O que medir no M4 a partir desta pesquisa

Itens que esta pesquisa acrescenta à lista da ADR-0009, sem substituí-la:

1. Interseção por saltos de termos `EQ` contra o caminho atual, nas nove formas do W3, em 1 mil, 5 mil e
   10 mil memórias, com e sem KASAN.
2. Custo de abrir N bancos SQLite por domínio e consultar cada um (N = 1, 10, 100), no estilo GUFI, contra
   um banco único com autorização em lote.
3. Janela entre `chmod`/`setfacl` num diretório de namespace e a atualização do arquivo de índice do
   domínio pelo indexador.
4. Tempo de revarredura completa do índice externo após `FAN_Q_OVERFLOW` e após remontagem.
5. Amplitude do canal de tempo: diferença de latência do `QUERY` entre zero e 10 mil candidatos alheios
   no primeiro termo, antes e depois da interseção.
6. Reprodução em VM do caso Landlock: agente com ruleset sem `READ_FILE` num namespace alheio, chamando
   `QUERY` e `getxattr`.

## Referências principais (por área)

Busca por metadados:

- Leung, Shao, Bisson, Pasupathy, Miller. Spyglass. USENIX FAST '09.
  https://www.usenix.org/conference/fast-09/spyglass-fast-scalable-metadata-search-large-scale-storage-systems
- Leung, Adams, Miller. Magellan. UCSC-SSRC-09-07, 2009. https://www.ssrc.ucsc.edu/pub/leung-ssrctr09-07.html
- Hua, Jiang, Zhu, Feng, Tian. SmartStore. SC '09.
  https://www.usenix.org/conference/fast-09/smartstore-new-metadata-organization-paradigm-semantic-awareness
- Xu, Jiang, Tian, Huang. Propeller. IEEE ICDCS 2014. DOI 10.1109/ICDCS.2014.46.
- Sim, Kim, Vazhkudai, Vallée, Lim, Butt. TagIt. SC17. DOI 10.1145/3126908.3126929.
- Manno, Lee, Challa, Zheng, Bonnie, Grider, Settlemyer. GUFI. SC22.
  https://sc22.supercomputing.org/proceedings/tech_paper/tech_paper_pages/pap199.html ;
  código https://github.com/mar-file-system/GUFI
- Paul et al. Brindexer. CCGrid 2020. https://par.nsf.gov/servlets/purl/10167824
- Robinhood Policy Engine. https://github.com/cea-hpc/robinhood
- Pan et al. Icicle. arXiv:2604.10295, 2026.

Permissões:

- Büttcher, Clarke. A Security Model for Full-Text File System Search in Multi-User Environments.
  USENIX FAST '05. https://www.usenix.org/conference/fast-05/security-model-full-text-file-system-search-multi-user-environments
- Parker-Wood, Strong, Miller, Long. Security Aware Partitioning for Efficient File System Search.
  IEEE MSST 2010. DOI 10.1109/MSST.2010.5496990.

Integração ao filesystem:

- Giampaolo. Practical File System Design with the Be File System. Morgan Kaufmann, 1998/1999.
- Haiku, `headers/private/file_systems/QueryParser.h` e `src/add-ons/kernel/file_systems/bfs/Query.cpp`.
  https://github.com/haiku/haiku
- Olson. The Design and Implementation of the Inversion File System. USENIX Winter 1993.
- Gifford, Jouvelot, Sheldon, O'Toole. Semantic File Systems. SOSP '91. DOI 10.1145/121132.121138.
- Microsoft. Change Journals; Using the Change Journal Identifier. learn.microsoft.com.
- Apple. File System Events Programming Guide. developer.apple.com.

Kernel Linux:

- Páginas de manual `fanotify(7)`, `fanotify_init(2)`, `fanotify_mark(2)`, `open_by_handle_at(2)`,
  repositório man-pages em git.kernel.org.
- Código v6.12: `include/linux/fsnotify.h`, `fs/xattr.c`, `fs/fhandle.c`, `security/landlock/fs.c`.
- LWN: https://lwn.net/Articles/755277/ , https://lwn.net/Articles/789038/ ,
  https://lwn.net/Articles/981392/ , https://lwn.net/Articles/1075829/

Execução de consultas:

- Culpepper, Moffat. Efficient Set Intersection for Inverted Indexing. ACM TOIS 29(1), 2010.
- Lemire, Boytsov, Kurz. SIMD Compression and the Intersection of Sorted Integers. arXiv:1401.6399.

## O que não foi verificado

- Artigos lidos só pelo resumo ou catálogo: Magellan, SmartStore, Propeller, TagIt, Brindexer, Icicle,
  Parker-Wood et al. (MSST 2010), artigo SC22 do GUFI (o código e a documentação do repositório foram lidos),
  Inversion, Semantic File Systems, Culpepper e Moffat, Lemire et al.
- Livro de Giampaolo sobre o BFS: não lido; as afirmações sobre o BFS vêm do código do Haiku, que é uma
  reimplementação (OpenBFS) e pode diferir do BeOS original.
- Venue de "SIMD compression and the intersection of sorted integers": li só o arXiv; o periódico não foi
  confirmado.
- Spotlight: se o `mds` filtra resultados por usuário e como. Windows Search: como aplica ACLs aos
  resultados. Ambos ficaram como "provável" ou não verificados.
- Tracker/LocalSearch: se usa inotify ou fanotify hoje.
- Lustre: sintaxe exata e modelo de privilégio de `changelog_register`/`changelog_deregister`; vieram de
  fontes secundárias.
- WinFS: razões técnicas do cancelamento; só cobertura de imprensa de 2006.
- Se as propostas de journal persistente de Goldstein tiveram continuação após 2019; a cobertura de 2024 e
  2026 que li não trata disso.
- Unprivileged `fanotify` em user namespaces a partir de 6.16: veio de resumo da LWN, não do código.
- Se o MoobFS gera `FAN_ATTRIB` no inode inferior além do inode da camada, e se a camada propaga eventos do
  inferior: não testado; a ADR-0009 já lista como item do M4.
- Efeito do Landlock sobre o `QUERY`: inferido do código do Landlock em v6.12; não reproduzido em VM.
- Robinhood: não verifiquei se há filtro de resultados por usuário em `rbh-find` ou na interface web.
- Nenhum número de desempenho desta pesquisa foi medido no MoobFS; as estimativas da seção 5 são análise.
