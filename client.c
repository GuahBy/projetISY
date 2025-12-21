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
static int g_connected_to_server = 0;
static char g_list_buffer[MAX_MESSAGE * 4];
static int g_list_received = 0;

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

            // Gérer les cas spéciaux avant d'afficher
            if (msg.type == MSG_CONNECT_ACK) {
                // Accusé de réception de connexion
                g_connected_to_server = 1;
            } else if (msg.type == MSG_JOIN && strcmp(msg.sender, g_username) == 0) {
                // Confirmation de connexion au groupe
                strncpy(g_current_group, msg.group, MAX_GROUP_NAME - 1);
                g_current_group[MAX_GROUP_NAME - 1] = '\0';

                // Parser le contenu pour extraire le statut et la couleur
                // Format: "STATUS:COLOR" ou "STATUS"
                char status[32] = "";
                char color_name[32] = "green";  // Par défaut

                if (sscanf(msg.content, "%[^:]:%s", status, color_name) < 1) {
                    strncpy(status, msg.content, sizeof(status) - 1);
                }

                // Convertir le nom de couleur en code ANSI
                const char *color_code = COLOR_GREEN;
                if (strcmp(color_name, "red") == 0) color_code = COLOR_RED;
                else if (strcmp(color_name, "green") == 0) color_code = COLOR_GREEN;
                else if (strcmp(color_name, "yellow") == 0) color_code = COLOR_YELLOW;
                else if (strcmp(color_name, "blue") == 0) color_code = COLOR_BLUE;
                else if (strcmp(color_name, "magenta") == 0) color_code = COLOR_MAGENTA;
                else if (strcmp(color_name, "cyan") == 0) color_code = COLOR_CYAN;
                else if (strcmp(color_name, "white") == 0) color_code = COLOR_WHITE;

                // Appliquer la couleur
                strncpy(g_user_color, color_code, sizeof(g_user_color) - 1);
                g_user_color[sizeof(g_user_color) - 1] = '\0';

                // Afficher le message approprié en fonction du statut
                if (strcmp(status, "CREATED") == 0) {
                    printf("%sGroupe '%s' créé avec succès. Vous êtes administrateur.%s\n",
                           COLOR_GREEN, msg.group, COLOR_RESET);
                } else {
                    printf("%sConnecté au groupe '%s'%s\n", COLOR_GREEN, msg.group, COLOR_RESET);
                }
            } else if (msg.type == MSG_LEAVE) {
                // Message de kick ou de leave
                if (strcmp(msg.sender, "Serveur") == 0) {
                    // C'est un kick du serveur
                    g_current_group[0] = '\0';
                    // Réinitialiser la couleur à vert par défaut
                    strncpy(g_user_color, COLOR_GREEN, sizeof(g_user_color) - 1);
                    g_user_color[sizeof(g_user_color) - 1] = '\0';
                }
                // Afficher le message
                message_display(&msg, g_user_color);
            } else if (msg.type == MSG_LIST_USERS_RESPONSE) {
                // Réponse avec la liste des utilisateurs
                strncpy(g_list_buffer, msg.content, sizeof(g_list_buffer) - 1);
                g_list_buffer[sizeof(g_list_buffer) - 1] = '\0';
                g_list_received = 1;
            } else if (msg.type == MSG_LIST_GROUPS_RESPONSE) {
                // Réponse avec la liste des groupes
                strncpy(g_list_buffer, msg.content, sizeof(g_list_buffer) - 1);
                g_list_buffer[sizeof(g_list_buffer) - 1] = '\0';
                g_list_received = 1;
            } else if (msg.type == MSG_CHANGE_COLOR) {
                // Changement de couleur du groupe
                const char *color_code = COLOR_GREEN;

                if (strcmp(msg.content, "red") == 0) color_code = COLOR_RED;
                else if (strcmp(msg.content, "green") == 0) color_code = COLOR_GREEN;
                else if (strcmp(msg.content, "yellow") == 0) color_code = COLOR_YELLOW;
                else if (strcmp(msg.content, "blue") == 0) color_code = COLOR_BLUE;
                else if (strcmp(msg.content, "magenta") == 0) color_code = COLOR_MAGENTA;
                else if (strcmp(msg.content, "cyan") == 0) color_code = COLOR_CYAN;
                else if (strcmp(msg.content, "white") == 0) color_code = COLOR_WHITE;

                strncpy(g_user_color, color_code, sizeof(g_user_color) - 1);
                g_user_color[sizeof(g_user_color) - 1] = '\0';

                // Afficher le message
                message_display(&msg, g_user_color);
            } else {
                // Affichage normal des autres messages
                message_display(&msg, g_user_color);
            }

            display_prompt(g_username, g_current_group, g_user_color);
        }

        usleep(10000); // 10 ms
    }

    return NULL;
}

