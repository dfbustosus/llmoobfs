# Estratégia de distribuição e operação do MoobFS

Estado: **proposta** (branch `m3/3b-query-authz`). Nada marcado FUTURO ou DEPENDE DO M4 está
implementado. Não altera ADRs, formato, UAPI nem código. Base: ADR-0001, ADR-0007, specs M2/M3,
`docs/security-roadmap.md`, registros `docs/history/0015` a `0020`.

Legenda:

- **EXISTE**: está no repositório e foi exercitado pela suíte descrita em `docs/testing.md`.
- **FUTURO**: proposta; depende só de trabalho de engenharia, sem medição nova.
- **DEPENDE DO M4**: só se decide depois dos números de `docs/benchmark-plan.md`.

## 0. Fatos que limitam o desenho

| Fato | Fonte | Consequência operacional |
| --- | --- | --- |
| Módulo nativo empilhado sobre ext4, btrfs ou XFS; tipo de filesystem `moobfs_layer` | `layer/main.c`, ADR-0007 | O administrador monta com `mount -t moobfs_layer -o lower=<dir> none <ponto>` |
| Testado apenas com o kernel Debian 6.12.111+deb13-amd64 (suíte em QEMU) e o kernel upstream 6.12.112 com KASAN/UBSAN/lockdep (workflow sob demanda e semanal) | `vm/Dockerfile`, `.github/workflows/kasan.yml` | Nenhuma outra versão de kernel é suportada |
| Ferramentas de espaço de usuário: `moobfs-attr`, `moobfs-check`, `moobfs-index` (mais binários de teste); `mkfs.moobfs` e `fsck.moobfs` são do M1, congelado | `tools/`, `Makefile` | Não há ferramenta que emita `QUERY` para uso geral; `tests/query.c` é binário de teste |
| `QUERY` e `GET_INFO` exigem descritor aberto da **raiz** da montagem (`file->f_path.dentry == sb->s_root`, senão `ENOTTY`) | `layer/index.c` (`moobl_root_ioctl`) | Vale para qualquer cliente, inclusive em contêiner |
| Fase 3b: `QUERY` resolve cada caminho com as credenciais do chamador e aplica `inode_permission(MAY_READ)`; omite em silêncio o inacessível; `truncated` conta só o visível | `layer/index.c`, `docs/spec-m3.md` §3.1, registro 0020 | O isolamento entre agentes é o do POSIX do inferior, inclusive ACLs |
| `FS_USERNS_MOUNT` e `FS_ALLOW_IDMAP` não definidos | `layer/main.c`, registro 0020 | Montar exige `CAP_SYS_ADMIN` no user namespace inicial; mount idmapped é recusado pelo VFS com `EINVAL` |
| Nada preso (desmontado, o inferior é árvore comum com xattr); o inferior não pode ser modificado com a camada montada (regra do overlayfs); identidade é convenção do cliente `(namespace, memory_id)` | ADR-0007, `docs/spec-m3.md` §2 | Backup, recuperação e provisionamento se apoiam nisso; o kernel não valida unicidade |
| Índice em RAM reconstruído a cada montagem dentro de `index_budget`; sem persistência (decisão do M4) | `layer/index.c`, `docs/spec-m2.md` | Tempo de montagem cresce com o número de inodes tipados |

## 1. Arquitetura de implantação recomendada

Três camadas. Cada uma pode ser adotada sem a seguinte; nenhuma impõe daemon.

```text
Integração   agente: grava arquivos + define atributos tipados + QUERY (ioctl) + GET_INFO
Workspace    espaço de usuário: diretórios, namespaces lógicos, donos, modos, ACLs
Core         módulo moobfs_layer: instalação, compatibilidade de kernel, mount, diagnóstico, update
-----------  ext4 / btrfs / XFS (inferior) em disco
```

### 1.1 Core

| Item | Estado | Detalhe |
| --- | --- | --- |
| Compilar (`make layer KDIR=...`), `insmod`/`mount -t moobfs_layer` (só em VM de teste; `MODULE_ALIAS_FS` permite `modprobe`) | EXISTE | |
| DKMS, assinatura de módulo e Secure Boot | FUTURO | seção 6 (S10) |
| Diagnóstico por `GET_INFO` (caps, flags, orçamento e uso do índice, tipo do inferior) | EXISTE | ioctl; sem ferramenta de linha de comando de uso geral |
| Diagnóstico offline do inferior (`moobfs-check`, `moobfs-index` como oráculo) | EXISTE | `moobfs-check` sai com 0 limpo, 2 problemas, 1 erro |

Regra do Core (FUTURO): carregar só em hosts cujo `uname -r` está na matriz testada; fora dela,
DKMS e `moobfs mount` recusam.

