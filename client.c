#include "messaging.h"
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>

// Variables globales
static int g_sockfd = -1;
static char g_username[MAX_USERNAME];
static char g_current_group[MAX_GROUP_NAME];
static char g_user_color[16] = COLOR_GREEN;
static struct sockaddr_in g_server_addr;
static int g_running = 1;
static SharedMemory *g_shm = NULL;
static int g_shmid = -1;
static int g_semid = -1;

void cleanup_and_exit(int signum) {
    printf("\n\nDéconnexion...\n");
    
    // Envoyer message de déconnexion
    if (g_sockfd >= 0 && strlen(g_username) > 0) {
        Message msg;
        message_create(&msg, MSG_DISCONNECT, g_username, NULL, NULL, "");
        socket_send(g_sockfd, &msg, &g_server_addr);
    }
    
    g_running = 0;
    
    if (g_sockfd >= 0) {
        close(g_sockfd);
    }
    
    if (g_shm != NULL) {
        shm_detach(g_shm);
    }
    
    printf("Au revoir!\n");
    exit(0);
}

void* receive_thread(void *arg) {
    Message msg;
    struct sockaddr_in src_addr;
    
    while (g_running) {
        ssize_t n = socket_receive(g_sockfd, &msg, &src_addr);
        
        if (n > 0) {
            printf("\n");
            message_display(&msg, g_user_color);
            display_prompt(g_username, g_current_group, g_user_color);
        }
        
        usleep(10000); // 10 ms
    }
    
    return NULL;
}

void print_help(void) {
    printf("\n=== COMMANDES DISPONIBLES ===\n");
    printf("  /join <groupe>         - Rejoindre un groupe\n");
    printf("  /leave                 - Quitter le groupe actuel\n");
    printf("  /msg <user> <message>  - Envoyer un message privé\n");
    printf("  /create <groupe>       - Créer un nouveau groupe\n");
    printf("  /merge <g1> <g2>       - Fusionner deux groupes\n");
    printf("  /users                 - Lister les utilisateurs\n");
    printf("  /groups                - Lister les groupes\n");
    printf("  /color <couleur>       - Changer la couleur du prompt\n");
    printf("                          (red, green, yellow, blue, magenta, cyan, white)\n");
    printf("  /clear                 - Effacer l'écran\n");
    printf("  /help                  - Afficher cette aide\n");
    printf("  /quit                  - Quitter\n");
    printf("\n");
}

