# miniShell


Petit shell en C, lit une ligne, la découpe en arguments, gère quelques built-ins, applique les redirections, puis exécute la commande via fork + execvp. Le prompt affiche le répertoire courant.



FONCTIONNALITES

- Prompt avec chemin courant (msh:/chemin/actuel> )

- Exécution de commandes externes

- Built-ins :

 -- exit — quitter le shell

 -- cd <dir> pour changer de dossier

 -- cd - — revenir au dossier précédent

- Redirections :

 -- cmd > f.txt — écrase/écrit dans un fichier

 -- cmd >> f.txt — ajoute à la fin d’un fichier

 -- cmd < f.txt — lit l’entrée depuis un fichier

- Gestion des signaux :

- Le shell ignore Ctrl-C/Ctrl-\ (parent)

- Les commandes, elles, reprennent le comportement normal



COMPILATION

- Avec gcc : 
- gcc -std=c17 -Wall -Wextra -O2 -o msh msh.c

- Utilisation : 
./msh


- Avec le Makefile :
make run



EXEMPLES : 

ls -l

echo "hello" > out.txt

echo "again" >> out.txt

cat < out.txt

cd /tmp

cd -

exit