### 1.2 Workspace

Camada de espaço de usuário que o módulo deliberadamente **não** tem (`docs/spec-m3.md` §3).

| Responsabilidade | Mecanismo | Estado |
| --- | --- | --- |
| Criar diretório de namespace e subdiretórios (`shared/`, `agents/<agent>/`) | `mkdir`, `chown`, `chmod`, `setfacl` pela camada | EXISTE como comandos POSIX; ACLs funcionam pela camada desde `d0c8911`; nenhuma ferramenta os encapsula |
| Conceder e revogar acesso | `setfacl -m`/`-x` e `usermod -aG`, encapsulados | FUTURO |
| Auditar permissões e identidades | `moobfs-check` (identidade e atributos) mais varredura de modos | EXISTE (identidade/atributos); FUTURO (modos) |

### 1.3 Integração (contrato mínimo para clientes de agentes)

O contrato é o que `tests/` e a convenção do cliente de memória já exercitam. Nenhum servidor.

1. **Gravar** o conteúdo como arquivo comum dentro do diretório do agente (`write`, `fsync`,
   `rename` atômico a partir de arquivo temporário no mesmo diretório). EXISTE.
2. **Definir atributos tipados** `user.moob.*` pela API de xattr (`setxattr`, ou
   `moobfs-attr set`). O kernel valida tipo e tamanho e recusa codificação inválida. Atributos
   mínimos da convenção M3: `namespace`, `memory_id` (recomendado UUID v7), `agent`; proveniência
   opcional `origin_namespace`, `origin_id`. EXISTE (validação); convenção documentada em
   `docs/spec-m3.md` §2.
3. **Consultar** com `open(<raiz da montagem>, O_RDONLY|O_DIRECTORY)` e `ioctl(MOOB_IOC_QUERY)`
   (até 4 termos, operadores `EQ/LT/LE/GT/GE/PREFIX/EXISTS`, `limit` até 4.096). O resultado é
   caminho relativo à raiz; o cliente abre o arquivo normalmente e aceita `EACCES`/`ENOENT`
   (contrato TOCTOU, `docs/spec-m3.md` §3.1). EXISTE.
4. **Descobrir** capacidades com `MOOB_IOC_GET_INFO` antes de depender de algo (seção 2.8). EXISTE.
5. Sem ioctl o cliente segue funcional (arquivos, xattr, varredura): nada preso. Biblioteca de
   cliente que empacote os passos 2 a 4: FUTURO; `include/moobfs_ioctl.h` e `moobfs_attr.h` já
   são compartilhados com o módulo (campos de largura fixa).

## 2. Perguntas operacionais

### 2.1 Um mount por host ou um mount por agente

| Critério | Um mount por host (por inferior) | Um mount por agente |
| --- | --- | --- |
| Isolamento | POSIX por diretório; `QUERY` filtrado pelo chamador (fase 3b, `ISOLATION_PASS` em ext4/btrfs/XFS) | acrescenta isolamento por caminho: o agente só vê seu `lower=` |
| Memória do índice | um índice, um `index_budget` (padrão 128 MiB) | `N` orçamentos somados; nada é compartilhado entre montagens |
| Reconstrução | uma varredura do inferior inteiro por montagem | varreduras menores e independentes |
| `QUERY` | varre candidatos de todos e descarta os inacessíveis (lotes de 256, uma resolução de caminho por candidato) | só candidatos do agente |
| Administração | um `fstab`, um `moobfs-check` | `N` de cada |
| Contêineres | `QUERY` exige a raiz (2.5) | cada contêiner recebe a raiz do seu superbloco |

**Recomendação (EXISTE como configuração):** um mount por host por filesystem inferior,
namespaces como diretórios (`/<ns>/shared/`, `/<ns>/agents/<agent>/`), um uid/grupo por agente
e `nolinks`. Menos memória, um ponto de administração, compartilhamento por permissão. O custo
de varrer candidatos alheios no `QUERY` não foi medido: **DEPENDE DO M4** (W3).
**Mount por agente faz sentido** quando: o agente roda em contêiner e precisa de `QUERY`;
agentes de confiança muito diferente (um erro de ACL não pode expor o outro); é preciso
remontar, restaurar ou apagar um agente isoladamente; há `index_budget` ou `noindex`
distintos; ou os inferiores diferem (btrfs com snapshots versus ext4). Não há testes com
dezenas de montagens nem dados de escala.

### 2.2 Diretórios privados e compartilhados

Criar com a camada montada (ou desmontada). Comandos POSIX EXISTEM; automação é FUTURO.

