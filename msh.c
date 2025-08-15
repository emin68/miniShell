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


struct Redirs{
    char *in;  // '<'
    char *out; // '>' ou '>>'
    int append; //0 => '>' , 1 => '>>'
};


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

// Compacte cmds[] en retirant <, >, >> et les noms de fichiers.
// Remplit r->in / r->out / r->append.
// Laisse dans cmds[] uniquement la commande et ses vrais arguments.
static void parse_redirs(char **cmds, int *n, struct Redirs *r) {
    r->in = NULL; r->out = NULL; r->append = 0;

    int w = 0; // index d'écriture
    for (int i = 0; i < *n; i++) {
        if ((strcmp(cmds[i], ">") == 0 || strcmp(cmds[i], ">>") == 0)) {
            if (i + 1 >= *n) { fprintf(stderr, "syntax error: expected file after '>'\n"); *n = 0; break; }
            r->out = cmds[i + 1];
            r->append = (cmds[i][1] == '>');
            i++;             // saute le nom de fichier
            continue;        // ne recopie pas les tokens de redir
        }
        if (strcmp(cmds[i], "<") == 0) {
            if (i + 1 >= *n) { fprintf(stderr, "syntax error: expected file after '<'\n"); *n = 0; break; }
            r->in = cmds[i + 1];
            i++;
            continue;
        }
        cmds[w++] = cmds[i];
    }
    cmds[w] = NULL;
    *n = w;
}
static int only_space_tab(const char *s) {
    for (; *s; s++) if (*s != ' ' && *s != '\t') return 0;
    return 1;
}

int main(void){
    char *line = NULL; // Buffer pour stocker la ligne lue
    size_t buf_len=0;    // Capacité du buffer
    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;              // parent ignore Ctrl-C
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    char prev_cwd[PATH_MAX] = {0};

    while (1) {
        
        //pwd chemin courant
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof cwd)) {
            printf("msh:%s> ", cwd);
        } else {
            printf("msh> ");
        }
        fflush(stdout);

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
        if (line[0] == '\0' || only_space_tab(line)){
            continue; // ignore lignes vides / espaces
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
        if (strcmp(cmds[0], "cd") == 0) {
            char now[PATH_MAX];
            if (!getcwd(now, sizeof now)) strcpy(now, "");

            const char *dest = NULL;
            if (nb_cmds < 2) {
                dest = getenv("HOME"); //cd->home
                if (!dest) dest = "/";
            } else if (strcmp(cmds[1], "-") == 0) {
                if (prev_cwd[0] == '\0') {
                    fprintf(stderr, "cd: OLDPWD not set\n");
                    free(cmds);
                    continue;
                }
                dest = prev_cwd;
                puts(prev_cwd); // comme bash: affiche le nouveau chemin
            } else {
                dest = cmds[1];
            }

            if (chdir(dest) != 0) {
                perror("cd");
            } else {
                // maj prev_cwd si cd ok
                if (now[0] != '\0') strncpy(prev_cwd, now, sizeof prev_cwd - 1);
                prev_cwd[sizeof prev_cwd - 1] = '\0';
            }
            free(cmds);
            continue;
        }

        struct Redirs r = {0};
        parse_redirs(cmds, &nb_cmds, &r);
        if (nb_cmds == 0) { free(cmds); continue; } // ex: ">" seul, ou erreur



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
            // Redirections
            if (r.in) {
                int fd = open(r.in, O_RDONLY);
                if (fd < 0) { perror("open <"); _exit(126); }
                if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 <"); _exit(126); }
                close(fd);
            }
            if (r.out) {
                int flags = O_WRONLY | O_CREAT | (r.append ? O_APPEND : O_TRUNC);
                int fd = open(r.out, flags, 0666);
                if (fd < 0) { perror("open >"); _exit(126); }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 >"); _exit(126); }
                close(fd);
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
