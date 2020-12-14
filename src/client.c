/****************************************
** client.c - a stream socket client demo
****************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

// the port client will be connecting to
#define PORT "3490"

// max number of bytes we can get at once
#define MAXDATASIZE 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

int main(int argc, char *argv[]) {

    int sockfd, numbytes;

    // buffer...
    char buf[MAXDATASIZE];

    struct addrinfo hints, *servinfo, *p;

    int rv;
    char s[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    // Kümmert sich darum, dass das struct leer ist
    memset(&hints, 0, sizeof hints);

    // Du kannst über das ai_family Feld erzwingen, dass IPv4, oder IPv6 benutzt wird,
    // oder benutzt einfach AF_UNSPEC um das jeweils Notwendige zu benutzen.
    hints.ai_family = AF_UNSPEC;

    // ai_socktype -> SOCK_STREAM oder SOCK_DGRAM
    hints.ai_socktype = SOCK_STREAM;

    // Diese Funktion arbeitet wirklich hart, sie bringt viele Optionen mit.Die structs, die
    // Du später benötigst, werden hier für die folgenden Funktionen vorbereitet.
    // Doch zuerst ein kleines bisschen Geschichte: Ursprünglich hat man eine Funktion
    // gethostbyname() benutzt um einen DNS Lookup zu machen. Die erhaltenen Informationen
    // hat man dann in das struct sockaddr_in eingetragen, welches dann an die entsprechenden
    // Funktionen übergeben wurde.
    // Das ist jedoch nicht länger nötig. Mittlerweile benutzt man
    // die Funktion getaddrinfo(), welche alle möglichen Dinge für Dich übernimmt, dazu
    // gehören DNS und Service Name Lookups. Außerdem werden auch direkt die structs, die
    // Du später brauchen wirst, mit den notwendigen Informationen gefüllt.

    // 1. Parameter: der Host Name bzw. IP Adresse, mit der man sich verbinden möchte.

    // 2. Parameter: Als nächstes gibt es den service Parameter, der eine Port Nummer
    // sein kann (Port 80, zum Beispiel), oder aber der Name zu einem bestimmten Service.
    // Eine Liste kann man entweder in /etc/services aufrufen, oder aber auf der IANA Port Liste
    //      http://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xhtml
    // nachsehen. Beispiele sind „http“, „ftp“, „telnet“, „smtp“ oder etwas anderes.

    // 3. Parameter: Zuletzt der hints Parameter, der auf ein struct addrinfo zeigt, welches bereits mit
    // relevanten Informationen gefüllt wurde.

    // 4. Parameter: Falls alles ordentlich funktioniert, zeigt servinfo auf eine
    // verkettete Liste von struct addrinfo-Elementen, wobei jedes einzelne dieser
    // Elemente ein struct sockaddr besitzt:
    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {

        // socket() gibt Dir einfach einen Socket-Deskriptor zurück,
        // den Du später für die system calls benutzen kannst, oder
        // aber -1, falls es einen Fehler gab.
        if ((sockfd = socket(
                p->ai_family, // PF_INET oder PF_INET6
                p->ai_socktype, // SOCK_STREAM oder SOCK_DGRAM
                p->ai_protocol) // protocol kann auf 0 gesetzt werden
            ) == -1) {
            perror("server: socket");
            continue;
        }

        // 1. Parameter: sockfd ist der Socket Datei-Deskriptor, den wir ja schon kennen, dieser wurde ja von socket()
        // zurückgegeben.
        // 2. Parameter: serv_addr ist ein struct sockaddr, der die Zieladresse und den Zielport angibt.
        // 3. Parameter: addrlen ist mal wieder die Länge der Server Adresse in Bytes.

        // Diese ganzen Informationen können wir als Rückgabewert von getaddrinfo() erhalten.
        if (connect(sockfd,
                    p->ai_addr,
                    p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }


    // Wichtig hierbei ist, dass Du die Adressfamilie sehen kannst, über das Feld
    // ai_family. Hierüber kannst Du dann sehen, ob es sich um eine AF_INET, oder
    // aber eine AF_INET6 Adresse handelt. Dann kannst Du es zu einem struct sockaddr_in,
    // oder einem struct sockaddr_in6 umwandeln,

    // inet_pton(): presentation to network
    //      * 192.2.3.4 > 000101010101...
    //      * String > in den binären Ausdruck
    //      * in_addr oder in6_addr > IPv4- oder IPv6-Darstellung
    // inet_ntop(): network to presentation
    //      * 000101010101... > 192.2.3.4
    //      * binärer Ausdruck > in String-Darstellung

    // 2. Parameter:
    //    sockaddr_storage > sockaddr > sockaddr_in oder sockaddr_in6
    inet_ntop(p->ai_family,
              get_in_addr((struct sockaddr *) p->ai_addr),
              s,
              sizeof s);

    printf("client: connecting to %s\n", s);

    // Am Ende, wenn wir fertig sind mit der verketteten Liste,
    // können und sollten wir den Speicher wieder frei machen,
    // ndem wir freeaddrinfo() aufrufen.
    freeaddrinfo(servinfo);


    // sockfd ist der Socket-Deskriptor von dem Du liest, buf ist der Buffer von dem die
    // Informationen erhältst, len ist die maximale Länge des Buffers und flags kann wieder
    // auf 0 gesetzt werden.

    // recv() gibt die Anzahl an in den Buffer gelesenen Bytes an, oder aber -1,
    // falls ein Fehler auftrat.

    // Achtung! recv() kann aber auch 0 zurückgeben und das kann wiederum nur eines bedeuten:
    // Die andere Seite hat die Verbindung zu Dir geschlossen.
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    buf[numbytes] = '\0';

    printf("client...: received %s\n", buf);

    close(sockfd);

    return 0;
}