| Necessidade | Dono:grupo | Modo / ACL |
| --- | --- | --- |
| Raiz da montagem | `root:root` | `0755` (o ioctl exige abrir a raiz; `0711`/`0700` bloqueiam o agente, achado do registro 0020) |
| Diretório do namespace `/<ns>` | `root:moob-<ns>` | `0750` |
| Privada do agente `/<ns>/agents/<a>` | `<a>:<a>` | `0700`, arquivos `0600` |
| Privada com auditor, ou compartilhada só leitura (`2750`) | idem | `0700` + `setfacl -m u:auditor:rX` e `-d -m u:auditor:rX` |
| Compartilhada com escrita `/<ns>/shared` | `root:moob-<ns>` | `2770` (setgid: novos arquivos herdam o grupo) + `setfacl -d -m g:moob-<ns>:rwX` |

Fatos: todo diretório do caminho precisa de `x` para o chamador, senão o resultado do `QUERY`
é omitido; mudar `user.moob.namespace`/`agent` não concede acesso; `chmod`/`setfacl` valem no
próximo `QUERY`; ACLs POSIX atravessam a camada desde `d0c8911`; renomear diretório deixa
caminhos obsoletos até remontar (seção 7); `nolinks` evita hard links entre agentes.

### 2.3 Impedir que agentes contornem a camada pelo diretório inferior

Quem escreve no inferior por fora ignora a validação de atributos e o filtro do `QUERY` e
viola a regra "inferior imutável com a camada montada". Medidas, da mais simples à mais forte:

1. `lower=` em `/var/lib/moobfs/<nome>` com `root:root 0700` (todos os componentes sem `x` para outros).
2. Montagem em `/srv/memory`, inferior em caminho que o agente não alcança.
3. Separação de mount namespace: agentes iniciam em outro namespace (`unshare -m` ou contêiner)
   onde só o ponto da camada existe (por bind mount), com propagação `private`/`slave` para o
   inferior não vazar.
4. Agentes sem `CAP_DAC_OVERRIDE`, `CAP_DAC_READ_SEARCH`, `CAP_SYS_ADMIN` (`--cap-drop=ALL`, uid não root);
   perfil AppArmor/SELinux para o caminho do inferior é opcional (FUTURO).

**Limite honesto:** root, `CAP_SYS_ADMIN`, `CAP_DAC_OVERRIDE` ou acesso ao dispositivo sempre
contornam. A camada não é fronteira contra root e não promete confidencialidade além do
inferior (S1); para isso, dm-crypt/fscrypt no inferior. Modificação indevida do inferior só é
detectada na remontagem ou por `moobfs-check`, nunca em tempo real.

### 2.4 Identidade e proveniência ao compartilhar ou clonar

Chave `(namespace, memory_id)`; conteúdo igual não é identidade igual; reflink é
compartilhamento físico (`docs/spec-m3.md` §2).

| Operação | Efeito | Ação do cliente |
| --- | --- | --- |
| Compartilhar no mesmo namespace | nenhuma cópia | conceder ACL ao diretório |
| `cp --reflink=auto` ou `FICLONE` (reflink só em btrfs/XFS; ext4 copia) | novo inode; xattr só se a ferramenta o copiar (`cp -a`) | novo `memory_id`, `namespace` de destino, `origin_namespace`, `origin_id` do original |
| `ln` (hard link) | mesmo inode e atributos | não usar para identidades distintas; `nolinks` impede |
| `rename` | identidade preservada | nada |

Receita manual (grava sob nome temporário e só então faz `rename`, porque entre `cp` e a troca
do `memory_id` há dois inodes com a mesma chave, exatamente o erro de `moobfs-check`):

```sh
cp --reflink=auto -p ns-a/shared/x.md ns-b/agents/b/.x.tmp
moobfs-attr set ns-b/agents/b/.x.tmp origin_namespace string ns-a
moobfs-attr set ns-b/agents/b/.x.tmp origin_id string "<memory_id do original>"
moobfs-attr set ns-b/agents/b/.x.tmp memory_id string "$(uuidgen)"
moobfs-attr set ns-b/agents/b/.x.tmp namespace string ns-b
mv ns-b/agents/b/.x.tmp ns-b/agents/b/x.md
```

`moobfs-check` classifica: normal (mesmo id em namespaces distintos), aviso (mesmo id e
conteúdo idêntico sem proveniência), erro (dois inodes com a mesma chave). EXISTE (verificação);
`moobfs memory clone` é FUTURO.

### 2.5 Subdiretórios para contêineres com bind mount

