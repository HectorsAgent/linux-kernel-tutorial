# linux-kernel-tutorial

Små, körbara modeller i C för att förstå Linux kernelarkitektur. Vi följer
Linux namn, objektgränser och kontrollflöden och förenklar detaljer som annars
gör det svårt att se huvudidén.

Projektet börjar med resan från en process syscall till nätverksstacken.
Kataloger och filgränser följer Linux
källträd: `net/`, `include/`, `drivers/` och `tools/` ligger direkt i roten.
Fler delar, exempelvis schemaläggning, minneshantering och filsystem, kan tillkomma
under sina vanliga Linux-kataloger, som `kernel/`, `mm/` och `fs/`.

Modellerna körs i **user space**. De är undervisningsexempel med medvetna
förenklingar, inte kernelmoduler eller en komplett kernel.

## Nätverksstacken

Modellen innehåller processer och FD-tabeller, socket-/protokoll-dispatch,
namespaces, IPv4-routing, enkla TCP-buffertar,
loopback/veth, Netfilter-hooks, conntrack, brandväggsregler och NAT.

Börja med [läsordningen och förenklingarna](Documentation/networking/tutorial.md).
Börja vid anropet till `tutorial_send()` i [send_demo.c](tools/net/send_demo.c).
Därifrån följer vi [__sys_sendto()](net/socket.c), filuppslaget och socketlagren
innan vi kommer till TCP och IP. Läs en funktion åt gången.

| Plats | Innehåll |
| --- | --- |
| `kernel/fork.c` | En liten processkonstruktor och modellens `current` |
| `fs/file.c` | Processens FD-tabell, filuppslag och filreferenser |
| `net/socket.c`, `net/ipv4/af_inet.c` | Syscall, socketobjekt och protokollval |
| `net/core/` | Paketbuffertar, interface, namespaces och neighbor-hantering |
| `net/ipv4/` | IPv4, routing och TCP:s separata sändnings-/mottagningsvägar |
| `net/netfilter/` | Hooks, conntrack, NAT och den enkla nftables-modellen |
| `drivers/net/` | Loopback, veth och en fiktiv Ethernet-drivrutin |
| `include/linux/`, `include/net/` | Objekt och API:er på sina Linux-liknande platser |
| `tools/net/send_demo.c` | Från processens syscall till en Ethernet-frame |
| `tools/net/nat_demo.c` | Containerexemplet med vidarebefordran och NAT |
| `tools/testing/selftests/net/` | Modellens tester |
| `Documentation/networking/` | Läsguide och förklaringar |

Endast delar som modellen använder finns med. Labbets egna storleksgränser,
konfigurationshjälpare och spårutskrifter ligger i `include/tutorial/` och
`lib/tutorial.c`. Dessa, demot och `drivers/net/tutorial.c` har inga exakta
motsvarigheter i Linux. Bygget använder en vanlig Makefile i stället för
Linux fullständiga Kbuild/Kconfig-system.

## Bygg och kör

Kräver en C11-kompilator och `make`. Inga externa bibliotek behövs.

```sh
git clone https://github.com/HectorsAgent/linux-kernel-tutorial.git
cd linux-kernel-tutorial
make demo
make nat-demo
make test
```

`make demo` följer processens `send(fd, "hej", 3, 0)` genom syscall, filobjekt,
socketlager, TCP, IP, neighbor-uppslag och Ethernet-drivrutinen. Drivrutinens
utkorg representerar överlämningen till kabeln. Även MAC-adresser och EtherType
finns med i framen; den serialiseras inte till ett riktigt wire-format.

`make nat-demo` fortsätter med en container, hostens NAT och ett svar från en
simulerad extern server. Spårutskrifterna visar funktioner, hooks, adresser,
portar och TTL. Inga riktiga nätverkspaket skickas och maskinens
nätverkskonfiguration ändras inte.

`make` bygger nätverksbiblioteket och båda demona. `make clean` tar bort byggfilerna.
