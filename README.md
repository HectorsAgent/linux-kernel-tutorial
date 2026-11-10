# linux-kernel-tutorial

Små, körbara modeller i C för att förstå Linux kernelarkitektur. Vi följer
Linux namn, objektgränser och kontrollflöden och förenklar detaljer som annars
gör det svårt att se huvudidén.

Projektet börjar med nätverksstacken. Kataloger och filgränser följer Linux
källträd: `net/`, `include/`, `drivers/` och `tools/` ligger direkt i roten.
Fler områden, exempelvis processer, minneshantering och filsystem, kan tillkomma
under sina vanliga Linux-kataloger, som `kernel/`, `mm/` och `fs/`.

Modellerna körs i **user space**. De är undervisningsexempel med medvetna
förenklingar, inte kernelmoduler eller en komplett kernel.

## Nätverksstacken

Modellen innehåller namespaces, IPv4-routing, enkla TCP-buffertar,
loopback/veth, Netfilter-hooks, conntrack, brandväggsregler och NAT.

Börja med [läsordningen och förenklingarna](Documentation/networking/tutorial.md).
De första kodfilerna att följa är [ip_output.c](net/ipv4/ip_output.c) och
[ip_input.c](net/ipv4/ip_input.c).

| Plats | Innehåll |
| --- | --- |
| `net/core/` | Paketbuffertar, interface, namespaces och neighbor-hantering |
| `net/ipv4/` | IPv4, routing och TCP:s separata sändnings-/mottagningsvägar |
| `net/netfilter/` | Hooks, conntrack, NAT och den enkla nftables-modellen |
| `drivers/net/` | Loopback, veth och en fiktiv Ethernet-drivrutin |
| `include/linux/`, `include/net/` | Objekt och API:er på sina Linux-liknande platser |
| `tools/net/nat_demo.c` | Det körbara containerexemplet |
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
git clone https://github.com/HectorBjernersjo/linux-kernel-tutorial.git
cd linux-kernel-tutorial
make demo
make test
```

Demot följer ett paket från en container genom hostens NAT och tillbaka från
en simulerad extern server. Spårutskriften visar funktioner, hooks, adresser,
portar och TTL i varje namespace. Inga riktiga nätverkspaket skickas och
maskinens nätverkskonfiguration ändras inte.

`make` bygger nätverksbiblioteket och demot. `make clean` tar bort byggfilerna.
