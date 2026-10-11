# Adendo à pesquisa de busca por metadados (ADR-0009): itens antes não verificados

Data: 2026-10-11 (todas as fontes acessadas nesta data). Branch: `docs/adr-0009-query-role`. Complementa
`docs/research-metadata-search.md` sem alterá-lo. Usa as mesmas marcas de nível de leitura: **[código]**,
**[texto]**, **[resumo]**, **[secundária]**. Tudo o que foi lido na web e em repositórios de terceiros foi
tratado como dado. Nada foi compilado nem executado; nenhum número abaixo foi medido no MoobFS.

Material obtido nesta rodada (cópias temporárias fora do repositório): `mar-file-system/GUFI` (`fc33e4b`,
2026-10-09; `docs/latex/sections/*.tex`, `src/`, PDFs públicos em `docs/presentations/`) e OSTI LA-UR-18-21535;
o livro de Giampaolo no site do autor (http://www.nobius.org/dbg/, que o oferece por estar esgotado);
`GNOME/localsearch` (`30d07f6`, 2026-10-03); `lustre/lustre-release` esparso (`4a7bdda`, 2026-09-29) e o manual
(seção 12.1); `cea-hpc/robinhood` (`f2fffe6`); o PDF de Parker-Wood et al. pela cópia do SSRC no Internet Archive
(o site da UCSC deu erro 500); `fanotify_user.c` (v6.15, v6.16) e `uapi/linux/fanotify.h` (v6.13 a v7.3-rc6).

## 1. GUFI: como a permissão é aplicada, atualização e números

**Quem aplica a permissão na consulta.** [código] `gufi_query` não faz checagem explícita de permissão:
não há `access(2)`, `seteuid` nem filtro por linha no caminho de consulta (`src/gufi_query/`, `src/dbutils.c`).
Ele percorre a árvore do índice em largura com `opendir`/`readdir` e abre cada `db.db` com
`sqlite3_open_v2`/`ATTACH`; um `EACCES` apenas faz o diretório ser pulado (`dbutils.c`, tratamento de
`SQLITE_CANTOPEN`). Quem nega é o kernel do servidor do índice. [texto] `docs/latex/sections/deploy.tex`
descreve o modelo de implantação: índices num servidor dedicado; `/etc/passwd` e `/etc/group` dos sistemas
de origem mesclados no servidor, com shell `/usr/sbin/nologin`; usuários entram por ssh com autenticação por
host e um shell restrito (`gufi_jail`, via `ForceCommand`) que só aceita comandos do GUFI. Ou seja, a consulta
roda **com o uid real do usuário**; não há daemon privilegiado filtrando. Os slides de 2023 (LA-UR-23-24629)
justificam: um índice plano com vários uids exigiria "checagem de permissão por linha" ou uso só por admin.

**Travessia e ancestrais.** [código + inferência] Os diretórios do índice recebem o modo, o dono e o grupo do
diretório de origem (`gufi_dir2index.c`, `chmod`/`chown`; `utils.c:dupdir`). Para começar numa subárvore o
usuário precisa de execução nos ancestrais (resolução de caminho normal). Para descer, o `gufi_query` precisa
de `opendir`, isto é, leitura **e** execução em cada diretório do índice: um diretório só com `x` corta a
descida, embora em POSIX os filhos pudessem ser alcançáveis por caminho conhecido. É mais restritivo que o
modelo de Büttcher e Clarke, não menos. As regras de rollup ([texto] `rollup.tex`) também exigem `rx`
compatível (o+rx; ou mesmos u/g e modos; etc.).

**ACLs.** [código] `dupdir` copia só `mode`, `uid` e `gid`; o comentário diz que xattrs do diretório do
índice não são necessários. `xattr_can_rollin` e os modos dos bancos externos olham só `st_mode`. Não
encontrei cópia de `system.posix_acl_*` em `src/`. [texto] O slide de 2026 (LA-UR-26-24285) lista "ACL/MLS"
entre as fontes de controle de acesso, sem detalhe. Conclusão: no código lido, ACLs POSIX não são
replicadas; um arquivo protegido só por ACL fica protegido apenas pelo modo copiado.

**Xattrs com visibilidade diferente.** [código] Só `user.*` é extraído. Os que não podem "rolar para dentro"
(regras em `xattrs.c:xattr_can_rollin`) vão para bancos externos: por uid, dono `uid:nobody`, modo `0600`; por
gid, dono `nobody:gid`, modo `0040` se o arquivo tem `g+r`, senão `0000` (só root lê) (`dbutils.c`, ~linha
970). O usuário anexa os que o kernel deixar abrir.

**Modelo de atualização.** [texto] Não há fluxo de eventos. O slide de 2023 diz que a LANL faz varreduras
completas a cada 4 horas e que a atualização incremental é possível, mas não testada; em 2022 o índice novo
era publicado trocando um symlink. A atualização incremental documentada (`incremental_update.tex`;
LA-UR-26-25065, 2026) varre a árvore de origem inteira, marca diretórios "suspeitos" por timestamp ou por
arquivo de log, regenera só os `db.db` desses diretórios num "parking lot" e aplica um diff CRUD obtido por
`LEFT OUTER JOIN` entre listagens de diretórios (renomes são detectados por inode). A latência de
atualização é, portanto, a de uma varredura (horas em sistemas grandes).

**Escala e desempenho.** [texto] SDC 2019 (LA-UR-19-29542; um servidor AMD Epyc 7401, 512 GB, kernel 3.10,
um NVMe, XFS), "encontrar todos os arquivos" como usuário comum: Lustre com 118.509.899 arquivos e 5,5 M
diretórios em 134,2 s contra 11.309 s com ferramentas POSIX (84x); NFS home com 13,2 M arquivos em 39,2 s
contra 2.040 s (51x). Criação do índice de 118,5 M arquivos do Lustre em 148,9 s. Primeira versão só 4x a 25x;
o ganho veio de abrir bancos mais barato (SQLite com `unix-none` e sem thread-safety). 2023: "Scratch 3"
(2,2 M diretórios, 65 M arquivos), `SELECT uid FROM pentries` com 224 threads levou 15,36 s sem rollup e
~2,8 s com rollup limitado a 100 mil-500 mil entradas; o rollup reduziu os diretórios a percorrer a 0,04%-0,39%
em três índices. Indexar o BeeGFS do TACC (56,6 M diretórios, 16 threads) levou ~35,75 h. Artigo SC22: [resumo]
apenas; o resumo informa 1,5x a 230x sobre o estado da arte em consultas de administrador. O texto completo
(IEEE/ACM, DOI 10.1109/SC41404.2022.00062, artigo 57) não foi encontrado em acesso aberto (OSTI só tem um
pôster de 2022 e os slides de 2018).

**Achado novo.** [texto] Em junho de 2026 a LANL apresentou "Access Controlled AI Context with GUFI"
(LA-UR-26-24285): texto extraído, FTS5 e vetores (`sqlite-vec`) guardados em bancos externos ao lado dos
metadados, para consultas RAG que respeitam as permissões de origem. O slide afirma que a mesma pergunta feita
por pessoas diferentes terá respostas diferentes. Sem números de segurança ou desempenho além de um sistema de
teste com 70 mil arquivos e 7 mil diretórios.

Fontes: https://github.com/mar-file-system/GUFI (arquivos citados);
https://www.osti.gov/biblio/1422987 ; https://laro.lanl.gov/esploro/outputs/conferenceProceeding/GUFI-Fast-Secure-File-System-Metadata/9916458628503761 [resumo].

**Implicação para a ADR-0009.** O GUFI confirma o desenho "o kernel nega a abertura do banco do domínio", mas
com três diferenças que a ADR precisa assumir explicitamente: ele roda a consulta como o próprio usuário num
host dedicado, não replica ACLs e não tem atualização ao vivo (o índice fica horas atrasado). Um índice
alimentado por `fanotify` com cópia de ACLs e janela curta após `chmod` não tem precedente no GUFI.

## 2. Giampaolo, "Practical File System Design with the Be File System"

[texto] Seções 5.2 a 5.4 e 9.3 do PDF do autor.

- **Escolha do índice numa consulta com vários termos** (5.3, p. 94): depois do parse, o FS desce da raiz
  escolhendo o caminho com "menos casamentos"; prefere expressões que usam a B+tree (por exemplo `size >
  20000`) a expressões com regex, e também compara o tamanho dos índices. O autor admite que a lógica é
  "fairly convoluted" e que as heurísticas poderiam melhorar. Os demais termos são avaliados lendo os
  atributos do próprio arquivo, sem usar outros índices. Confirma o que o código do Haiku mostrou.
- **`!=` sozinho varre todos os arquivos** porque arquivos sem o atributo casam (p. 95-96).
- **Permissões nos resultados**: a seção de consultas não menciona checagem de permissão; o resultado é um
  inode convertido em caminho graças ao nome e ao diretório pai guardados no inode (p. 97). A ausência no
  texto é compatível com o "TODO" do Haiku, mas não prova o comportamento do BeOS.
- **Live queries** (p. 98): cada índice citado numa consulta recebe uma etiqueta que aponta para a árvore da
  consulta; a cada modificação do índice o FS percorre as live queries interessadas e testa o arquivo. O autor
  registra "many subtle locking issues" nesse caminho índice-consulta-índice.
- **Custo de manter índices** (5.2 e 9.3): o índice de tamanho só é atualizado no `close`, porque atualizar a
  cada escrita custava um "performance hit" significativo; acima de ~10 mil duplicatas a atualização do
  índice de tamanho fica lenta; remover o índice de tempo de criação deu ~20% de ganho num benchmark de
  criação/remoção; permissões e dono não foram indexados porque o custo superava o benefício. Nos benchmarks,
  BFS sem índices foi quase duas vezes mais rápido que BFS com os três índices padrão (lat_fs e PostMark; 17
  transações/s com índices).

Fonte: http://www.nobius.org/dbg/practical-file-system-design.pdf (via http://www.nobius.org/dbg/).

**Implicação para a ADR-0009.** O próprio autor do BFS mediu manutenção de índice no caminho de escrita
como o dobro do custo de operações de metadados, e adiou atualizações para o `close`. Reforça manter no kernel
só o índice já existente e barato, sem planejador nem índices novos; a escolha de termo por custo estimado é
a única técnica de planejamento que o BFS achou valer a pena.

## 3. Windows Search e Spotlight: filtragem por usuário

**Windows Search** [texto], "Windows Indexing Features" (Windows 7/Server 2008 R2, arquivado): um índice por
computador; o serviço roda como LocalSystem; os arquivos do índice têm ACL só para Administrators e SYSTEM;
"security trimming" garante que cada usuário só busque os próprios arquivos e os compartilhados, conforme as
ACLs de cada arquivo. O documento admite que um administrador consegue ler o índice. O antigo Indexing
Service ([texto], documentação arquivada) executava a consulta no contexto do sistema e depois removia os
resultados que o usuário não podia ler: pós-filtragem por um serviço privilegiado, com a justificativa
explícita de não revelar sequer a existência do documento. O modelo do SharePoint 2007 ([secundária] via
busca) guarda o descritor de segurança no índice e checa por item na consulta.

**Spotlight** [secundária]: um índice por volume (`.Spotlight-V100`, segundo Eclectic Light) e filtragem por
usuário na consulta. A frase "Spotlight respects the boundaries of users' personal files—even though there is
only one index per volume" aparece citada num fórum da Apple em 2006, sem fonte identificada. Não encontrei
documentação da Apple que descreva o mecanismo.

Fontes: https://learn.microsoft.com/en-us/previous-versions/windows/it-pro/windows-7/dd744700(v=ws.10) ;
https://learn.microsoft.com/en-us/previous-versions/windows/desktop/indexsrv/security-summary ;
https://discussions.apple.com/thread/747558 ; https://eclecticlight.co/2021/01/28/spotlight-on-search-how-spotlight-works/

**Implicação para a ADR-0009.** Os dois buscadores de desktop dominantes usam índice compartilhado,
privilegiado e com corte por ACL na consulta, isto é, o "serviço de consulta" que a ADR-0009 lista como
alternativa, e não o particionamento do GUFI. O precedente existe e é maduro, mas concentra o risco num
processo privilegiado.

## 4. WinFS: razões do cancelamento

[texto] Quentin Clark, blog da equipe WinFS, "WinFS Update" (2006-06-23) e "Update to the Update"
(2006-06-26): o WinFS deixou de ser entregue como produto separado; a parte de banco de dados foi para o SQL
Server e o ADO.NET; o restante ficou em "incubação". Perguntado se o WinFS morreu pelo desenho, Clark responde
que não, mas admite várias mudanças internas de desenho e reescritas, e diz que fazer o motor relacional se
comportar e ter desempenho como o filesystem do Windows exigia muito cuidado, com barras de desempenho e
compatibilidade "super high".

[secundária, mas de participante] Hal Berenson, série "WinFS, Integrated/Unified Storage, and Microsoft"
(fev.-mar. 2013), que se apresenta como gerente geral de Integrated Storage na organização do SQL Server na
época do Longhorn: parte 4 atribui a queda à aposta pesada no CLR (removido no "reset" do Longhorn), à falta
de cliente interno comprometido e à falta de caminho de entrega no Windows; parte 3 cita requisitos
conflitantes entre dados estruturados e não estruturados, receio de complexidade no modo kernel e no caminho
de boot, e o choque entre metadados arbitrários e evolução de esquema controlada.

Fontes: https://learn.microsoft.com/en-us/archive/blogs/winfs/winfs-update ;
https://learn.microsoft.com/en-us/archive/blogs/winfs/update-to-the-update ;
https://hal2020.com/2013/02/25/winfs-integratedunified-storage-and-microsoft-part-3/ ;
https://hal2020.com/2013/03/10/winfs-integratedunified-storage-and-microsoft-part-4/

**Implicação para a ADR-0009.** As razões técnicas documentadas são o custo de fazer um banco relacional
atender as exigências de desempenho e compatibilidade de um filesystem, e o acoplamento a uma plataforma
instável; ambas apoiam a decisão de não trazer planejador relacional para o kernel.

## 5. GNOME LocalSearch (antigo tracker-miner-fs)

[código] `localsearch` `30d07f6`:

- **inotify ou fanotify**: os dois. Com `HAVE_FANOTIFY` (opção de build `fanotify`, padrão `auto`),
  `tracker_monitor_new` tenta primeiro `TrackerMonitorFanotify`; se falhar, usa `GFileMonitor` do GLib
  (inotify). Por diretório, se não conseguir criar o handle (por exemplo `EXDEV` no btrfs), cai para o
  monitor do GLib.
- **Modo do fanotify**: sem privilégio: `fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF | FAN_REPORT_DFID_NAME)`
  e marcas de inode por diretório (`FAN_MARK_ADD | FAN_MARK_ONLYDIR`) com `FAN_EVENT_ON_CHILD`; eventos
  incluem `FAN_ATTRIB`, criação, remoção, movimentos e `FAN_CLOSE_WRITE`. Limita-se a 80% de
  `/proc/sys/fs/fanotify/max_user_marks`; acima disso, diretórios novos deixam de ser vigiados e só um
  contador é incrementado.
- **Estouro**: não encontrei tratamento de `FAN_Q_OVERFLOW` no código do monitor; pela leitura, um evento de
  estouro (sem registro `DFID_NAME`) é simplesmente pulado. Não verifiquei se outra camada percebe a perda.
- **Consistência no início**: o notificador compara `mtime` guardado e `mtime` em disco ao varrer as raízes
  (`tracker-file-notifier.c`).
- **Permissões**: roda como serviço systemd de usuário (`ConditionUser=!@system`,
  `XDG_SESSION_CLASS=user`), logo indexa só o que o usuário lê, num índice por usuário.

Fonte: https://gitlab.gnome.org/GNOME/localsearch (`src/indexer/tracker-monitor.c`,
`tracker-monitor-fanotify.c`, `tracker-file-notifier.c`, `tracker-miner-fs.service.in`, `meson_options.txt`).

