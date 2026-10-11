# Campanha do M4 em convidado acelerado: roteiro

Estado: proposta (2026-10-10), não executada. Complementa `docs/benchmark-plan.md` ("Método":
VM dedicada com KVM; emulação só para correção) e `docs/benchmark-harness.md`. O harness
está completo e validado na VM emulada; este documento diz como obter números de verdade.

## 1. Por que a VM atual não serve para medir

`make vm-test` roda um kernel x86_64 (Debian 6.12.111) no QEMU dentro do Docker Desktop. Em um
Mac com Apple Silicon isso é emulação instrução a instrução (TCG), dezenas de vezes mais lenta
que o hardware e com distribuição de latências que não representa nada. O runner do GitHub
também não tem KVM. Logo, nenhum tempo impresso por essas execuções é medida do projeto.

## 2. Três caminhos

| Caminho | Hardware | Arquitetura do convidado | Valor dos números | Custo |
| --- | --- | --- | --- | --- |
| A. Host Linux x86_64 com KVM (bare metal ou nuvem com virtualização aninhada) | qualquer | x86_64, o mesmo kernel Debian 6.12.111 já testado | referência para o relatório do M4 | aluguel de horas ou máquina própria |
| B. Mac Apple Silicon com Hypervisor.framework (Lima, UTM ou `qemu-system-aarch64 -accel hvf`) | o Mac de desenvolvimento | arm64, Debian 13 com kernel 6.12 arm64 | comparação relativa A/B/C na mesma máquina; arquitetura fora da matriz testada | zero |
| C. VM de nuvem x86_64 usada como host (Azure D4as_v5, Oracle E3.Flex ou similar), com o módulo no kernel da própria instância | qualquer | x86_64, kernel 6.12.112 compilado na instância por `bench/kernel-build.sh` | medição em VM de nuvem, ambiente realista de produção; mesmo tarball do KASAN, sem instrumentação | horas de aluguel |

Recomendação (2026-10-11): C é o caminho escolhido para a primeira campanha, por juntar x86,
custo baixo e um segundo kernel na matriz; A continua sendo a referência quando houver
hardware dedicado; B fica como preliminar opcional. Em B, o módulo é compilado para uma arquitetura nova: antes de medir, a suíte
inteira (`vm/suite.sh`) e uma execução sob KASAN precisam passar nesse kernel, e o relatório
deve dizer "arm64 virtualizado, fora da matriz".

## 2.1 Caminho C: VM de nuvem como host (Azure)

Criação (portal da Azure): imagem Ubuntu Server 24.04 LTS x64 Gen2; tamanho
`Standard_D4as_v5` (4 vCPU, 16 GB) ou `D4s_v5`; **tipo de segurança Standard**, não Trusted
launch, porque o kernel compilado não é assinado e o Secure Boot o recusaria; chave SSH; só a
porta 22; disco do SO Premium SSD 64 GB; **três discos de dados idênticos** para ext4, btrfs e
XFS, com cache "Nenhum"; sem série B (burstable); desligamento automático.

Discos para a campanha: **Premium SSD v2** com IOPS e vazão fixos e iguais nos três. Os Premium
SSD de P1 a P20 têm bursting por créditos ligado por padrão (o P6 garante 240 IOPS e sobe até
3500 enquanto há créditos, que se esgotam em cerca de 30 minutos de carga), então a velocidade
do dispositivo mudaria no meio de uma campanha de horas e a diferença seria atribuída ao sistema
medido. P30 e acima não têm bursting por créditos, mas são grandes e caros para este uso. Discos
P6 com bursting servem para o portão `HOST_SETUP_PASS` e para o piloto, nunca para o relatório.

Na instância, como root, na ordem:

