#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>


// Découpe 'ligne' en mots séparés par espace/tab
// Stocke les pointeurs dans **cmds
// Retourne aussi le nombre de mots dans *nb_cmds
// cmds[i] pointe directement dans 'ligne' (modifié par strtok_r)
static void recup_commande(char *ligne, char ***cmds, int *nb_cmds){
    size_t cap = 8;  // capacité initiale
    int n = 0;       // nombre de mots trouvés
    char **tab = malloc(cap * sizeof(char *));
    if (!tab) { perror("malloc"); exit(1); }

    char *pos;  // pointeur de progression
    char *mot = strtok_r(ligne, " \t", &pos);

    while (mot) {
        if (n + 1 >= (int)cap) { // +1 pour le NULL final
            cap *= 2;
            char **tmp = realloc(tab, cap * sizeof(char *));
            if (!tmp) { free(tab); perror("realloc"); exit(1); }
            tab = tmp;
        }
        tab[n++] = mot;
        mot = strtok_r(NULL, " \t", &pos);
    }
    tab[n] = NULL; // pour execvp

    *cmds = tab;
    *nb_cmds = n;
}


int main(void){
    char *line = NULL; // Buffer pour stocker la ligne lue
    size_t buf_len=0;    // Capacité du buffer
    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;              // parent ignore Ctrl-C
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);


    while (1) {
        // 1. Afficher le prompt
        printf("msh> ");
        fflush(stdout); // Force l’affichage immédiat

        // 2. Lire une ligne avec getline()
        ssize_t nread = getline(&line, &buf_len, stdin);
        if (nread == -1){
            // EOF (Ctrl-D) ou erreur de lecture
            if (feof(stdin)) break;      // sortie propre si Ctrl-D
            perror("getline");           // sinon affiche l'erreur
            continue;                    // et relance la boucle
        }


        // 3. Enlever le \n final
        if (nread > 0 && line[nread - 1] == '\n'){
            line[nread - 1] = '\0';
        }
        
        //tokenisation
        char **cmds = NULL;
        int nb_cmds = 0;
        recup_commande(line, &cmds, &nb_cmds);

        if (nb_cmds == 0){ 
            free(cmds);
            continue; // ligne vide
        }
        
        //si "exit" on quitte le shell
        if (strcmp(cmds[0], "exit") == 0){
            free(cmds);
            break; // quitte le shell
        }
        //commande cd
        if (strcmp(cmds[0], "cd") == 0){
            const char *dest = NULL;
            if (nb_cmds < 2){
                dest = getenv("HOME");  // cd → HOME
                if (!dest) dest = "/";  // fallback
            }
            else{
                dest = cmds[1];
            }

            if (chdir(dest) != 0){
                perror("cd");
            }
            free(cmds);
            continue; // on ne fork pas, on repart au prompt
        }
        //pwd chemin courant
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof cwd)) {
            printf("msh:%s> ", cwd);
        } else {
            printf("msh> ");
        }
        fflush(stdout);

        // — redirection de sortie ">"
        char *outfile = NULL;   // nom du fichier après '>'

        // on compacte cmds[] "sur place" : on enlève '>' et le fichier
        int w = 0;              // index d'écriture
        for (int i = 0; i < nb_cmds; i++) {
            if (strcmp(cmds[i], ">") == 0) {
                if (i + 1 >= nb_cmds) {
                    fprintf(stderr, "syntax error: expected file after '>'\n");
                    nb_cmds = 0;           // invalide la commande
                    break;
                }
                outfile = cmds[i + 1];     // on retient le fichier
                i++;                       // on saute aussi le nom de fichier
                continue;                  // on NE garde PAS '>' ni le fichier dans cmds
            }
            cmds[w++] = cmds[i];           // sinon on garde l’argument
        }
        cmds[w] = NULL;
        nb_cmds = w;

        if (nb_cmds == 0) {                // ex: ">" tout seul → rien à exécuter
            free(cmds);
            continue;
        }

        //initialise un processus
        pid_t pid = fork(); 
        if (pid < 0){
            perror("fork");
            free(cmds);
            continue;
        }

        if (pid == 0){
            signal(SIGINT, SIG_DFL);// l'enfant retrouve le comportement normal
            signal(SIGQUIT, SIG_DFL);
            // Enfant : exécute la commande
            if (outfile) {
                int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd < 0) { perror("open >"); _exit(126); }

                if (dup2(fd, STDOUT_FILENO) < 0) { // remplace stdout (1) par le fichier
                    perror("dup2 >");
                    _exit(126);
                }
                close(fd); // on peut fermer: stdout pointe déjà vers fd
            }
            execvp(cmds[0], cmds);
            // Si on arrive ici, exec a échoué
            perror("execvp");
            _exit(127);
        }
        else{
            // Parent : attendre l’enfant pour éviter les zombies
            int status = 0;
            if (waitpid(pid, &status, 0) < 0) {
                perror("waitpid");
            }
        }

        free(cmds);

    }

    free(line); // Libérer la mémoire allouée par getline()
    return 0;
}