int process_command(char *input) {
    // Supprimer le retour à la ligne
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
        len--;
    }
    
    if (len == 0) {
        return 0;
    }
    
    // Commandes
    if (input[0] == '/') {
        char cmd[32], arg1[MAX_USERNAME], arg2[MAX_USERNAME];
        
        if (strcmp(input, "/quit") == 0) {
            cleanup_and_exit(0);
        }
        else if (strcmp(input, "/help") == 0) {
            print_help();
        }
        else if (strcmp(input, "/clear") == 0) {
            clear_screen();
        }
        else if (strcmp(input, "/users") == 0) {
            if (g_shm != NULL) {
                sem_p(g_semid);
                display_users(g_shm);
                sem_v(g_semid);
            }
        }
        else if (strcmp(input, "/groups") == 0) {
            if (g_shm != NULL) {
                sem_p(g_semid);
                display_groups(g_shm);
                sem_v(g_semid);
            }
        }
        else if (strcmp(input, "/leave") == 0) {
            if (strlen(g_current_group) == 0) {
                printf("Vous n'êtes dans aucun groupe\n");
            } else {
                Message msg;
                message_create(&msg, MSG_LEAVE, g_username, NULL, g_current_group, "");
                socket_send(g_sockfd, &msg, &g_server_addr);
                g_current_group[0] = '\0';
            }
        }
        else if (sscanf(input, "/join %s", arg1) == 1) {
            // Quitter le groupe actuel si nécessaire
            if (strlen(g_current_group) > 0) {
                Message leave_msg;
                message_create(&leave_msg, MSG_LEAVE, g_username, NULL, g_current_group, "");
                socket_send(g_sockfd, &leave_msg, &g_server_addr);
            }
            
            // Rejoindre le nouveau groupe
            Message msg;
            message_create(&msg, MSG_JOIN, g_username, NULL, arg1, "");
            socket_send(g_sockfd, &msg, &g_server_addr);
            strncpy(g_current_group, arg1, MAX_GROUP_NAME - 1);
            g_current_group[MAX_GROUP_NAME - 1] = '\0';
            
            printf("Tentative de connexion au groupe %s...\n", arg1);
        }
        else if (sscanf(input, "/create %s", arg1) == 1) {
            Message msg;
            message_create(&msg, MSG_CREATE_GROUP, g_username, NULL, NULL, arg1);
            socket_send(g_sockfd, &msg, &g_server_addr);
            printf("Demande de création du groupe %s...\n", arg1);
        }
        else if (sscanf(input, "/merge %s %s", arg1, arg2) == 2) {
            char content[MAX_MESSAGE];
            snprintf(content, MAX_MESSAGE, "%s:%s", arg1, arg2);
            
            Message msg;
            message_create(&msg, MSG_MERGE_GROUPS, g_username, NULL, NULL, content);
            socket_send(g_sockfd, &msg, &g_server_addr);
            printf("Demande de fusion des groupes %s et %s...\n", arg1, arg2);
        }
        else if (sscanf(input, "/color %s", arg1) == 1) {
            const char *color = COLOR_GREEN;
            
            if (strcmp(arg1, "red") == 0) color = COLOR_RED;
            else if (strcmp(arg1, "green") == 0) color = COLOR_GREEN;
            else if (strcmp(arg1, "yellow") == 0) color = COLOR_YELLOW;
            else if (strcmp(arg1, "blue") == 0) color = COLOR_BLUE;
            else if (strcmp(arg1, "magenta") == 0) color = COLOR_MAGENTA;
            else if (strcmp(arg1, "cyan") == 0) color = COLOR_CYAN;
            else if (strcmp(arg1, "white") == 0) color = COLOR_WHITE;
            else {
                printf("Couleur inconnue. Choix: red, green, yellow, blue, magenta, cyan, white\n");
                return 0;
            }
            
            strncpy(g_user_color, color, sizeof(g_user_color) - 1);
            g_user_color[sizeof(g_user_color) - 1] = '\0';
            
            if (g_shm != NULL) {
                sem_p(g_semid);
                user_set_color(g_shm, g_username, color);
                sem_v(g_semid);
            }
            
            printf("Couleur changée en %s\n", arg1);
        }
        else if (strncmp(input, "/msg ", 5) == 0) {
            // Format: /msg username message
            char *space = strchr(input + 5, ' ');
            if (space == NULL) {
                printf("Usage: /msg <utilisateur> <message>\n");
                return 0;
            }
            
            *space = '\0';
            char *recipient = input + 5;
            char *content = space + 1;
            
            if (strlen(content) == 0) {
                printf("Message vide\n");
                *space = ' ';
                return 0;
            }
            
            Message msg;
            message_create(&msg, MSG_PRIVATE, g_username, recipient, NULL, content);
            socket_send(g_sockfd, &msg, &g_server_addr);
            
            printf("%s[PRIVÉ à %s]: %s%s\n", 
                   COLOR_MAGENTA, recipient, content, COLOR_RESET);
            
            *space = ' ';
        }
        else {
            printf("Commande inconnue. Tapez /help pour l'aide.\n");
        }
    }
    else {
        // Message public
        if (strlen(g_current_group) == 0) {
            printf("Vous devez rejoindre un groupe d'abord (/join <groupe>)\n");
            return 0;
        }
        
        Message msg;
        message_create(&msg, MSG_PUBLIC, g_username, NULL, g_current_group, input);
        socket_send(g_sockfd, &msg, &g_server_addr);
    }
    
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <username> <server_ip> [server_port]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    char *username = argv[1];
    char *server_ip = argv[2];
    int server_port = PORT_BASE;
    
    if (argc > 3) {
        server_port = atoi(argv[3]);
    }
    
    // Copier le nom d'utilisateur
    strncpy(g_username, username, MAX_USERNAME - 1);
    g_username[MAX_USERNAME - 1] = '\0';
    
    printf("=== CLIENT DE MESSAGERIE ===\n");
    printf("Utilisateur: %s\n", g_username);
    printf("Serveur: %s:%d\n\n", server_ip, server_port);
    
    // Installer le gestionnaire de signal
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    
    // Attacher à la mémoire partagée
    key_t shm_key = ftok(SHM_KEY_FILE, '1');
    if (shm_key != -1) {
        g_shmid = shmget(shm_key, sizeof(SharedMemory), 0);
        if (g_shmid != -1) {
            shm_attach(g_shmid, &g_shm);
            
            key_t sem_key = ftok(SEM_KEY_FILE, '1');
            if (sem_key != -1) {
                g_semid = semget(sem_key, 1, 0);
            }
        }
    }
    
    // Créer le socket
    g_sockfd = socket_create_udp();
    if (g_sockfd == -1) {
        return EXIT_FAILURE;
    }
    
    // Configurer l'adresse du serveur
    memset(&g_server_addr, 0, sizeof(g_server_addr));
    g_server_addr.sin_family = AF_INET;
    g_server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_ip, &g_server_addr.sin_addr) <= 0) {
        perror("Adresse IP invalide");
        return EXIT_FAILURE;
    }
    
    // Configurer le socket en mode non-bloquant
    int flags = fcntl(g_sockfd, F_GETFL, 0);
    fcntl(g_sockfd, F_SETFL, flags | O_NONBLOCK);
    
    // Créer le thread de réception
    pthread_t recv_thread_id;
    if (pthread_create(&recv_thread_id, NULL, receive_thread, NULL) != 0) {
        perror("Erreur pthread_create");
        return EXIT_FAILURE;
    }
    
    printf("Connecté! Tapez /help pour l'aide.\n\n");
    
    // Boucle principale d'entrée
    char input[MAX_MESSAGE];
    
    while (g_running) {
        display_prompt(g_username, g_current_group, g_user_color);
        
        if (fgets(input, sizeof(input), stdin) != NULL) {
            process_command(input);
        }
    }
    
    pthread_join(recv_thread_id, NULL);
    cleanup_and_exit(0);
    
    return 0;
}