```bash
apt-get update && apt-get install -y build-essential flex bison libelf-dev libssl-dev bc dpkg-dev debhelper rsync kmod git
git clone https://github.com/studiomoob/llmoobfs && cd llmoobfs
sh bench/kernel-build.sh            # 6.12.112 a partir do config do Ubuntu + vm/bench.config; 45-60 min em 4 vCPU
sed -i 's/^GRUB_DEFAULT=.*/GRUB_DEFAULT=saved/' /etc/default/grub && update-grub
grep -E "menuentry '|submenu '" /boot/grub/grub.cfg | cut -d"'" -f2     # nome exato da entrada 6.12
grub-reboot "Advanced options for Ubuntu>Ubuntu, with Linux 6.12.112-moobfs-bench"   # só o próximo boot
shutdown -h now   # no portal: VM desalocada, desligar "inicialização segura", ligar
uname -r          # 6.12.112-moobfs-bench; se não subir, reiniciar volta ao kernel da Azure
lsblk                               # identificar os três discos de dados (sdc, sdd, sde ou similares)
sh bench/host-setup.sh --ext4 /dev/sdc --btrfs /dev/sdd --xfs /dev/sde   # formata, compila, roda a suíte inteira
```

`HOST_SETUP_PASS` é o portão: a suíte completa (`MOOBFS_LAYER_PASS`, `MOOBFS_ACCEPTANCE_PASS`)
passou nesse kernel e nesses discos, sem diagnóstico no `dmesg`. Qualquer falha vira registro
antes de medir. Depois, calibração (§5) e campanha (§6) com os mesmos dispositivos montados.
Se o KASAN do kernel 6.12.112 ainda não tiver rodado nessa máquina, não é necessário: o
workflow já cobre esse kernel; o que a suíte valida aqui é o host e os discos.

## 3. Preparar o convidado (caminho B, Lima)

Tudo abaixo é o procedimento proposto; os comandos não foram executados neste repositório.

```bash
brew install lima
limactl create --name moob --vm-type vz --cpus 4 --memory 8 --disk 40 template://debian-13
limactl start moob
limactl shell moob
```

Dentro do convidado (Debian arm64):

```bash
sudo apt-get update && sudo apt-get install -y build-essential linux-headers-$(uname -r) \
  e2fsprogs btrfs-progs xfsprogs sqlite3 libsqlite3-dev git dmsetup acl python3 fio
git clone https://github.com/studiomoob/llmoobfs && cd llmoobfs
make layer KDIR=/lib/modules/$(uname -r)/build
make attr-tools bench-tools BUILD=build/arm64 CFLAGS="-O2 -g"
gcc -O2 -Wall -Wextra -Werror -Iinclude tests/query.c common/attr.c -o build/arm64/query
gcc -O2 -Wall -Wextra -Werror tests/layer.c -o build/arm64/layer
gcc -O2 -Wall -Wextra -Werror tests/idmap.c -o build/arm64/idmap
gcc -O2 -Itests/third_party/fsx tests/third_party/fsx/fsx.c -o build/arm64/fsx
gcc -O2 tests/third_party/log-writes/replay-log.c tests/third_party/log-writes/log-writes.c -o build/arm64/replay-log
cp layer/moobfs_layer.ko bench/queries-w3.txt build/arm64/
for f in sqlbase equiv run; do cp bench/$f.sh build/arm64/bench-$f.sh; done
cp bench/queries-w3.txt build/arm64/bench-queries-w3.txt
```

Discos dedicados: no Lima, acrescentar discos virtio com `limactl disk create` e `--mount`,
ou, para a preliminar, usar arquivos de imagem em loop (pior para números de disco, aceitável
para calibrar):

```bash
sudo truncate -s 2G /var/tmp/ext4.img /var/tmp/btrfs.img /var/tmp/xfs.img
sudo truncate -s 256M /var/tmp/scratch.img /var/tmp/log.img
sudo mke2fs -q -F -t ext4 -b 4096 -L moob-lower /var/tmp/ext4.img
sudo mkfs.btrfs -q -f -L moob-lower /var/tmp/btrfs.img
sudo mkfs.xfs -q -f -L moob-lower /var/tmp/xfs.img
for i in ext4 btrfs xfs scratch log; do sudo losetup -f --show /var/tmp/$i.img; done
```

