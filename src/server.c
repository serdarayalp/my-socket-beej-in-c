/****************************************
** server.c - a stream socket server demo
****************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

// the port users will be connecting to
#define PORT "3490"

// how many pending connections queue will hold
#define BACKLOG 10

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr); // sin_addr = Internet address (IPv4)
    }
    return &(((struct sockaddr_in6 *) sa)->sin6_addr); // sin6_addr = Internet address (IPv6)
}

// sockfd ist das Socket an das Du die Daten schicken willst, buf ist der Buffer, der die Daten enthält
// und len ist ein Zeiger auf ein int, der die Anzahl an Byte im Buffer enthält.
// Aber was passiert auf der Seite des Empfangenden, wenn nur der Teil eines Pakets ankommt?
// Wenn die Pakete unterschiedlich lang sind, woher weiß dann der Empfänger, wo das eine Paket
// aufhört und das nächste beginnt?
int sendall(int sockfd, char *buffer, unsigned long *len) {
    // how many bytes we've sent
    int total = 0;

    // how many we have left to send
    unsigned long bytesleft = *len;

    int n;
    while (total < *len) {

        // send() gibt die Anzahl an Bytes zurück, die auch tatsächlich rausgeschickt werden.
        // Es kann sein, dass diese Zahl kleiner ist als die von Dir angegebene Zahl!
        // Das liegt daran, dass Du der Funktion manchmal einfach zu viele Daten gibst,
        // die sie verschicken soll, es aber nicht kann. Es werden dann so viele Daten
        // wie möglich verschickt und die Funktion erwartet von Dir, dass Du den fehlenden
        // Rest dann hinterher schickst. Ist der zurückgegebene Wert von send() ungleich
        // dem Wert in len, liegt es an Dir den Rest des Strings zu verschicken.

        // sockfd ist der Socket-Deskriptor, an den Du die Daten schicken
        // willst (ganz gleich, ob das der ist, den Du über socket() erhalten hast,
        // oder aber über accept()). msg ist ein Zeiger auf die Daten, die
        // Du verschicken möchtest und len ist die Länge der Daten in Byte.
        // Du kannst die flags einfach auf 0 setzen
        n = send(sockfd, buffer + total, bytesleft, 0);

        // Es wird -1 zurückgegeben, falls ein Fehler aufgetreten ist
        if (n == -1) {
            break;
        }

        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

int main(void) {

    // listen on sock_fd, new connection on new_fd
    int sockfd, new_fd;

    struct addrinfo hints, *servinfo, *p;

    // struct sockaddr_storage, das groß genug ist um sowohl IPv4 als auch IPv6 structs
    // zu halten. Es sieht nun mal so aus, dass Du bei manchen Aufrufen vorher nicht weißt,
    // ob struct sockaddr mit IPv4, oder aber IPv6 Adressen gefüllt wird.
    // Für diesen Fall übergibst Du einfach diese Struktur, die genau so aufgebaut ist
    // wie struct sockaddr, nur dass diese größer ist, dann wandelst Du es zu dem Typ um,
    // den Du brauchst. Wichtig hierbei ist, dass Du die Adressfamilie sehen kannst,
    // über das Feld ss_family – Hierüber kannst Du dann sehen, ob es sich um eine AF_INET,
    // oder aber eine AF_INET6 Adresse handelt. Dann kannst Du es zu einem struct sockaddr_in,
    // oder einem struct sockaddr_in6 umwandeln, wenn Du willst.
    struct sockaddr_storage their_addr;

    socklen_t sin_size;
    struct sigaction sa;

    // Startest Du ein Server Programm neu und rufst bind() auf, kann es sein, dass
    // dies nicht klappt. Es kommt die Fehlermeldung, dass der Port von einem anderen
    // Programm benutzt wird. Was hat das bitte zu bedeuten? Nun ja, das Socket, das
    // vorher verbunden war, hängt immer noch im Kernel und verstopft den Port.
    // Du kannst jetzt entweder warten, bis dieses automatisch vom Kernel bereinigt
    // wird, indem Du etwa eine Minute wartest, oder aber Du kannst den folgenden
    // Code zu Deinem Programm hinzufügen, sodass es wieder möglich ist, dass Du den
    // Port benutzen kannst:
    int yes = 1;

    // INET_ADDRSTRLEN > IPv4
    // INET6_ADDRSTRLEN > IPv6
    char s[INET6_ADDRSTRLEN];

    int rv;

    // Kümmert sich darum, dass das struct leer ist
    memset(&hints, 0, sizeof hints);

    // Du kannst über das ai_family Feld erzwingen, dass IPv4, oder IPv6 benutzt wird,
    // oder benutzt einfach AF_UNSPEC um das jeweils Notwendige zu benutzen.
    hints.ai_family = AF_UNSPEC;

    // ai_socktype -> SOCK_STREAM oder SOCK_DGRAM
    hints.ai_socktype = SOCK_STREAM;

    // Durch das benutzen des AI_PASSIVE Flags teilt man dem Programm mit, dass es sich
    // an die IP Adresse des Hosts binden soll, auf dem es läuft. Wenn Du das Programm
    // an eine spezielle lokale IP Adresse binden möchtest, kannst Du das AI_PASSIVE
    // Flag einfach weglassen und dafür die IP Adresse als erstes Argument
    // an getaddrinfo() übergeben.
    hints.ai_flags = AI_PASSIVE; // use my IP


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
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {

        // Dann machen wir den Aufruf. Falls ein Fehler auftritt (getaddrinfo() gibt
        // eine Zahl != 0 zurück), können wir das ausgeben, indem wir die Funktion
        // gai_strerror() benutzen.
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
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

        // Startest Du ein Server Programm neu und rufst bind() auf, kann es sein,
        // dass dies nicht klappt. Es kommt die Fehlermeldung, dass der Port von
        // einem anderen Programm benutzt wird. Was hat das bitte zu bedeuten?
        // Nun ja, das Socket, das vorher verbunden war, hängt immer noch im Kernel
        // und verstopft den Port. Du kannst jetzt entweder warten, bis dieses
        // automatisch vom Kernel bereinigt wird, indem Du etwa eine Minute wartest,
        // oder aber Du kannst den folgenden Code zu Deinem Programm hinzufügen,
        // sodass es wieder möglich ist, dass Du den Port benutzen kannst:
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        // Wenn Du dann erst mal das Socket hast, musst Du es erst einmal mit einem
        // Port auf Deinem Computer assoziieren. Dies wird üblicherweise gemacht,
        // wenn Du über die Funktion listen() auf eingehende Verbindungen
        // auf einem vorher definierten Port wartest. Die Portnummer wird benötigt,
        // damit der Kernel ankommende Pakete einem gewissen Prozess zuordnen
        // können (genauer: dem Datei-Deskriptor eines Prozesses).
        if (bind(sockfd,
                 p->ai_addr, // ist ein Zeiger, der Informationen über Deine Adresse hat, also den Port und die IP Adresse.
                 p->ai_addrlen
        ) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    // Am Ende, wenn wir fertig sind mit der verketteten Liste,
    // können und sollten wir den Speicher wieder frei machen,
    // ndem wir freeaddrinfo() aufrufen.
    freeaddrinfo(servinfo);

    // backlog ist die Anzahl an eingehenden Verbindung in der Warteschleife,
    // die erlaubt werden. Das bedeutet, dass eingehende Verbindungen in einer
    // Warteschleife verweilen, bis sie akzeptiert werden (durch accept()) und
    // hierfür setzt man ein Limit an Verbindungen, die in dieser Warteschleife
    // verweilen dürfen. Die meisten Systeme setzen diesen Wert auf 20, aber
    // Du solltest auch mit 5 oder 10 zurecht kommen.
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    // der Aufruf von sigaction() ist neu. Der Code wird benutzt um Prozesse zu beenden,
    // die nutzlos sind, nachdem ein Kind-Prozess (erstellt über fork()) beendet wurde.
    // Wenn Du diese Zombie-Prozesse nicht beendest und den Platz wieder nicht frei machst,
    // bekommst Du Probleme mit Deinem System-Administrator.
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    // main accept() loop
    while (1) {
        sin_size = sizeof their_addr;

        // Jemand, der weit entfernt sitzt, möchte sich mit Deiner Maschine
        // auf einen gewissen Port verbinden (über den Aufruf connect()). Mittels listen() nimmst
        // Du diese Verbindungsaufnahme wahr. Die Verbindung kommt in die Warteschlange und wartet
        // nun darauf, dass sie akzeptiert wird. Du rufst also accept() auf und sagst der Funktion,
        // dass sie die ausstehende Verbindung aufnehmen soll. Sie wird Dir einen brandneuen Socket
        // Datei-Deskriptor zurückgeben um mit dieser einen Verbindung zu arbeiten. Ganz genau,
        // auf einmal hast Du zwei Socket Datei-Deskriptoren. Der Ursprüngliche
        // wartet weiterhin auf eingehende Verbindungen, wohingegen der neue genutzt werden kann,
        // um Sachen zu verschicken send() und zu erhalten recv().

        // sockfd ist der Socket-Deskriptor, der auf eingehende Verbindungen wartet.
        // addr ist für gewöhnlich ein Zeiger zu dem lokalen
        // struct sockaddr_storage. Hier werden die Informationen über die eingehende
        // Verbindungen gehalten, hiermit kannst Du herausfinden, welcher Host
        // Dich aufruft und über welchen Port dies geschieht. addrlen ist ein lokaler
        // Integer, welcher auf sizeof(struct sockaddr_storage) gesetzt werden sollte,
        // bevor seine Adresse an accept() weitergegeben wird. accept() wird nämlich
        // nicht mehr Bytes in addr reinpacken. Falls weniger reingepackt werden, wird
        // der Wert von addrlen entsprechend verändert.
        new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }


        // their_addr:
        // Wichtig hierbei ist, dass Du die Adressfamilie sehen kannst, über das Feld
        // ss_family. Hierüber kannst Du dann sehen, ob es sich um eine AF_INET, oder
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
        inet_ntop(their_addr.ss_family, // IPv4 oder IPv6
                  get_in_addr((struct sockaddr *) &their_addr), // Adresse
                  s, // ein Zeiger auf den String, der das Ergebnis enthalten soll
                  sizeof s); // die maximale Länge des Strings

        printf("server: got connection from %s\n", s);

        // fork():
        // Clone the calling process, creating an exact copy.
        // Return -1 for errors, 0 to the new process,
        // and the process ID of the new process to the old process.
        if (!fork()) {

            // this is the child process
            close(sockfd); // child doesn't need the listener

            char *buf = "\n\nHello, world!\n\n";
            unsigned long len, bytes_sent;
            len = strlen(buf);

            // send() gibt die Anzahl an Bytes zurück, die auch tatsächlich rausgeschickt werden.
            // Es kann sein, dass diese Zahl kleiner ist als die von Dir angegebene Zahl!
            // Das liegt daran, dass Du der Funktion manchmal einfach zu viele Daten gibst,
            // die sie verschicken soll, es aber nicht kann. Es werden dann so viele Daten
            // wie möglich verschickt und die Funktion erwartet von Dir, dass Du den fehlenden
            // Rest dann hinterher schickst. Ist der zurückgegebene Wert von send() ungleich
            // dem Wert in len, liegt es an Dir den Rest des Strings zu verschicken.

            // sockfd ist der Socket-Deskriptor, an den Du die Daten schicken
            // willst (ganz gleich, ob das der ist, den Du über socket() erhalten hast,
            // oder aber über accept()). msg ist ein Zeiger auf die Daten, die
            // Du verschicken möchtest und len ist die Länge der Daten in Byte.
            // Du kannst die flags einfach auf 0 setzen

            /*
            if ((bytes_sent = send(new_fd, buf, len, 0)) == -1) {
                perror("send");
            }
            printf("sent data: %lu Bytes\n", bytes_sent);
            */

            if (sendall(new_fd, buf, &len) == -1) {
                perror("sendall");
                printf("We only sent %lu bytes because of the error!\n", len);
            }
            printf("sent data...: %lu Bytes\n", len);

            close(new_fd);
            exit(0);
        }

        close(new_fd);  // parent doesn't need this
    }

    return 0;
}