Restrições do código: sem mount idmapped (`mount_setattr` com idmap dá `EINVAL`; sem
`FS_ALLOW_IDMAP`) e sem montar dentro de user namespace (sem `FS_USERNS_MOUNT`; exige
`CAP_SYS_ADMIN` no namespace inicial). O contêiner nunca monta a camada; recebe bind mount de
algo montado no host.

Padrão: (1) montar a camada no host; (2) bind-mount, leitura e escrita, apenas do diretório do
agente (`/srv/memory/ns-a/agents/a1`); (3) **alinhar uids**: o processo roda com o mesmo
uid/gid numérico dono dos arquivos (`--user 20001:20001`, sem userns do runtime ou com
mapeamento 1:1), pois não há idmap para corrigir; (4) leitura, escrita e `setxattr` funcionam e
o kernel valida a codificação.

**Limitação, dita sem rodeios:** `QUERY` e `GET_INFO` só aceitam descritor cujo dentry é a
raiz do superbloco (`ENOTTY` caso contrário). Um bind mount de subdiretório **não pode emitir
`QUERY`**. Alternativas (nenhuma implementada):

| Alternativa | Custo / risco | Estado |
| --- | --- | --- |
| Bind-mount adicional da **raiz inteira** em leitura, ao lado do subdiretório gravável | resultados continuam filtrados pelas credenciais do agente, mas a raiz fica navegável conforme o POSIX; o ioctl não verifica `MNT_READONLY`, porém **não foi testado** em bind `ro` | FUTURO, a validar |
| Um mount por agente, com a raiz dele montada no contêiner | mais memória e administração | EXISTE como configuração |
| Helper fora do contêiner que abre a raiz e emite `QUERY`, entregando o resultado por socket Unix local ou descritor herdado (sem rede, sem daemon HTTP) | precisa assumir o uid do agente antes do ioctl, senão consulta com outras credenciais (confused deputy) | FUTURO, desenho não detalhado |
| Cliente varre árvore e xattr (`find` + `moobfs-attr list`) | perde o índice; custo linear | EXISTE |

Contêiner em userns mapeado: não testado.

### 2.6 Instalação em distribuições

| Aspecto | Proposta | Estado |
| --- | --- | --- |
| DKMS | fontes de `layer/`, `include/`, `common/attr.c` em `/usr/src/moobfs-<v>/`; `dkms.conf` com módulo `moobfs_layer`, `AUTOINSTALL=yes` | FUTURO |
| Política de kernel | instalar só se `uname -r` está na matriz (seção 6); fora dela, `BUILD_EXCLUSIVE_KERNEL` e mensagem. A API de VFS muda entre séries (o módulo usa `backing_file_*`, `fs_context`, `mnt_idmap` do 6.12) | FUTURO |

### 2.7 Atualizar, desmontar, fazer backup e recuperar sem perda

O inferior é a fonte de verdade; o único formato próprio é a codificação versionada do xattr.

- **Desmontar:** `umount`; nada próprio é gravado, o índice some. Durabilidade é a do inferior
  e do `fsync` do cliente (validada com `dm-log-writes` e `dm-flakey`). EXISTE.
- **Atualizar o módulo:** parar agentes, `sync`, `umount`, `rmmod moobfs_layer`, instalar o
  novo, `modprobe`, `moobfs-check <lower>`, montar, conferir `GET_INFO`. Reverter é o caminho
  inverso; o módulo recusa montar se `user.moob.format` for de versão maior que a suportada.
  Mudança de formato exige especificação, testes de entradas inválidas e decisão de compatibilidade.
- **Backup:** `rsync -aHAX --numeric-ids <lower>/ destino/` (`-X` é obrigatório: sem ele os
  `user.moob.*` se perdem; `-A` preserva ACLs). `git` versiona conteúdo, **não** xattr
  arbitrários; usar com exportação dos atributos (FUTURO). Snapshot read-only de btrfs do
  inferior não o modifica e é consistente por subvolume. Imagem de dispositivo só desmontado.
- **Restaurar:** copiar para diretório novo, `moobfs-check`, montar, comparar `QUERY` com o
  oráculo `moobfs-index`. Ferramentas EXISTEM; roteiro automatizado é FUTURO.
- **Recuperar:** após queda, `fsck` do inferior, `moobfs-check`, remontar (o índice é
  reconstruído). Atributo inválido: `moobfs-check` lista caminho e motivo (saída 2), conserto
  manual com `moobfs-attr`; não repara. Sem módulo: montar o inferior diretamente
  (`grep`, `getfattr`, `git`, `rsync` funcionam); escritas diretas não passam pela validação,
  então rodar `moobfs-check` antes de remontar. Índice estourado (`MOOB_FLAG_INDEX_INCOMPLETE`):
  aumentar `index_budget` ou usar `noindex`.