**Implicação para a ADR-0009.** Há precedente em produção de `fanotify` sem privilégio com marcas por
diretório e `DFID_NAME`, exatamente o modo disponível a um agente comum; mas ele esbarra no limite de marcas
e não trata estouro. Um indexador do MoobFS deve tratar `FAN_Q_OVERFLOW` com revarredura e expor quando
parou de vigiar algo.

## 6. Changelogs do Lustre e Robinhood

[texto] Manual, seção 12.1: o administrador registra leitores no MDS (`lctl --device <MDT> changelog_register
--user NOME --mask TIPOS`); o sistema expurga até o maior registro processado por todos; registros não são
expurgados além do ponto de um leitor registrado; `lfs changelog` lê e `lfs changelog_clear <mdt> <id>
<endrec>` confirma; `changelog_deregister` equivale a um `clear` total. O manual descreve o changelog como
centralizado e "designed to be transactional", e o oferece para auditoria (com `OPEN`, `GXATR` e `NOPEN` fora
da máscara padrão), com controle por nodemap.

[código] `lustre/mdt/mdt_internal.h:mdt_changelog_allow` é exigido tanto para abrir o changelog
(`tgt_llog_open`) quanto para `CHANGELOG_CLEAR`: o chamador precisa de uid 0, gid 0, `CAP_SYS_ADMIN` e do
papel RBAC `chlg_ops` do nodemap. Logo, ler e confirmar são operações de root. `mdd` tem coleta de leitores
ociosos ligada por padrão (`mdd_changelog_gc = 1`), com limite de ociosidade de 30 dias
(`CHLOG_MAX_IDLE_TIME = 2592000`), limite de índices ociosos e intervalo mínimo de 1 h entre coletas. Não li
a função que efetivamente remove o leitor.

