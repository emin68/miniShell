#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

int main(void) {
    char *line = NULL; // Buffer pour stocker la ligne lue
    size_t buf_len = 0;    // Capacité du buffer

    while (1) {
        // 1. Afficher le prompt
        printf("msh> ");
        fflush(stdout); // Force l’affichage immédiat

        // 2. Lire une ligne avec getline()
        ssize_t nread = getline(&line, &buf_len, stdin);
        if (nread == -1) {
            // EOF (Ctrl-D) ou erreur de lecture
            if (feof(stdin)) break;      // sortie propre si Ctrl-D
            perror("getline");           // sinon affiche l'erreur
            continue;                    // et relance la boucle
        }


        // 3. Enlever le \n final
        if (nread > 0 && line[nread - 1] == '\n') {
            line[nread - 1] = '\0';
        }

        // 4) Réafficher ce qu’on a lu (test)
        printf(">> %s\n", line);

    }

    free(line); // Libérer la mémoire allouée par getline()
    return 0;
}