### 2.8 Descoberta de capacidades por `GET_INFO`

Primeira chamada de um cliente (ioctl na raiz, EXISTE; sem ferramenta de linha de comando).
| Campo | Significado / uso |
| --- | --- |
| `uapi_version`, `format_version`, `attr_*_max` | recusar versão desconhecida; gravar atributos nesta versão e validar limites no cliente |
| `caps` REFLINK / DATA_CHECKSUMS / SNAPSHOTS | anunciados pelo **tipo** do inferior (btrfs: os três; xfs, ocfs2, bcachefs: reflink), não por sonda; XFS sem `reflink=1` pode anunciar o que não tem, então tratar `EOPNOTSUPP` |
| `caps` XATTR | inferior aceita xattr; sem isso atributos tipados não funcionam |
| `flags` NOLINKS / INDEX_OFF | não tentar `ln`; com `noindex`, varrer em vez de `QUERY` |
| `flags` PATHS_STALE | esperar falsos negativos no `QUERY` até remontar |
| `flags` INDEX_INCOMPLETE | orçamento atingido; `QUERY` pode omitir |
| `index_budget`, `index_used`, `index_entries`, `index_inodes`, `lower_fs` | monitorar ocupação (alertar perto do orçamento), registrar o inferior |

### 2.9 O que existe, o que falta, o que depende do M4

| Tema | EXISTE | FUTURO | DEPENDE DO M4 |
| --- | --- | --- | --- |
| Passagem POSIX, atributos tipados, ACLs | sim | biblioteca de cliente | — |
| `QUERY` autorizado pelo chamador | sim (VM Debian) | KASAN na branch; `chmod`/`rename` concorrentes | custo por candidato (W3) |
| `moobfs-check` com identidade | sim | verificar modos e ACLs | — |
| DKMS, pacotes, assinatura, CLI `moobfs` | não | sim | — |
| `QUERY` em contêiner com subdiretório; persistência do índice | não | raiz `ro` ou helper | persistência: M4 |
| `QUERY_BATCH`, `READ_BATCH`, cursor; escala suportada | não; sem números | — | M4 (decisão) |

## 3. Experiência CLI proposta (FUTURO)

Um único executável `moobfs`, sem daemon, sem estado além de um manifesto de texto
(`/etc/moobfs/moobfs.conf`); cada subcomando é *wrapper* fino de ferramentas ou syscalls
existentes (`moobfs attr ...` repassa a `moobfs-attr`). Nada é implementado hoje.

| Subcomando | Faz | Construído sobre | Estado |
| --- | --- | --- | --- |
| `moobfs mount <lower> <ponto> [--nolinks] [--noindex] [--index-budget 256M]` | verifica kernel contra a matriz, verifica `lower` (modo, dono, não é outra camada), `mount -t moobfs_layer` | `mount(8)`, `uname`, matriz | FUTURO |
| `moobfs status [ponto]` | imprime `GET_INFO` (caps, flags, orçamento e uso) e o módulo carregado/versão | `MOOB_IOC_GET_INFO` | FUTURO |
| `moobfs workspace create <ns> [--agents a1,a2] [--shared]` | cria `/<ns>/shared`, `/<ns>/agents/<a>`, grupos, modos, ACLs padrão | `mkdir`, `chown`, `chmod`, `setfacl`, `groupadd` | FUTURO |
| `moobfs check <lower\|ponto>` (wrapper de `moobfs-check`) | invoca verificações | `moobfs-check` | FUTURO |
| `moobfs workspace grant\|revoke <ns> <agente\|usuario> [ro\|rw]`; `workspace list` | `setfacl -m`/`-x` (com `-d`); lista namespaces, donos, ACLs e contagem de memórias (`QUERY` `EXISTS memory_id`) | ACL POSIX, `getfacl`, `MOOB_IOC_QUERY` | FUTURO |
| `moobfs query <ponto> --where namespace=ns-a --where tags~prefix ...` | abre a raiz, emite `QUERY` com as credenciais de quem executa, imprime caminhos (JSON linha a linha) | `MOOB_IOC_QUERY`, `tests/query.c` como base | FUTURO |
| `moobfs memory clone <origem> <destino-ns>` | seção 2.4, com troca atômica | `cp --reflink`, `moobfs-attr`, `rename` | FUTURO |

Princípios: saída `--json`, códigos de saída como os de `moobfs-check` (0 limpo, 2 problemas,
1 erro), nada é sobrescrito, nada destrutivo sem `--yes`, nenhuma rede.

## 4. Fluxo completo (alvo; hoje só os passos 2 e 3 manuais em VM são EXISTE)

