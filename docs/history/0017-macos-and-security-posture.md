# 0017 — Uso no macOS e postura de segurança comparada ao mercado

Data: 2026-10-10, America/Sao_Paulo. Estado: **perguntas e respostas**; nenhuma mudança de
código. Registro de três perguntas do usuário e das respostas dadas, com as fontes consultadas.

## Pergunta 1: "Como seria utilizar isso no Mac?"

Resposta: não roda nativamente. A camada é um módulo do kernel Linux (`layer/moobfs_layer.ko`)
sobre ext4, btrfs ou XFS. O macOS usa XNU, kexts de terceiros estão em desuso e exigem reduzir a
segurança no Apple Silicon; FSKit é em espaço de usuário, a abordagem que a ADR-0001 descarta.

Formas de uso no Mac:

- Ferramentas de host (`tools/`) compilam e rodam no macOS com `make test`; não montam nada.
- VM Linux no Mac (uso real): hoje `make vm-test` com Docker e QEMU x86-64 emulado; para uso
  efetivo, VM arm64 (Lima, UTM, OrbStack) com o módulo compilado contra o kernel da VM, inferior
  em disco ext4/btrfs/XFS dentro da VM, e o agente rodando dentro da VM.
- Mac como cliente da VM por SSH, sshfs, NFS ou SMB: `QUERY` e `GET_INFO` são ioctls e não
  atravessam esses protocolos; exportação NFS está fora do escopo do M2. Precisaria de um
  serviço dentro da VM, na linha do cliente de memória próprio da aceitação do M2.
- "Nada preso": o inferior desmontado é arquivos com xattr; `rsync -X` ou git levam ao APFS, e
  `moobfs-check` valida offline no Mac. O índice e a consulta só existem no kernel Linux.
- Não funciona pôr o inferior numa pasta do Mac via virtiofs: APFS não tem reflink no sentido de
  btrfs/XFS e xattr por virtiofs com host macOS não é garantido.

Suporte nativo no Mac seria uma nova ADR (FSKit ou kext), não uma extensão.

## Pergunta 2: "Quais os mecanismos de segurança? É padrão de mercado? Exploit de kernel, leitura de memória inválida, guardrails?"

Levantamento do que o repositório tem (verificado em `layer/`, `.github/workflows/ci.yml`,
`vm/`, `docs/testing.md`):

- Validação na borda: `QUERY` recusa limite zero ou acima do teto, flags e reservados não nulos
  (`layer/index.c`); `copy_to_user` com tamanho fixo; atributos tipados validados antes de tocar
  o inferior (tipo, tamanho, alfabeto do nome, UTF-8).
- Permissões em dupla camada (`inode_permission` no inferior mais a verificação do próprio
  inferior) e `override_creds`, como no overlayfs.
- KASAN, UBSAN e lockdep em workflow dedicado (`vm/kasan-guest.sh`), falhando em qualquer
  `BUG`, `Oops`, `WARNING` ou relato do KASAN no `dmesg`; ASan e UBSan no código de `common/`.
- libFuzzer sobre a codificação de atributos, com tempo limitado no CI.
- Módulo carregado só em VM QEMU descartável; orçamento de memória do índice (`index_budget`).

Lacunas apontadas: modelo de ameaça escrito; fuzzing de syscalls (syzkaller com descrições dos
ioctls); tratar o inferior como entrada não confiável na varredura de montagem; manter
`FS_USERNS_MOUNT` desligado enquanto user namespaces estiverem fora do escopo e dizer isso na
ADR; `SECURITY.md`, sparse, smatch e checkpatch no CI; assinatura do módulo e Secure Boot para
usuários finais. "Guardrails" no sentido de LLM não cabem na camada: é armazenamento; política de
conteúdo fica no cliente.

## Pergunta 3: "Pesquise em artigos, no mercado, em projetos com essas mesmas características"

Fontes consultadas em 2026-10-10 (busca na web; resumos, não leitura integral):

