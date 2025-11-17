#include "messaging.h"
#include <signal.h>
#include <fcntl.h>

// Variables globales pour le nettoyage
static int g_shmid = -1;
static int g_semid = -1;
static SharedMemory *g_shm = NULL;
static int g_sockfd = -1;
static FILE *g_logfile = NULL;

void cleanup_and_exit(int signum) {
    printf("\n\nArrêt du serveur...\n");

    if (g_logfile != NULL) {
        log_event(g_logfile, "SERVER", "Arrêt du serveur");
        fclose(g_logfile);
        g_logfile = NULL;
    }

    if (g_sockfd >= 0) {
        close(g_sockfd);
    }

    if (g_shm != NULL) {
        shm_detach(g_shm);
    }

    if (g_shmid >= 0) {
        shm_destroy(g_shmid);
    }

    if (g_semid >= 0) {
        sem_destroy(g_semid);
    }

    printf("Nettoyage terminé\n");
    exit(0);
}

void handle_client_message(int sockfd, SharedMemory *shm, int semid,
                          Message *msg, struct sockaddr_in *client_addr) {
    // Log de débogage
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), ip_str, INET_ADDRSTRLEN);
    printf("[DEBUG HANDLER] Message Type: %d, De: %s (%s:%d)\n",
           msg->type, msg->sender, ip_str, ntohs(client_addr->sin_port));

    char log_buffer[512];

    // Verrouiller l'accès à la mémoire partagée
    sem_p(semid);

    switch (msg->type) {
        case MSG_JOIN: {
            // Ajouter l'utilisateur s'il n'existe pas
            User *user = user_find(shm, msg->sender);
            if (user == NULL) {
                user_add(shm, msg->sender, client_addr, ntohs(client_addr->sin_port));
            }

            // Créer le groupe s'il n'existe pas
            Group *group = group_find(shm, msg->group);
            if (group == NULL) {
                group_create(shm, msg->group);
            }

            // Ajouter l'utilisateur au groupe
            group_add_user(shm, msg->group, msg->sender);

            // Diffuser le message de join au groupe
            message_send_to_group(sockfd, shm, msg);

            printf(">>> %s a rejoint %s\n", msg->sender, msg->group);
            snprintf(log_buffer, sizeof(log_buffer), "%s a rejoint le groupe %s (%s:%d)",
                    msg->sender, msg->group, ip_str, ntohs(client_addr->sin_port));
            log_event(g_logfile, "JOIN", log_buffer);
            break;
        }
        
        case MSG_LEAVE: {
            // Retirer l'utilisateur du groupe
            group_remove_user(shm, msg->group, msg->sender);

            // Diffuser le message de leave au groupe
            message_send_to_group(sockfd, shm, msg);

            printf(">>> %s a quitté %s\n", msg->sender, msg->group);
            snprintf(log_buffer, sizeof(log_buffer), "%s a quitté le groupe %s",
                    msg->sender, msg->group);
            log_event(g_logfile, "LEAVE", log_buffer);
            break;
        }
        
        case MSG_PUBLIC: {
            // Diffuser le message public au groupe
            message_send_to_group(sockfd, shm, msg);

            printf(">>> [%s] %s: %s\n", msg->group, msg->sender, msg->content);
            snprintf(log_buffer, sizeof(log_buffer), "[%s] %s: %s",
                    msg->group, msg->sender, msg->content);
            log_event(g_logfile, "PUBLIC", log_buffer);
            break;
        }
        
        case MSG_PRIVATE: {
            // Envoyer le message privé
            if (message_send_private(sockfd, shm, msg) == 0) {
                printf(">>> [PRIVÉ] %s -> %s: %s\n",
                       msg->sender, msg->recipient, msg->content);
                snprintf(log_buffer, sizeof(log_buffer), "%s -> %s: %s",
                        msg->sender, msg->recipient, msg->content);
                log_event(g_logfile, "PRIVATE", log_buffer);
            }
            break;
        }

        case MSG_CHANGE_COLOR: {
            // Changer la couleur de l'utilisateur
            const char *color_code = COLOR_GREEN;

            if (strcmp(msg->content, "red") == 0) color_code = COLOR_RED;
            else if (strcmp(msg->content, "green") == 0) color_code = COLOR_GREEN;
            else if (strcmp(msg->content, "yellow") == 0) color_code = COLOR_YELLOW;
            else if (strcmp(msg->content, "blue") == 0) color_code = COLOR_BLUE;
            else if (strcmp(msg->content, "magenta") == 0) color_code = COLOR_MAGENTA;
            else if (strcmp(msg->content, "cyan") == 0) color_code = COLOR_CYAN;
            else if (strcmp(msg->content, "white") == 0) color_code = COLOR_WHITE;

            // Mettre à jour la couleur de l'utilisateur
            user_set_color(shm, msg->sender, color_code);

            printf(">>> %s a changé sa couleur en %s\n", msg->sender, msg->content);
            snprintf(log_buffer, sizeof(log_buffer), "%s a changé sa couleur en %s",
                    msg->sender, msg->content);
            log_event(g_logfile, "CHANGE_COLOR", log_buffer);

            // Propager le changement de couleur à tous les membres du groupe
            message_send_to_group(sockfd, shm, msg);
            break;
        }

        case MSG_CREATE_GROUP: {
            // Créer un nouveau groupe
            if (group_create(shm, msg->content) >= 0) {
                printf(">>> Groupe %s créé par %s\n", msg->content, msg->sender);
                snprintf(log_buffer, sizeof(log_buffer), "Groupe %s créé par %s",
                        msg->content, msg->sender);
                log_event(g_logfile, "CREATE_GROUP", log_buffer);

                // Envoyer confirmation à l'utilisateur
                Message response;
                message_create(&response, MSG_CREATE_GROUP, "Serveur",
                             msg->sender, NULL, msg->content);
                User *user = user_find(shm, msg->sender);
                if (user != NULL) {
                    socket_send(sockfd, &response, &user->addr);
                }
            }
            break;
        }
        
        case MSG_MERGE_GROUPS: {
            // Format du contenu: "groupe1:groupe2"
            char group1[MAX_GROUP_NAME], group2[MAX_GROUP_NAME];
            if (sscanf(msg->content, "%[^:]:%s", group1, group2) == 2) {
                if (group_merge(shm, group1, group2) == 0) {
                    printf(">>> Groupes %s et %s fusionnés\n", group1, group2);
                    snprintf(log_buffer, sizeof(log_buffer), "Groupes %s et %s fusionnés par %s",
                            group1, group2, msg->sender);
                    log_event(g_logfile, "MERGE_GROUPS", log_buffer);

                    // Notifier tous les utilisateurs concernés
                    Message notification;
                    char notif_content[MAX_MESSAGE];
                    snprintf(notif_content, MAX_MESSAGE,
                            "Les groupes %s et %s ont été fusionnés", group1, group2);
                    message_create(&notification, MSG_PUBLIC, "Serveur",
                                 NULL, group1, notif_content);
                    message_send_to_group(sockfd, shm, &notification);
                }
            }
            break;
        }
        
        case MSG_DISCONNECT: {
            user_remove(shm, msg->sender);
            printf(">>> %s s'est déconnecté\n", msg->sender);
            snprintf(log_buffer, sizeof(log_buffer), "%s s'est déconnecté", msg->sender);
            log_event(g_logfile, "DISCONNECT", log_buffer);
            break;
        }
        
        case MSG_LIST_USERS:
        case MSG_LIST_GROUPS:
            // Ces commandes sont gérées côté client
            break;
            
        default:
            printf(">>> Type de message inconnu: %d\n", msg->type);
            break;
    }
    
    // Déverrouiller l'accès à la mémoire partagée
    sem_v(semid);
}