[código] Robinhood, `src/chglog_reader/`: só chama `llapi_changelog_clear` no callback disparado depois que a
operação foi gravada no banco; agrupa confirmações (`batch_ack_count = 1024` por padrão, ou ao alcançar o
último registro enviado); grava no próprio banco o último registro aplicado a cada 10 mil registros ou 5 s
(`commit_update_max_delta`, `commit_update_max_delay`) para não reprocessar após reinício.

Fontes: https://doc.lustre.org/lustre_manual.xhtml (seção 12.1); https://github.com/lustre/lustre-release ;
https://github.com/cea-hpc/robinhood

**Implicação para a ADR-0009.** O cursor durável que funciona em produção tem três peças: leitor
privilegiado registrado, confirmação explícita depois de aplicar, e coleta de leitores esquecidos. Nenhuma
delas filtra eventos por usuário; um fluxo de mudanças do MoobFS lido por agentes comuns seria algo novo e
precisaria de regra de visibilidade própria.

## 7. Parker-Wood, Strong, Miller e Long, MSST 2010

[texto] Texto completo (14 p.). O algoritmo percorre a árvore em largura e abre uma partição nova sempre que
as permissões do diretório ou arquivo são mais restritivas que as da partição atual, ou quando muda o usuário
ou grupo; assim, quem acessa um arquivo de uma partição acessa todos. Usa só permissões UNIX; ACLs ficam como
trabalho futuro. Custo O(n) em tempo e O(log n) de memória (pilha de permissões do caminho). Avaliação em
quatro coletas anonimizadas de metadados (SOE/UCSC 6,9 M arquivos; NetApp web 15,6 M, eng 60,4 M, home 268,5
M), com resultados mostrados para SOE e web. A comparação com seis métodos usa tamanho de partição, entropia e
ganho de informação; **não mede latência de consulta** num sistema real (protótipo sobre Ceph é trabalho
futuro). No SOE, a partição por segurança gerou 29.479 partições com média de 234 entradas (desvio 3.309),
contra 81 partições de ~85 mil no particionamento guloso por DFS (Tabela 5; leitura da tabela extraída do
PDF, conferida pelo produto partições × média ≈ 6,9 M).

