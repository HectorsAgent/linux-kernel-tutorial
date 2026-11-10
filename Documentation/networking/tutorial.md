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
make test
```

`make` bygger även `build/liblearnnet.a`.

Demot har en container, en host som router och en simulerad extern server.
Containern skickar `hej` till servern och får `svar` tillbaka. Bara hosten har
conntrack/NAT. Alla namespaces skriver ut funktionerna och hooks som paketet
passerar, inklusive adresser, portar och TTL efter översättningar.

Containeradress: `10.0.0.2`. Hostens LAN: `10.0.0.1`. Hostens externa adress:
`203.0.113.10`. Server: `203.0.113.20`. Servern ligger direkt på den simulerade
externa länken för att vi ska slippa ytterligare en router. Adresserna
`203.0.113.x` används här som dokumentationsadresser.

## Börja läsa här

1. [net/ipv4/ip_output.c](../../net/ipv4/ip_output.c): motsvarigheten till din
   `send_ip()`, nu uppdelad i Linux funktioner. Börja längst ned med
   `ip_queue_xmit()`, följ sedan `ip_local_out()` och `ip_output()`.
2. [net/ipv4/ip_input.c](../../net/ipv4/ip_input.c): börja med `ip_rcv()` längst ned.
   Efter PREROUTING hittar routingen nästa funktion genom `dst_input()`.
3. [net/ipv4/route.c](../../net/ipv4/route.c): `local` före `main`, längsta prefix,
   samt hur en route får `ip_local_deliver` eller `ip_forward` som fortsättning.
4. [net/ipv4/ip_forward.c](../../net/ipv4/ip_forward.c): vidarebefordran och TTL.
   Notera att funktionen aldrig bygger en ny IP-header eller anropar TCP.
5. [net/netfilter/core.c](../../net/netfilter/core.c): våra "middlewares".
   `NF_HOOK()` skapar kontexten, `nf_hook_slow()` kör funktionerna i
   prioritetsordning och sedan körs `okfn()` om alla accepterade.
6. [tools/net/nat_demo.c](../../tools/net/nat_demo.c): topologi och regelkonfiguration.

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

**Lokalt skapat:** `tcp_sendmsg` → `ip_queue_xmit` → initial routing →
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
- En tråd, fasta tabellstorlekar och linjära uppslag. Inga lås, RCU, refcounts,
  per-CPU-data, trie eller delade minnesbuffertar. Structar med pekare ska inte
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
- Processer, FD-tabeller och syscall-lagret är utanför detta bibliotek.
  Vi börjar direkt vid en lokal socket för att hålla fokus på nätverksstacken.

## Verifiering

`make test` kör tolv scenarier: lokal TCP över lo till en eth0-adress,
byteström och segmentering, segment i fel ordning, längsta-prefix-routing,
neighbor-nyckel per interface, forwarding utan conntrack, TTL, SNAT tur/retur,
separat conntrack i båda namespaces, brandvägg och sena conntrack-bekräftelser,
UDP-portkollisioner och svar, bevarade NAT-mappningar efter regeländring,
DNAT med omvänd översättning, OUTPUT-DNAT med omrouting samt tidig NF_DROP.

Minneskontroll med GCC/Clang:

```sh
make BUILD=build-asan CFLAGS='-std=c11 -Wall -Wextra -Werror -Wpedantic -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' test
```

## Motsvarande riktiga kod, när vi vill jämföra

- [ip_input.c](https://codebrowser.dev/linux/linux/net/ipv4/ip_input.c.html)
- [ip_output.c](https://codebrowser.dev/linux/linux/net/ipv4/ip_output.c.html)
- [ip_forward.c](https://codebrowser.dev/linux/linux/net/ipv4/ip_forward.c.html)
- [Netfilters hook-objekt](https://codebrowser.dev/linux/linux/include/linux/netfilter.h.html)
- [Netfilters hook-flöden och prioriteter](https://wiki.nftables.org/wiki-nftables/index.php/Netfilter_hooks)

Vårt nästa designsteg kan tas direkt i `ip_output.c`: där syns varför OUTPUT
och POSTROUTING är två olika punkter, och varför forwarding bara passerar den
senare av dem.
