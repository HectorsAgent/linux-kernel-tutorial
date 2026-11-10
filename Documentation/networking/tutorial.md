# Linux nätverksstack i miniatyr, i C

Nätverksdelen av [linux-kernel-tutorial](../../README.md). Modellen följer Linux
centrala objekt och IPv4-vägar, med små funktioner som går att läsa från början
till slut.

Allt körs i **user space** med simulerade interface. Programmet öppnar inga riktiga
nätverkssockets och ändrar inga regler eller interface på datorn. Kräver bara C11,
`make` och en vanlig C-kompilator. Inga externa bibliotek.

## Kör

```sh
# Kör från repots rot:
make demo
make nat-demo
make test
```

`make` bygger även `build/liblearnnet.a`.

`make demo` börjar i en process som skickar `"hej"` med ett FD-nummer och slutar
vid en färdig Ethernet-frame i den simulerade drivrutinens utkorg. Det finns
ingen mottagare i det första demot: slutpunkten är "ut på kabeln".

`make nat-demo` har en container, en host som router och en simulerad extern server.
Containern skickar `hej` till servern och får `svar` tillbaka. Bara hosten har
conntrack/NAT. Alla namespaces skriver ut funktionerna och hooks som paketet
passerar, inklusive adresser, portar och TTL efter översättningar.

Containeradress: `10.0.0.2`. Hostens LAN: `10.0.0.1`. Hostens externa adress:
`203.0.113.10`. Server: `203.0.113.20`. Servern ligger direkt på den simulerade
externa länken för att vi ska slippa ytterligare en router. Adresserna
`203.0.113.x` används här som dokumentationsadresser.

## Börja läsa här

Följ **en funktion per steg**. Konfigurationen före sändningen är labbstöd och
kan lämnas till senare; anslutning och neighbor-post finns redan när vi börjar.

1. [tools/net/send_demo.c](../../tools/net/send_demo.c): anropet till
   `tutorial_send(&process, fd, "hej", 3, 0)`. Detta ersätter libc och CPU:ns
   syscall-inträde och väljer vilken process som är `current` i simuleringen.
2. [net/socket.c](../../net/socket.c): `__sys_sendto()` får FD-numret och
   beskriver databufferten med `import_ubuf()`. Ingen paketbuffer finns ännu.
3. [fs/file.c](../../fs/file.c): `fdget()` / `fget()` hittar filobjektet i
   `current->files->fdt`. `fdput()` släpper den tillfälliga referensen efteråt.
4. [net/socket.c](../../net/socket.c): `sock_from_file()` hittar `struct socket`.
   `sock_sendmsg()` anropar `socket->ops->sendmsg`, alltså socketfamiljens kod.
5. [net/ipv4/af_inet.c](../../net/ipv4/af_inet.c): `inet_sendmsg()` anropar
   `sock->sk_prot->sendmsg`, som för denna anslutning pekar på `tcp_sendmsg()`.
6. [net/ipv4/tcp.c](../../net/ipv4/tcp.c): `tcp_sendmsg()` kopierar bytes från
   `msghdr.msg_iter` till TCP-socketens send-buffer med `copy_from_iter()`.
7. [net/ipv4/tcp_output.c](../../net/ipv4/tcp_output.c): `tcp_write_xmit()`
   delar upp datan i segment och skapar `sk_buff` med TCP-header och payload.
8. [net/ipv4/ip_output.c](../../net/ipv4/ip_output.c): `ip_queue_xmit()`
   lägger till IP-headern och routar. `ip_local_out()` kör OUTPUT och
   `ip_output()` kör POSTROUTING.
9. [net/core/neighbour.c](../../net/core/neighbour.c): `neigh_output()`
   slår upp nästa hops MAC och fyller Ethernet-headern med MAC-adresser/EtherType.
10. [net/core/dev.c](../../net/core/dev.c): `dev_queue_xmit()` anropar
    `dev->netdev_ops->ndo_start_xmit`. I [drivers/net/tutorial.c](../../drivers/net/tutorial.c)
    är det `ethernet_xmit()`, som placerar framen i utkorgen för "kabeln".

När sändningen är bekant fortsätter vi med [ip_input.c](../../net/ipv4/ip_input.c),
[ip_forward.c](../../net/ipv4/ip_forward.c),
[route.c](../../net/ipv4/route.c) och [NAT-demot](../../tools/net/nat_demo.c).
I [net/netfilter/core.c](../../net/netfilter/core.c) finns våra "middlewares":
`NF_HOOK()` skapar kontexten och `nf_hook_slow()` kör funktionerna i prioritetsordning.