- Documentação do kernel, "Adding New Filesystems": fstests é a infraestrutura padrão e seu uso é
  fortemente recomendado; exige forma documentada de testar e detectar regressões; o filesystem
  deve lidar com entrada corrompida sem travar ou derrubar o kernel; fsck fortemente
  recomendado; ioctls próprios em patches separados com justificativa.
- Janus (IEEE S&P 2019) e Hydra (SOSP 2019), Georgia Tech: fuzzing conjunto de imagem e
  syscalls; Janus encontrou 90 bugs em oito filesystems do kernel, 32 CVEs; Hydra encontrou 155
  bugs incluindo inconsistência após queda e violações de POSIX. LFUZZ (FSU, 2024) estende Janus.
- Debate em linux-fsdevel e LWN (2018, 2023, 2025) sobre imagens maliciosas: maintainers não
  prometem que montar imagem hostil é seguro; Dave Chinner considera um crash disparável por
  usuário um problema de segurança (DoS) mesmo sem exploit; fila de bugs do syzbot excede a
  capacidade de correção; isolamento em VM (libguestfs) é a abordagem considerada segura.
- bcachefs: antes do merge no 6.7, o autor reportou fstests e suíte própria sob lockdep+KASAN,
  preempt e gcov (82% de linhas); removido da árvore no 6.18 e hoje DKMS.
- overlayfs: CVE-2015-1328, CVE-2016-1576, CVE-2020-16120 e CVE-2026-74619, todos ligados a
  montagem não privilegiada via `FS_USERNS_MOUNT` e user namespaces; distribuições que deixam a
  flag desligada evitam a classe inteira. Histórico de `override_creds=off` mostra que o modelo
  de credenciais do montador precisa estar escrito para ser revisável.
- eCryptfs (camada empilhada): CVE-2014-9922 (escalada por pilha grande de filesystems com
  overlayfs), CVE-2009-2406 (overflow em parser de metadados), e maioria dos CVEs no helper de
  montagem em espaço de usuário.
- xattr: CVE-2026-63833 (ntfs3, setxattr direto em nomes reservados dá euid 0), CVE-2010-2946
  (JFS, prefixo de nome contorna restrição de namespace), CVE-2019-19319 (ext4, setxattr após
  imagem forjada causa escrita fora de limites), CVE-2016-7097 (setgid preservado em setxattr).
- OpenZFS (módulo fora da árvore): `ztest` como stress em espaço de usuário com injeção de
  falhas; correção de 2026 para manipulação de pool por userns não privilegiado.
- syzkaller: descrições syzlang em `sys/linux/*.txt` com `.const` manual ou `syz-extract`;
  não há fluxo oficial para módulos fora da árvore, mas o padrão é um checkout local.

Conclusão registrada: o conjunto atual do MoobFS (validação na borda, KASAN/UBSAN/lockdep,
libFuzzer, VM isolada) é o que projetos comparáveis tinham no estágio experimental. O que falta
para o patamar de mercado: fstests, syzkaller sobre os ioctls e o `setxattr`, fuzzing do inferior
como entrada hostil, modelo de ameaça e de credenciais escrito, `FS_USERNS_MOUNT` explicitamente
desligado, e processo de divulgação.

## Pedido 4: "Salve todas as técnicas para um roadmap futuro"

Criado `docs/security-roadmap.md` com estado atual verificado, onze itens S1 a S11 com
precedente, passos e critério de pronto, e o que fica fora de escopo. Linha transversal
adicionada em `ROADMAP.md` e entrada no `CHANGELOG.md`.

## Próximo passo

Nenhum código alterado. Primeiro item do roadmap de segurança: S1, ADR de modelo de ameaça e
credenciais. Outros candidatos: ADR curta de modelo de ameaça; descrições syzlang para
`QUERY` e `GET_INFO`; rodar fstests (grupo `quick`) sobre a camada na VM.