Fonte: https://www.crss.ucsc.edu/pub/parkerwood-msst10.html (erro 500 nesta data); PDF via
http://web.archive.org/web/20231216195950/https://www.ssrc.ucsc.edu/media/pubs/5503961c7ba8d36fed0d9146a9cb9276ad93f459.pdf

**Implicação para a ADR-0009.** Em árvores reais, fronteiras de permissão geram muitas partições pequenas.
Se o MoobFS criar um banco por domínio de permissão, o número de domínios por agente pode ser grande; o
rollup do GUFI é a resposta conhecida, e o item 2 da lista de medições do M4 (N bancos) fica mais importante.

## 8. `fanotify` sem privilégio dentro de user namespaces

[código] Commit `58f5fbeb367f`, "fanotify: support watching filesystems and mounts inside userns" (Amir
Goldstein, 2025-05-16; aplicado por Jan Kara), presente em v6.16 e ausente em v6.15. Marcas de filesystem,
de montagem e de mntns passam a ser aceitas se o usuário tem `CAP_SYS_ADMIN` no user namespace onde o grupo
foi criado **e** no user namespace do superbloco (`s_user_ns`) ou da mntns. O `fanotify_init` continua usando
`capable()` (namespace inicial): um grupo criado dentro do namespace ainda recebe `FANOTIFY_UNPRIV`, isto é,
fila e marcas limitadas e eventos obrigatoriamente com FID ou id de montagem.