Du kan vänta med nedanstående filer tills vi behöver detaljerna:

- [nf_conntrack_core.c](../../net/netfilter/nf_conntrack_core.c): uppslag i båda
  riktningarna, tillfälliga poster och bekräftelse efter brandväggen.
- [nf_nat_core.c](../../net/netfilter/nf_nat_core.c) väljer NAT-mappning och
  hanterar portkollisioner. [nf_nat_proto.c](../../net/netfilter/nf_nat_proto.c)
  kopplar in IPv4-NAT och utför översättningarna i båda riktningarna.
- [nf_tables_core.c](../../net/netfilter/nf_tables_core.c): regelmatchningar.
  [nf_tables_api.c](../../net/netfilter/nf_tables_api.c) konfigurerar kedjor/regler.
- [tcp.c](../../net/ipv4/tcp.c): socketkonfiguration och send/recv-API.
  [tcp_output.c](../../net/ipv4/tcp_output.c) segmenterar och sänder.
  [tcp_ipv4.c](../../net/ipv4/tcp_ipv4.c) hittar den mottagande socketen;
  [tcp_input.c](../../net/ipv4/tcp_input.c) buffrar och ordnar mottagna data.
- [dev.c](../../net/core/dev.c): gemensam TX/RX och interfacekonfiguration.
  [neighbour.c](../../net/core/neighbour.c) hittar nästa hops MAC.
  [loopback.c](../../drivers/net/loopback.c) och
  [veth.c](../../drivers/net/veth.c) är separata drivrutiner.
  [tutorial.c](../../drivers/net/tutorial.c) är en fiktiv Ethernet-drivrutin
  med en utkorg som ersätter fysisk hårdvara.

## Tre paketvägar att leta efter

**Process till TCP:** `tutorial_send` → `__sys_sendto` → `fdget` →
`sock_from_file` → `sock_sendmsg` → `inet_sendmsg` → `tcp_sendmsg`.

**Lokalt skapat paket:** `tcp_sendmsg` → `tcp_write_xmit` → `ip_queue_xmit` → initial routing →
`ip_local_out` → OUTPUT → `dst_output` → `ip_output` → POSTROUTING →
`ip_finish_output` → neighbor/Ethernet → `dev_queue_xmit`.

**Inkommande till lokal adress:** `netif_receive_skb` → `ip_rcv` → PREROUTING →
routing → `dst_input` → `ip_local_deliver` → INPUT → `tcp_v4_rcv`.

**Vidarebefordrat:** `netif_receive_skb` → `ip_rcv` → PREROUTING → routing →
`dst_input` → `ip_forward` → FORWARD → `dst_output` → `ip_output` → POSTROUTING →
`ip_finish_output` → neighbor/Ethernet → `dev_queue_xmit`.

Ett paket till en egen IP-adress går vid lokal sändning ut genom `lo` och
kommer tillbaka genom mottagningsvägen. Det gäller även en IP konfigurerad på
`eth0`. OUTPUT kan ändra destinationen; därför gör modellen om routinguppslaget
efter OUTPUT. Linux har mer selektiva mekanismer för omrouting.

## Objekten

Definitionerna är uppdelade efter Linux headerstruktur. Exempelvis finns
`struct sk_buff` i [include/linux/skbuff.h](../../include/linux/skbuff.h),
`struct net_device` i [include/linux/netdevice.h](../../include/linux/netdevice.h),
`struct net` i [include/net/net_namespace.h](../../include/net/net_namespace.h),
`struct sock` i [include/net/sock.h](../../include/net/sock.h) och `struct nf_conn`
i [include/net/netfilter/nf_conntrack.h](../../include/net/netfilter/nf_conntrack.h).
Headers använder forward declarations när en pekare räcker, så definitioner
behöver inte dras in överallt.