void print_help(void) {
    printf("\n=== COMMANDES DISPONIBLES ===\n");
    printf("Attention : les commandes sont sensibles à la casse\n\n");
    printf("  /join <groupe>         - Rejoindre un groupe (créé automatiquement si inexistant)\n");
    printf("  /leave                 - Quitter le groupe actuel\n");
    printf("  /msg <user> <message>  - Envoyer un message privé\n");
    printf("  /merge <g1> <g2>       - Fusionner deux groupes (admin uniquement)\n");
    printf("  /kick <user>           - Exclure un utilisateur (admin uniquement)\n");
    printf("  /promote <user>        - Promouvoir un utilisateur admin (admin uniquement)\n");
    printf("  /demote <user>         - Rétrograder un administrateur (admin uniquement)\n");
    printf("  /users                 - Lister les utilisateurs\n");
    printf("  /groups                - Lister les groupes\n");
    printf("  /color <couleur>       - Changer la couleur du prompt (admin uniquement)\n");
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
            // Envoyer une requête au serveur
            Message msg;
            message_create(&msg, MSG_LIST_USERS, g_username, NULL, NULL, "");

            if (socket_send(g_sockfd, &msg, &g_server_addr) < 0) {
                printf("Erreur : impossible d'envoyer la requête au serveur\n");
                return 0;
            }

            // Attendre la réponse (avec timeout)
            g_list_received = 0;
            int timeout = 0;
            while (!g_list_received && timeout < 50) {  // 500ms max
                usleep(10000);  // 10ms
                timeout++;
            }

            if (!g_list_received) {
                printf("Erreur : pas de réponse du serveur\n");
                printf("Vérifiez que le serveur est démarré.\n");
                return 0;
            }

            // Afficher la liste des utilisateurs
            printf("\n=== UTILISATEURS CONNECTÉS ===\n");
            if (strlen(g_list_buffer) == 0) {
                printf("Aucun utilisateur connecté\n");
            } else {
                char *token = strtok(g_list_buffer, "|");
                int count = 0;
                while (token != NULL) {
                    char username[MAX_USERNAME];
                    char group[MAX_GROUP_NAME];
                    if (sscanf(token, "%[^:]:%s", username, group) == 2) {
                        printf("  - %s (groupe: %s)\n", username, group);
                        count++;
                    }
                    token = strtok(NULL, "|");
                }
                printf("\nTotal: %d utilisateur(s)\n", count);
            }
            printf("\n");
        }
        else if (strcmp(input, "/groups") == 0) {
            // Envoyer une requête au serveur
            Message msg;
            message_create(&msg, MSG_LIST_GROUPS, g_username, NULL, NULL, "");

            if (socket_send(g_sockfd, &msg, &g_server_addr) < 0) {
                printf("Erreur : impossible d'envoyer la requête au serveur\n");
                return 0;
            }

            // Attendre la réponse (avec timeout)
            g_list_received = 0;
            int timeout = 0;
            while (!g_list_received && timeout < 50) {  // 500ms max
                usleep(10000);  // 10ms
                timeout++;
            }

            if (!g_list_received) {
                printf("Erreur : pas de réponse du serveur\n");
                printf("Vérifiez que le serveur est démarré.\n");
                return 0;
            }

            // Afficher la liste des groupes
            printf("\n=== GROUPES ACTIFS ===\n");
            if (strlen(g_list_buffer) == 0) {
                printf("Aucun groupe actif\n");
            } else {
                char *token = strtok(g_list_buffer, "|");
                int count = 0;
                while (token != NULL) {
                    char groupname[MAX_GROUP_NAME];
                    int user_count, admin_count;
                    if (sscanf(token, "%[^:]:%d:%d", groupname, &user_count, &admin_count) == 3) {
                        printf("  - %s (%d membre(s), %d admin(s))\n", groupname, user_count, admin_count);
                        count++;
                    }
                    token = strtok(NULL, "|");
                }
                printf("\nTotal: %d groupe(s)\n", count);
            }
            printf("\n");
        }
        else if (strcmp(input, "/leave") == 0) {
            if (strlen(g_current_group) == 0) {
                printf("Vous n'êtes dans aucun groupe\n");
            } else {
                char temp_group[MAX_GROUP_NAME];
                strncpy(temp_group, g_current_group, MAX_GROUP_NAME - 1);
                temp_group[MAX_GROUP_NAME - 1] = '\0';

                Message msg;
                message_create(&msg, MSG_LEAVE, g_username, NULL, g_current_group, "");
                socket_send(g_sockfd, &msg, &g_server_addr);
                g_current_group[0] = '\0';

                // Réinitialiser la couleur à vert par défaut
                strncpy(g_user_color, COLOR_GREEN, sizeof(g_user_color) - 1);
                g_user_color[sizeof(g_user_color) - 1] = '\0';

                printf("Vous avez quitté le groupe %s\n", temp_group);
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

            if (socket_send(g_sockfd, &msg, &g_server_addr) < 0) {
                printf("Erreur : impossible d'envoyer la requête au serveur\n");
            } else {
                printf("Connexion au groupe '%s' en cours...\n", arg1);
                printf("(Si vous ne recevez pas de confirmation, vérifiez que le serveur est démarré)\n");
            }
            // Ne pas mettre à jour g_current_group ici, attendre la confirmation du serveur
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
            // Vérifier que la couleur est valide
            if (strcmp(arg1, "red") != 0 &&
                strcmp(arg1, "green") != 0 &&
                strcmp(arg1, "yellow") != 0 &&
                strcmp(arg1, "blue") != 0 &&
                strcmp(arg1, "magenta") != 0 &&
                strcmp(arg1, "cyan") != 0 &&
                strcmp(arg1, "white") != 0) {
                printf("Couleur inconnue. Choix: red, green, yellow, blue, magenta, cyan, white\n");
                return 0;
            }

            // Vérifier qu'on est dans un groupe
            if (strlen(g_current_group) == 0) {
                printf("Vous devez rejoindre un groupe pour changer sa couleur\n");
                return 0;
            }

            // Envoyer le changement de couleur au serveur
            // Le serveur propagera le changement à tous les membres du groupe
            Message msg;
            message_create(&msg, MSG_CHANGE_COLOR, g_username, NULL, g_current_group, arg1);
            socket_send(g_sockfd, &msg, &g_server_addr);
        }
        else if (sscanf(input, "/kick %s", arg1) == 1) {
            // Vérifier qu'on est dans un groupe
            if (strlen(g_current_group) == 0) {
                printf("Vous devez être dans un groupe pour exclure un utilisateur\n");
                return 0;
            }

            // Envoyer la demande d'exclusion au serveur
            Message msg;
            message_create(&msg, MSG_KICK_USER, g_username, NULL, g_current_group, arg1);
            socket_send(g_sockfd, &msg, &g_server_addr);
            printf("Demande d'exclusion de %s du groupe %s...\n", arg1, g_current_group);
        }
        else if (sscanf(input, "/promote %s", arg1) == 1) {
            // Vérifier qu'on est dans un groupe
            if (strlen(g_current_group) == 0) {
                printf("Vous devez être dans un groupe pour promouvoir un utilisateur\n");
                return 0;
            }

            // Envoyer la demande de promotion au serveur
            Message msg;
            message_create(&msg, MSG_PROMOTE_ADMIN, g_username, NULL, g_current_group, arg1);
            socket_send(g_sockfd, &msg, &g_server_addr);
            printf("Demande de promotion de %s comme administrateur de %s...\n", arg1, g_current_group);
        }
        else if (sscanf(input, "/demote %s", arg1) == 1) {
            // Vérifier qu'on est dans un groupe
            if (strlen(g_current_group) == 0) {
                printf("Vous devez être dans un groupe pour rétrograder un administrateur\n");
                return 0;
            }

            // Envoyer la demande de rétrogradation au serveur
            Message msg;
            message_create(&msg, MSG_DEMOTE_ADMIN, g_username, NULL, g_current_group, arg1);
            socket_send(g_sockfd, &msg, &g_server_addr);
            printf("Demande de rétrogradation de %s dans %s...\n", arg1, g_current_group);
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

    // Tester la connexion au serveur
    printf("Connexion au serveur...\n");
    Message connect_msg;
    message_create(&connect_msg, MSG_CONNECT, g_username, NULL, NULL, "");

    if (socket_send(g_sockfd, &connect_msg, &g_server_addr) < 0) {
        fprintf(stderr, "Erreur : impossible d'envoyer la requête de connexion\n");
        return EXIT_FAILURE;
    }

    // Attendre la confirmation avec timeout (2 secondes)
    g_connected_to_server = 0;
    int timeout = 0;
    while (!g_connected_to_server && timeout < 200) {  // 2 secondes max
        usleep(10000);  // 10ms
        timeout++;
    }

    if (!g_connected_to_server) {
        fprintf(stderr, "\n%sErreur : Serveur injoignable%s\n", COLOR_RED, COLOR_RESET);
        fprintf(stderr, "Vérifiez que le serveur est démarré à l'adresse %s:%d\n\n", server_ip, server_port);
        g_running = 0;
        close(g_sockfd);
        return EXIT_FAILURE;
    }

    printf("%sConnecté au serveur!%s Tapez /help pour l'aide.\n\n", COLOR_GREEN, COLOR_RESET);
    
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