Fonte: https://github.com/torvalds/linux/commit/58f5fbeb367f ; `fs/notify/fanotify/fanotify_user.c` v6.15 e
v6.16. Corrige a origem da afirmação do documento principal, que vinha da LWN.

**Implicação para a ADR-0009.** Só ajuda se o filesystem foi montado dentro do user namespace do indexador
(`s_user_ns` igual). Para um volume MoobFS montado pelo host, o indexador de volume inteiro continua
precisando de `CAP_SYS_ADMIN` no namespace inicial.

## 9. Trabalho de Goldstein: eventos pré-conteúdo, HSM e journal

[código] `FAN_PRE_ACCESS` foi introduzido por `4f8afa33817a` (Goldstein, 2024-11-15) e entrou pelo merge
`fsnotify_hsm_for_v6.14-rc1` (`8883957b3c9d`, 2025-01-23); o cabeçalho UAPI o mostra a partir de v6.14 e não
em v6.13. Houve recuos: o evento pré-conteúdo em page fault foi revertido (`955fbe0ef19d`, 2025-03-12) e
eventos pré-conteúdo e de permissão passaram a vir desligados por padrão para pseudo-arquivos
(`711f9b8fbe4f`, 2025-02-03). `FAN_PRE_MODIFY` não aparece no cabeçalho UAPI de nenhuma versão de v6.13 a
v7.3-rc6. [secundária] A LWN (2024) descreve um branch com `FAN_PRE_ACCESS` e `FAN_PRE_MODIFY`; mensagens de
2025 citam um branch "fan_pre_modify-wip" e um RFC de eventos de HSM para diretórios. Não encontrei nada
mesclado que seja um journal persistente de mudanças.