| Namn | Ansvar i modellen |
| --- | --- |
| `struct task_struct` / `current` | Processen som gör systemanropet |
| `struct files_struct` / `fdtable` | Processens tabell från FD-nummer till filpekare |
| `struct file` | Öppet filobjekt; en socketfil pekar på `struct socket` |
| `struct socket` / `proto_ops` | Socketlagrets objekt och familjens operationer |
| `struct proto` | Transportprotokollets operationer, exempelvis `tcp_prot` |
| `struct msghdr` / `iov_iter` | Beskriver datan som ska skickas, före paketisering |
| `struct net` | Ett network namespace: interface, routes, sockets, regler och conntrack |
| `struct net_device` / `net_device_ops` | Interface och dess sändningsfunktion |
| `struct sk_buff` | Paketet samt metadata som interface, route och conntrack-referens |
| `struct sock` / `tcp_sock` | En lokal TCP-ändpunkt och dess byteströmsbuffertar |
| `struct dst_entry` / `rtable` | Routingresultat med interface och fortsättningsfunktioner |
| `struct nf_hook_state` | Hook, namespace, in-/utinterface och eventuell lokal socket |
| `struct nf_conn` | Ett spårat flöde med original- och svarstupel, utan TCP-socket |
| `struct nft_chain` / `nft_rule` | En liten regelmodell för filter och NAT |

Samma funktionskod används av alla namespaces. Det är `dev_net(dev)` vid RX
och `sock_net(sk)` vid lokal TX som bestämmer vilken nätverksmiljö som används.
Processens aktuella nätverksnamespace finns i `current->nsproxy->net_ns`.
Sändningen följer ändå socketens `sk_net`, även om processen bytt namespace
efter att socketen skapades. FD-numret är bara ett index i processens egen tabell;
det innehåller ingen information om nätverk eller protokoll.
När ett paket går över veth rensas gammal route/conntrack-koppling innan peer
tar emot det. Mottagaren gör egna uppslag i sitt namespace.

Conntrack kopplas här in när vi lägger till NAT eller en regel med `ct_state`.
Det kan också aktiveras med `nf_conntrack_enable()`. I ett aktiverat namespace
spåras även trafik som inte matchar en NAT-regel. Conntrack får inget beroende
till hostens sockettabell. En ny post bekräftas sent i INPUT eller POSTROUTING;
droppas första paketet före dess städas den tillfälliga posten bort med paketet.

## Medvetna förenklingar jämfört med Linux

- **Katalogerna speglar berörda delar av Linux**, med `net/`, `include/`,
  `drivers/net/`, `tools/` och `Documentation/` direkt i roten. Vi skapar inte
  tomma kopior av subsystem som ännu saknas. Flera stora Linux-filer har här
  bara några tiotal rader. Ett separat UAPI-headerlager och Kbuild/Kconfig
  är bortvalda. `include/tutorial/`, `lib/tutorial.c`, demot och den fiktiva
  Ethernet-drivrutinen är labbspecifika tillägg.
- **Samma namn betyder inte samma ABI.** Centrala objekt och paketfunktioner
  följer Linux, men signaturer, fält och vissa konstantvärden är förenklade.
  `net_init`, `netdev_add`, `tcp_socket`, `fib_add_route` och `nft_add_*` är
  hjälpfunktioner för labbets konfiguration, inte exakta Linux-API:er.
- IP-/transportheaders är små C-strukturer. `skb->data` innehåller bara payload.
  Inga riktiga wire-format, checksummor, header-offsets, fragment eller headroom.
  **IP-adresser och portar är i host byte order**, även i headerstrukturerna.
- En tråd, fasta tabellstorlekar och linjära uppslag. `struct file` har enkel
  referensräkning för `fget`/`fput`. Inga lås, RCU, atomics, nätverksobjekts-
  refcounts, per-CPU-data, trie eller delade paketbuffertar. Structar med pekare ska inte
  kopieras eller flyttas efter konfiguration.
- TCP-sockets skapas redan anslutna. Ingen listen/accept, handshake, ACK,
  omsändning, congestion control, stängning eller sekvensnummer-wrap. Små
  icke överlappande segment kan anlända i fel ordning. UDP levereras till en
  enkel räknare; separata UDP-sockets och ICMP saknas.
- Conntrack använder en liten tabell per `struct net`. Ingen hashoptimering,
  zonindelning, timeout eller full protokoll-state machine. ESTABLISHED betyder
  här att ett paket har känts igen i svarsriktningen; det bevisar inte ett
  färdigt TCP-handslag. När tabellen blir full droppas nya flöden.
- En filterkedja och en NAT-kedja per hook. Ingen Netlink, CLI-parser,
  nftables-VM, underkedjor, dynamiska sets, NOTRACK eller valbar kedjeprioritet.
  NAT-kedjor finns för PREROUTING, OUTPUT och POSTROUTING. DNAT bevarar porten
  om ingen ny anges; MASQUERADE försöker behålla originalporten. En DNAT-kollision
  som modellen inte kan representera droppas. NAT-regler väljer en mappning en
  gång; senare paket använder conntrack, även om reglerna ändras.