## 4. Validar antes de medir

A suíte é parametrizada como em `vm/kasan-guest.sh`: `BIN`, `TOOLS`, `MODS`, `LOWER`,
`LAYER` e `DEV_*`. Como root no convidado:

```bash
export BIN=$PWD/build/arm64 TOOLS=/usr/bin MODS=/nonexistent LOWER=/mnt/moob-lower LAYER=/mnt/moob-layer
export DEV_EXT4=/dev/loop0 DEV_BTRFS=/dev/loop1 DEV_XFS=/dev/loop2 DEV_SCRATCH=/dev/loop3 DEV_LOG=/dev/loop4
mkdir -p $LOWER $LAYER; ln -sf /usr/sbin/dmsetup /usr/sbin/mke2fs /usr/sbin/e2fsck /usr/bin/setfacl /usr/bin/getfacl $BIN/ 2>/dev/null
insmod build/arm64/moobfs_layer.ko
sh vm/suite.sh | tee build/arm64/suite.log
```

Critério: `MOOBFS_LAYER_PASS` e `MOOBFS_ACCEPTANCE_PASS`, sem `WARNING`, `BUG` ou `Oops` em
`dmesg`. Diferenças de arquitetura prováveis que a suíte vai expor: tamanho de página (o Lima
usa 4 KiB em Debian arm64; 16 KiB mudaria limites de xattr do ext4), e `/tools` ausente
(`TOOLS` aponta para `/usr/bin`). Qualquer falha vira registro próprio antes de qualquer número.

## 5. Calibrar o disco

`sh bench/calibrate.sh --out <dir da campanha> ext4=/srv/moob/ext4 btrfs=/srv/moob/btrfs
xfs=/srv/moob/xfs`: 30 s de escritas aleatórias de 4 KiB, cada uma seguida de `fsync`, por
inferior; imprime operações por segundo e a latência do próprio `fsync` (p50, p99) e guarda o
JSON do `fio`. A latência de conclusão da escrita (`clat`) não serve para isso: com `--fsync=1`
ela mede só a escrita no cache de página (cerca de 10 µs no host da Azure). O resumo entra no
relatório junto com o `environment.txt`.

## 6. Executar a campanha

Montar a camada por inferior e chamar o orquestrador com os três sistemas:

```bash
mount /dev/loop0 /mnt/a-ext4 && mount /dev/loop1 /mnt/b-btrfs
mount /dev/loop2 /mnt/c-lower && mount -t moobfs_layer -o lower=/mnt/c-lower none /mnt/c-layer
BIN=$PWD/build/arm64 sh bench/run.sh --out build/bench/$(date -u +%Y%m%dT%H%M)-arm64 \
  --scale 1000 --profile 8 --reps 7 --procs 4 \
  --plain A-ext4=/mnt/a-ext4/bench --plain B-btrfs=/mnt/b-btrfs/bench \
  --layer C-ext4=/mnt/c-layer:/mnt/c-lower
```

Escalas S1 a S6 e perfis 6, 8 e 16 são execuções separadas do mesmo comando. O `report.py`
produz `results.csv` e marca "inconclusivo" acima de 20% de dispersão. Saturação do índice: a
mesma campanha com `index_budget=` reduzido na montagem, lendo `get_info.txt` de cada sistema.

## 7. O que registrar

Por campanha: `environment.txt` (o `run.sh` grava kernel, CPU, memória, presença de KVM ou
HVF, versão do SQLite, commit), a calibração do `fio`, a saída de `equiv.txt` com os planos
`SCAN` justificados, e o `results.csv`. O relatório em `docs/history/` cita o caminho de cada
arquivo e declara a arquitetura e o tipo de aceleração. Nenhum número entra em README, site ou
documento de valor sem esse caminho.