int main(int argc, char **argv) {
    int port = PORT_BASE;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    printf("=== SERVEUR DE MESSAGERIE ===\n");
    printf("Port: %d\n\n", port);

    // Ouvrir le fichier de log
    g_logfile = fopen("server.log", "a");
    if (g_logfile == NULL) {
        perror("Erreur lors de l'ouverture du fichier de log");
        fprintf(stderr, "Le serveur continuera sans logging\n");
    } else {
        log_event(g_logfile, "SERVER", "Démarrage du serveur");
        printf("Fichier de log: server.log\n");
    }

    // Installer le gestionnaire de signal
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    
    // Créer les fichiers de clés s'ils n'existent pas
    FILE *f = fopen(SHM_KEY_FILE, "w");
    if (f != NULL) {
        fclose(f);
    }
    
    f = fopen(SEM_KEY_FILE, "w");
    if (f != NULL) {
        fclose(f);
    }
    
    // Créer la mémoire partagée
    key_t shm_key = ftok(SHM_KEY_FILE, '1');
    if (shm_key == -1) {
        perror("Erreur ftok pour SHM");
        return EXIT_FAILURE;
    }
    
    g_shmid = shm_create(shm_key, sizeof(SharedMemory));
    if (g_shmid == -1) {
        return EXIT_FAILURE;
    }
    
    if (shm_attach(g_shmid, &g_shm) == -1) {
        return EXIT_FAILURE;
    }
    
    // Initialiser la mémoire partagée
    memset(g_shm, 0, sizeof(SharedMemory));
    printf("Mémoire partagée initialisée (ID: %d)\n", g_shmid);
    
    // Créer le sémaphore
    key_t sem_key = ftok(SEM_KEY_FILE, '1');
    if (sem_key == -1) {
        perror("Erreur ftok pour SEM");
        return EXIT_FAILURE;
    }
    
    g_semid = sem_create(sem_key);
    if (g_semid == -1) {
        return EXIT_FAILURE;
    }
    
    if (sem_init(g_semid, 1) == -1) {
        return EXIT_FAILURE;
    }
    printf("Sémaphore initialisé (ID: %d)\n", g_semid);
    
    // Créer et configurer le socket
    g_sockfd = socket_create_udp();
    if (g_sockfd == -1) {
        return EXIT_FAILURE;
    }
    
    if (socket_bind_udp(g_sockfd, port) == -1) {
        return EXIT_FAILURE;
    }
    
    // Configurer le socket en mode non-bloquant
    int flags = fcntl(g_sockfd, F_GETFL, 0);
    fcntl(g_sockfd, F_SETFL, flags | O_NONBLOCK);

    printf("\nServeur en écoute...\n\n");

    char log_buffer[256];
    snprintf(log_buffer, sizeof(log_buffer), "Serveur en écoute sur le port %d", port);
    log_event(g_logfile, "SERVER", log_buffer);

    // Boucle principale
    Message msg;
    struct sockaddr_in client_addr;
    
    while (1) {
        ssize_t n = socket_receive(g_sockfd, &msg, &client_addr);
        
        if (n > 0) {
            handle_client_message(g_sockfd, g_shm, g_semid, &msg, &client_addr);
        }
        
        // Petite pause pour éviter une utilisation CPU excessive
        usleep(10000); // 10 ms
    }
    
    cleanup_and_exit(0);
    return 0;
}