Cenário: host Debian com kernel da matriz, inferior ext4 em `/dev/vdb`.
1. **Instalar (FUTURO):** `apt install moobfs-dkms moobfs-tools`; DKMS compila para o kernel em
   uso e recusa kernel fora da matriz; com Secure Boot, assinar com MOK. Hoje: `make layer KDIR=...`, `insmod`.
2. **Inferior (EXISTE, POSIX):** `mkfs.ext4 /dev/vdb`, montar em `/var/lib/moobfs/store`, `chmod 0700`.
3. **Montar (EXISTE; `moobfs mount` FUTURO):**
   `mount -t moobfs_layer -o lower=/var/lib/moobfs/store,nolinks,index_budget=256M none /srv/memory`;
   `chmod 0755 /srv/memory`; conferir `/proc/mounts` e `GET_INFO`.
4. **Workspace (FUTURO; hoje manual):** `moobfs workspace create projeto-x --agents planner,coder --shared`
   cria grupo, usuários, diretórios `2770`/`0700` e ACLs padrão (2.2).
5. **Integrar o agente:** roda com seu uid; recebe o caminho do seu diretório e o da raiz;
   segue o contrato da seção 1.3 (gravar com `fsync` + `rename`, definir `namespace`, `agent`,
   `memory_id`, `QUERY` na raiz, `open()` tratando `EACCES`/`ENOENT`). Em contêiner: bind mount
   do diretório do agente e, para `QUERY`, uma das alternativas de 2.5.
6. **Operar:** `moobfs status`, `moobfs check` periódico sobre inferior desmontado ou snapshot,
   backup `rsync -aHAX`; compartilhar entre namespaces por clonagem com nova identidade (2.4).
7. **Remover agente:** `moobfs workspace revoke`, arquivar `agents/<a>` preservando xattr,
   `userdel`. **Remover workspace:** arquivar e apagar `/<ns>` pela camada (nunca por fora com
   ela montada), `groupdel`.
8. **Desinstalar:** `umount`, `rmmod moobfs_layer`, remover pacotes. O inferior permanece como
   árvore comum com `user.moob.*` legível por `grep`, `getfattr`, `git`, `rsync`.

## 5. Modelo de permissões e isolamento

Apenas POSIX (bits, grupos, ACLs, setgid). Atributos tipados não concedem nem negam acesso.
Autorização do `QUERY`: resolução do caminho com as credenciais do chamador mais
`inode_permission(MAY_READ)`, igual a um `open()` (registro 0020).

| Ator | Pode | Não pode | Observação |
| --- | --- | --- | --- |
| Administrador (root, `CAP_SYS_ADMIN`) | montar/desmontar, carregar módulo, ler e escrever o inferior, contornar toda permissão, criar workspaces | — | única fronteira que a camada não protege |
| Agente (uid próprio, sem capacidades) | ler e escrever seu diretório; ler `shared/` conforme grupo/ACL; definir `user.moob.*` em arquivos que pode escrever; `QUERY` sobre o que enxerga | ver resultados de diretórios inacessíveis (omitidos em silêncio); montar; acessar o inferior | trocar `namespace` ou `agent` num atributo não amplia acesso |
| Processo em contêiner (uid alinhado) | igual ao agente, limitado aos mounts recebidos | `QUERY` se só tem subdiretório (2.5); montar a camada | mount em userns e idmapped recusados |
| Quem tem acesso ao disco/inferior | tudo (fora da camada) | — | mitigação: dm-crypt/fscrypt no inferior, fora do escopo da camada |

Verificado na suíte da fase 3b (ext4, btrfs, XFS): sem vazamento entre dois uids e namespaces;
reatribuição de atributo não concede acesso; `chmod` vale de imediato; isolamento após
remontagem; 600 candidatos privados e 6 autorizados em mais de um lote. Também verificado em 2026-10-10 (registro 0020): ACLs POSIX pela camada, identidade com
`moobfs-check`, `mount_setattr` idmap recusado com `EINVAL` e mount em user namespace recusado
com `EPERM`. Pendente: KASAN na branch e concorrência `chmod`/`rename`/`unlink` com consultas.

## 6. Empacotamento e compatibilidade