Fontes: https://github.com/torvalds/linux/commit/4f8afa33817a ; https://github.com/torvalds/linux/commit/8883957b3c9d ;
https://lwn.net/Articles/981392/

**Implicação para a ADR-0009.** O que entrou no mainline serve a HSM (buscar conteúdo antes do acesso), não
a registrar mudanças de forma durável. Um cursor durável para o MoobFS continua sendo código próprio; a
proposta de época e contador em `GET_INFO` segue sendo o passo mais barato.

## Correções ao documento principal

- Item 8 do resumo e linha "sem privilégio" da seção 4: a mudança de 6.16 está confirmada no código, mas
  exige `CAP_SYS_ADMIN` também no `s_user_ns` do superbloco; "ser dono do user namespace da montagem" deve ser
  lido nesse sentido.
- Linha do Tracker na seção 3: o LocalSearch usa `fanotify` sem privilégio quando disponível e inotify como
  alternativa.
- Linha dos changelogs do Lustre: ler e confirmar exigem root com `CAP_SYS_ADMIN` e papel de nodemap.
- Linha do Windows Search: o filtro por usuário está documentado (corte por ACL num serviço LocalSystem).
- Linha do GUFI: "atualização incremental por varredura comparada" está certa, mas a prática da LANL
  documentada em 2023 é varredura completa a cada 4 horas; ACLs não são replicadas no código lido.

## Ainda não verificado

- Texto completo do artigo SC22 do GUFI: sem cópia aberta encontrada (IEEE/ACM); só resumo e slides da LANL.
  Os números acima vêm de slides, não do artigo revisado.
- Se alguma implantação do GUFI replica ACLs ou rótulos MLS: o slide de 2026 menciona, o código lido não faz.
- Spotlight: mecanismo de filtragem por usuário sem documentação primária da Apple; só fórum e blogs.
- Windows Search em Windows 10/11: a documentação lida é da era Windows 7; não confirmei que o modelo continua.
- LocalSearch: se outra camada detecta perda de eventos após estouro; só li o monitor.
- Lustre: a função que remove leitores ociosos não foi lida; o efeito exato do GC (desregistrar ou só
  confirmar) é inferido dos nomes e do manual.
- Robinhood: se `rbh-find` ou a interface web filtram por usuário (continua não verificado).
- Livro de Giampaolo: não li o capítulo de API (se a chamada de consulta do BeOS checava permissões fora do
  FS); a afirmação sobre permissões é por ausência na seção 5.3.
- Berenson: os resumos das partes 3 e 4 vieram de leitura assistida da página; convém reler o original antes
  de citar trechos.
- LSFMM 2025 e 2026: não encontrei relato de sessão sobre journal persistente ou `FAN_PRE_MODIFY`.
