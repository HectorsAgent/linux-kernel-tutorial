# linux-kernel-tutorial

Små, körbara modeller i C för att förstå Linux kernelarkitektur. Vi följer
Linux namn, objektgränser och kontrollflöden och förenklar detaljer som annars
gör det svårt att se huvudidén.

Projektet börjar med nätverksstacken. Fler områden, exempelvis processer,
minneshantering och filsystem, kan få egna mappar efter hand.

Modellerna körs i **user space**. De är undervisningsexempel med medvetna
förenklingar, inte kernelmoduler eller en komplett kernel.

## Nätverksstacken

[network/](network/) innehåller namespaces, IPv4-routing, enkla TCP-buffertar,
loopback/veth, Netfilter-hooks, conntrack, brandväggsregler och NAT.

Börja med [läsordningen och förenklingarna](network/README.md). De första
kodfilerna att följa är [ip_output.c](network/net/ipv4/ip_output.c) och
[ip_input.c](network/net/ipv4/ip_input.c).

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