| Tópico | Proposta | Estado |
| --- | --- | --- |
| DKMS | `dkms.conf` com nome `moobfs`, versão do repositório, `PRE_BUILD` que recusa kernel fora da matriz, `POST_INSTALL` que roda `depmod`; árvore de fontes: `layer/*.c`, `layer/moobl.h`, `include/*.h`, `common/attr.c` | FUTURO |
| Pacotes | `moobfs-dkms` (fontes + `dkms.conf`), `moobfs-tools` (`moobfs-attr`, `moobfs-check`, `moobfs-index`, futuro `moobfs`). Não empacotar `mkfs.moobfs`/`fsck.moobfs` (M1 congelado) por padrão | FUTURO |
| Matriz de kernels | ver tabela abaixo | EXISTE (duas linhas) |
| Assinatura | gerar chave de projeto para builds de release; usuários com Secure Boot assinam o `.ko` do DKMS com MOK própria; `CONFIG_MODULE_SIG_FORCE` rejeita módulos sem assinatura | FUTURO (S10) |

Matriz de kernels (o que foi realmente rodado):

| Kernel | Suíte funcional (`make vm-test`) | KASAN/UBSAN/lockdep | Estado |
| --- | --- | --- | --- |
| Debian 6.12.111+deb13-amd64 | sim, em QEMU TCG | não (kernel de distro sem esses detectores) | EXISTE |
| Upstream 6.12.112 (virtme-ng, `vm/kasan.config`) | `vm/suite.sh` completa | sim | EXISTE; para a branch `m3/3b-query-authz` o registro 0020 lista KASAN como pendente |
| Qualquer outro kernel, série ou arquitetura (6.1/6.6 LTS, 6.13+, ARM64, outros 6.12.x de distro) | não | não | não suportado; exige porte e a suíte completa mais KASAN |
| macOS / Windows | — | — | só as ferramentas portáteis de M1 compilam; o módulo é Linux |

## 7. Riscos operacionais e de segurança

| Risco | Efeito | Mitigação | Estado |
| --- | --- | --- | --- |
| Módulo é código em anel 0 | bug vira falha do kernel inteiro, inclusive DoS por usuário comum | VM descartável para desenvolvimento; S1 a S9; não instalar em hosts de produção críticos antes do preview técnico e das etapas S1 a S4 | EXISTE (VM); S1-S9 FUTURO |
| Sem KASAN/lockdep em kernels de distro | corrupção de memória pode passar sem diagnóstico | suíte sob KASAN antes de cada release; kernel de debug em staging | EXISTE (workflow); sem cobertura contínua da branch atual |
| Secure Boot/lockdown, hosts de produção, ausência de syzkaller e fstests | módulo não carrega sem assinatura; superfície de ioctl e xattr não fuzzada no kernel | assinatura MOK; S2, S3, S10; não usar em produção antes do preview | FUTURO |
| Root contorna a camada | leitura e escrita irrestritas no inferior | documentar; dm-crypt/fscrypt; auditoria | limite estrutural |
| Inferior modificado com a camada montada | índice e caminhos desatualizados; comportamento indefinido (regra do overlayfs) | permissões 0700; não fazer backup por escrita; `moobfs-check` depois | EXISTE (regra); detecção em tempo real não existe |
| Memória do índice | `index_budget` de até 4 GiB por montagem; vários mounts somam; não verificado se a memória é contabilizada em cgroup | orçamento por montagem; `GET_INFO` monitorado; `MOOB_FLAG_INDEX_INCOMPLETE` | EXISTE (orçamento); teto por usuário FUTURO (S9) |
| Caminhos obsoletos após renomear diretório | `QUERY` com falsos negativos até remontar (`MOOB_FLAG_PATHS_STALE`) | evitar renomear diretórios de namespace em produção; remontar após; alias de dentry cobre entradas em cache | EXISTE (documentado, medido pendente) |
| TOCTOU entre `QUERY` e `open()` | resultado listado e depois recusado | cliente trata `EACCES`/`ENOENT` | contrato v1 |
| Mount/userns/idmap | classe de CVEs do overlayfs evitada por flags desligadas | flags desligadas; testes de `EINVAL` e `EPERM` na suíte (registro 0020) | EXISTE (código e teste) |

## 8. Plano incremental

### Etapa 1 — Laboratório (agora; somente VM)

Escopo: o que existe, em QEMU descartável (Debian 6.12.111; KASAN 6.12.112). Falta para fechar
a fase 3b (registro 0020): KASAN sobre a branch e `chmod`/`rename`/`unlink` concorrentes com
consultas; os testes de `unshare -Ur`+mount (`EPERM`) e `mount_setattr` idmap (`EINVAL`) já
estão em `vm/suite.sh` e passaram.
Aceitação: `make vm-test` termina com `MOOBFS_LAYER_PASS`, `MOOBFS_ACCEPTANCE_PASS` e
`ISOLATION_PASS` nos três inferiores; o workflow KASAN termina com `MOOBFS_KASAN_PASS`.
Pronto quando: ambos verdes na revisão marcada, com registro e `CHANGELOG.md` atualizados.