- Grannar förkonfigureras i stället för ARP. En adress per interface, vanlig
  unicast-routing och inga policyregler utöver local/main. Ingen bridge,
  VLAN, tunnel eller VRF. Veth/loopback anropar RX synkront utan IRQ/NAPI/köer.
- `tutorial_send()` är en C-funktion som simulerar libc och CPU:ns syscall-entry.
  Ingen riktig privilege transition, scheduler, fork eller virtuell minnesmodell.
  `current` är en global pekare. Process-/FD-tabeller, filobjekt, socketobjekt och
  familje-/protokoll-dispatch ingår. `kernel/fork.c` innehåller bara labbets
  processkonstruktor. `socket()`, bind/connect och TCP-handshake är inte syscalls
  i denna modell: `tcp_socket()` och `task_install_socket()` förbereder anslutningen.
- `__sys_sendto()` stödjer anslutna sockets, flags=0 och addr=NULL. Den returnerar
  negativa errno-värden direkt; libc:s `errno`-översättning saknas. `msghdr` har
  bara en sammanhängande databuffer. `import_ubuf()` avvisar NULL med längd > 0,
  men kan inte validera godtyckliga pekare eller verkliga user space-sidor.
  Bytekopieringen sker i TCP med `copy_from_iter()`. Inga standardströmmar skapas,
  så första FD-numret i exemplen är 0. Close frigör fil-/socketomslaget men TCP-
  objektet ägs fortsatt av namespacet; FIN och full socketlivstid är bortvalda.
- Receive-syscall är inte med ännu. NAT-demot läser mottagna byte direkt med
  modellens `tcp_recvmsg()`. Den simulerade Ethernet-utkorgen är sista steget på
  sändningssidan; inget skickas via verklig hårdvara och framen serialiseras inte.

## Verifiering

`make test` kör tolv nätverksscenarier och sex syscall-scenarier. Nätverkstesterna:
lokal TCP över lo till en eth0-adress,
byteström och segmentering, segment i fel ordning, längsta-prefix-routing,
neighbor-nyckel per interface, forwarding utan conntrack, TTL, SNAT tur/retur,
separat conntrack i båda namespaces, brandvägg och sena conntrack-bekräftelser,
UDP-portkollisioner och svar, bevarade NAT-mappningar efter regeländring,
DNAT med omvänd översättning, OUTPUT-DNAT med omrouting samt tidig NF_DROP.

Syscall-testerna finns i [test_syscall.c](../../tools/testing/selftests/net/test_syscall.c).
De följer byte från FD till Ethernet, provar samma FD-nummer i olika processer,
felaktiga FD:n, fil som inte är en socket, NULL-buffer, osupporterade flaggor,
nollängdssändning, socketens namespace efter processens namespace-byte, dispatch
genom `sk_prot`, filreferensens livstid över close, segmentering och full FD-tabell.

Minneskontroll med GCC/Clang:

```sh
make BUILD=build-asan CFLAGS='-std=c11 -Wall -Wextra -Werror -Wpedantic -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' test
```

## Motsvarande riktiga kod, när vi vill jämföra

- [net/socket.c](https://codebrowser.dev/linux/linux/net/socket.c.html)
- [af_inet.c](https://codebrowser.dev/linux/linux/net/ipv4/af_inet.c.html)
- [fs/file.c](https://codebrowser.dev/linux/linux/fs/file.c.html)
- [ip_input.c](https://codebrowser.dev/linux/linux/net/ipv4/ip_input.c.html)
- [ip_output.c](https://codebrowser.dev/linux/linux/net/ipv4/ip_output.c.html)
- [ip_forward.c](https://codebrowser.dev/linux/linux/net/ipv4/ip_forward.c.html)
- [Netfilters hook-objekt](https://codebrowser.dev/linux/linux/include/linux/netfilter.h.html)
- [Netfilters hook-flöden och prioriteter](https://wiki.nftables.org/wiki-nftables/index.php/Netfilter_hooks)

Vår gemensamma genomgång börjar vid `tutorial_send()` i `tools/net/send_demo.c`.
Första kernelsteget att läsa är `__sys_sendto()` i `net/socket.c`. Vi följer en
enda funktionsövergång åt gången och förklarar namn och objekt när de dyker upp.
