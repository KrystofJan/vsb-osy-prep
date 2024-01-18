# My preparation for OS exam
## SemaforServer
Síťový semafor. Server si vytvoří semafor a od klienta přijímá příkazy "UP" a "DOWN" a odpovídá na ně "UP-OK" nebo "DOWN-OK". 

## Simultalka
Server hraje simultánku se třemi klienty. Klienti se připojí a posílají na server např. jednoduché matematické příklady: "1+3", "38*22", ... a sever vždy odpoví jednomu klientu na 10 dotazů a pak se věnuje dalšímu. Přepínání pomocí 3 semaforů. 

## HTTPServer
Internetový prohlížeč zašle na sever příkaz: "ls -la /etc" ve tvaru "http://localhost:port/ls*-la*/etc", kde "*" je oddělovač. Server přijme request: "GET /ls*-la*/etc HTTP/1.1" a je potřeba text mezi GET a HTTP interpretovat jako příkaz s parametry, provést v potomkovi pomocí exec a výstup přesměrovat do soketu. Semafor hlídá, aby se prováděl v jeden okamžik jen jediný příkaz.Příklad HTTP Respose hlavičky bude v zadání. Příklad najdete ve svém prohlížeči v "develop tools". 

## RemoteShell
Remote shell. Podobně jako předchozí příklad, jen příkaz bude zadán z textového klienta, na serveru interpretován a výstup přesměrován do soketu. Přijatá data klient zobrazí i uloží do souboru.

## Test
In test/zadani/zadani.txt