### Etapa 2 — Preview técnico (DKMS em kernels fixados, limites documentados)

Escopo: usuários técnicos em VM ou host dedicado com kernel da matriz.
Entregáveis: `dkms.conf` com bloqueio de kernel fora da matriz; pacote `.deb` experimental
(`moobfs-dkms`, `moobfs-tools`); CLI `moobfs` mínima (seção 3); documentos de operação e de
contêiner; S1 (ADR de ameaça), S4 (inferior hostil), S5 (lint), S6 (`SECURITY.md`, política de
versões) concluídos; S2 (fstests) com resultados registrados; assinatura documentada e testada
em VM com lockdown (S10).
Aceitação:

1. instalar numa Debian limpa da matriz, montar, `workspace create`, rodar o contrato de
   integração como dois uids; sem vazamento entre eles;
2. atualizar e reverter o módulo sem perda (`moobfs-check` e `QUERY` igual ao oráculo antes e depois);
3. backup e restauração `rsync -aHAX`: `getfattr -R` e `cmp` idênticos;
4. kernel fora da matriz: DKMS recusa; módulo assinado carrega com lockdown, não assinado é recusado;
5. contêiner com bind mount do diretório do agente: leitura, escrita e xattr funcionam com
   uid alinhado; `QUERY` no subdiretório devolve `ENOTTY` como documentado.

Pronto quando: um terceiro reproduz tudo, a matriz e as limitações cabem numa página e nenhum vazamento é conhecido.

### Etapa 3 — Versão pública

Pré-condições: **M4 concluído e publicado** (W3 com autorização por candidato, tempo de
montagem e reconstrução, memória do índice, p50/p95/p99, amplificação de escrita) e decisões
sobre `QUERY_BATCH`, `READ_BATCH` e persistência registradas (DEPENDE DO M4);
**roadmap de segurança** com S1 a S6 e S8 a S10 concluídos (S2 fstests, S3 syzkaller com
cobertura definida, S9 limites, S10 assinatura/DKMS), S7 agendado e S11 revisado; KASAN,
lockdep, KFENCE e opções de S8 verdes na matriz completa.
Entregáveis: pacotes das distribuições da matriz, política de suporte, divulgação e
compatibilidade da UAPI, README sem promessas além do medido.
Aceitação: etapa 2 repetida em cada distribuição/kernel da matriz; syzkaller sem relatos
abertos por período definido; upgrade de kernel com DKMS; escala acima do medido documentada
como não validada.
Pronto quando: tudo verde e os registros trazem os números.

## 9. Arquivos do repositório que precisariam mudar

Este documento cria somente `docs/distribution-strategy.md`.
| Arquivo | Mudança | Etapa |
| --- | --- | --- |
| `dkms.conf` ou `packaging/dkms/` (novo) | definição DKMS | 2 |
| `packaging/debian/`, `packaging/rpm/moobfs.spec` (novos) | pacotes `.deb` (2) e `.rpm` (3) | 2-3 |
| `tools/moobfs.c` (novo), `tools/query-common.h`, `Makefile` | CLI `moobfs`; alvos `install`, `dist`, `dkms-test` | 2 |
| `vm/suite.sh`, `vm/Dockerfile`, `vm/in-container.sh` | testes de userns, idmap, bind mount (`ENOTTY`), upgrade/rollback; pacotes `dkms`, `rsync` | 1-2 |
| `.github/workflows/ci.yml`, `kasan.yml` | job DKMS, matriz de kernel, assinatura, jobs agendados | 2-3 |
| `docs/operations.md`, `docs/container-integration.md`, `docs/compatibility.md` (novos) | operação, contêineres, matriz e política de UAPI | 2 |
| `docs/testing.md`, `docs/spec-m3.md`, `docs/security-roadmap.md` | pacotes e instalação; referência cruzada de clonagem e da raiz em contêineres (§2, §3.1); estado de S1-S10 | 2-3 |
| `docs/decisions/` | ADR nova de modelo de ameaça (S1) com ID sequencial; nenhuma ADR existente é alterada | 2 |
| `docs/history/`, `CHANGELOG.md` | registro por entrega com comandos reproduzíveis | todas |
| `README.md`, `README.pt-BR.md`, `ROADMAP.md`, `SECURITY.md`, `site/` | link para este documento, matriz de kernel, avisos, divulgação, página de instalação | 2-3 |
| `include/moobfs_ioctl.h`, `layer/index.c`, `layer/main.c` | sem mudança prevista; qualquer ajuste (por exemplo, `QUERY` em bind de subdiretório) exigiria especificação, testes de entradas inválidas e decisão de compatibilidade da UAPI | — |
