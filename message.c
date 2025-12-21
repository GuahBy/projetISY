#include "messaging.h"

// ========== Gestion des messages ==========

void message_create(Message *msg, MessageType type, const char *sender,
                   const char *recipient, const char *group, const char *content) {
    msg->type = type;
    
    strncpy(msg->sender, sender, MAX_USERNAME - 1);
    msg->sender[MAX_USERNAME - 1] = '\0';
    
    if (recipient != NULL) {
        strncpy(msg->recipient, recipient, MAX_USERNAME - 1);
        msg->recipient[MAX_USERNAME - 1] = '\0';
    } else {
        msg->recipient[0] = '\0';
    }
    
    if (group != NULL) {
        strncpy(msg->group, group, MAX_GROUP_NAME - 1);
        msg->group[MAX_GROUP_NAME - 1] = '\0';
    } else {
        msg->group[0] = '\0';
    }
    
    strncpy(msg->content, content, MAX_MESSAGE - 1);
    msg->content[MAX_MESSAGE - 1] = '\0';
    
    msg->timestamp = time(NULL);
}

void message_display(const Message *msg, const char *color) {
    char time_str[64];
    struct tm *tm_info = localtime(&msg->timestamp);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    switch (msg->type) {
        case MSG_PUBLIC:
            printf("%s[%s] [%s] %s: %s%s\n",
                   color, time_str, msg->group, msg->sender, msg->content, COLOR_RESET);
            break;
            
        case MSG_PRIVATE:
            printf("%s[%s] [PRIVÉ de %s]: %s%s\n",
                   COLOR_MAGENTA, time_str, msg->sender, msg->content, COLOR_RESET);
            break;
            
        case MSG_JOIN:
            printf("%s[%s] %s a rejoint le groupe %s%s\n",
                   COLOR_CYAN, time_str, msg->sender, msg->group, COLOR_RESET);
            break;
            
        case MSG_LEAVE:
            if (strlen(msg->content) > 0) {
                // Message spécial (kick, etc.)
                printf("%s[%s] %s%s\n",
                       COLOR_YELLOW, time_str, msg->content, COLOR_RESET);
            } else {
                printf("%s[%s] %s a quitté le groupe %s%s\n",
                       COLOR_YELLOW, time_str, msg->sender, msg->group, COLOR_RESET);
            }
            break;
            
        case MSG_DISCONNECT:
            printf("%s[%s] %s s'est déconnecté%s\n",
                   COLOR_RED, time_str, msg->sender, COLOR_RESET);
            break;

        case MSG_CHANGE_COLOR:
            printf("%s[%s] %s a changé sa couleur en %s%s\n",
                   COLOR_CYAN, time_str, msg->sender, msg->content, COLOR_RESET);
            break;

        default:
            printf("[%s] Message de %s: %s\n", time_str, msg->sender, msg->content);
            break;
    }
}

int message_send_to_group(int sockfd, SharedMemory *shm, Message *msg) {
    Group *group = group_find(shm, msg->group);
    if (group == NULL) {
        fprintf(stderr, "Groupe %s non trouvé\n", msg->group);
        return -1;
    }
    
    int sent_count = 0;
    
    // Envoyer le message à tous les utilisateurs du groupe sauf l'envoyeur
    for (int i = 0; i < group->user_count; i++) {
        if (strcmp(group->users[i], msg->sender) != 0) {
            User *user = user_find(shm, group->users[i]);
            if (user != NULL && user->active) {
                if (socket_send(sockfd, msg, &user->addr) >= 0) {
                    sent_count++;
                }
            }
        }
    }
    
    printf("Message envoyé à %d utilisateur(s) du groupe %s\n", sent_count, msg->group);
    return sent_count;
}

int message_send_private(int sockfd, SharedMemory *shm, Message *msg) {
    User *recipient = user_find(shm, msg->recipient);
    if (recipient == NULL) {
        fprintf(stderr, "Utilisateur %s non trouvé\n", msg->recipient);
        return -1;
    }
    
    if (!recipient->active) {
        fprintf(stderr, "Utilisateur %s non connecté\n", msg->recipient);
        return -1;
    }
    
    if (socket_send(sockfd, msg, &recipient->addr) < 0) {
        fprintf(stderr, "Erreur d'envoi du message privé à %s\n", msg->recipient);
        return -1;
    }
    
    printf("Message privé envoyé à %s\n", msg->recipient);
    return 0;
}
