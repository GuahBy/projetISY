#include "messaging.h"

// ========== Gestion des utilisateurs ==========

int user_add(SharedMemory *shm, const char *username, struct sockaddr_in *addr, int port) {
    // Vérifier si l'utilisateur existe déjà
    for (int i = 0; i < shm->user_count; i++) {
        if (strcmp(shm->users[i].username, username) == 0) {
            if (shm->users[i].active) {
                fprintf(stderr, "Utilisateur %s existe déjà\n", username);
                return -1;
            } else {
                // Réactiver l'utilisateur
                shm->users[i].active = 1;
                shm->users[i].addr = *addr;
                shm->users[i].port = port;
                shm->users[i].last_activity = time(NULL);
                strcpy(shm->users[i].color, COLOR_GREEN);
                return i;
            }
        }
    }
    
    // Ajouter un nouvel utilisateur
    if (shm->user_count >= MAX_CLIENTS) {
        fprintf(stderr, "Nombre maximum d'utilisateurs atteint\n");
        return -1;
    }
    
    int idx = shm->user_count;
    strncpy(shm->users[idx].username, username, MAX_USERNAME - 1);
    shm->users[idx].username[MAX_USERNAME - 1] = '\0';
    shm->users[idx].addr = *addr;
    shm->users[idx].port = port;
    shm->users[idx].active = 1;
    shm->users[idx].current_group[0] = '\0';
    strcpy(shm->users[idx].color, COLOR_GREEN);
    shm->users[idx].last_activity = time(NULL);
    shm->user_count++;
    
    printf("Utilisateur %s ajouté (index %d)\n", username, idx);
    return idx;
}

int user_remove(SharedMemory *shm, const char *username) {
    for (int i = 0; i < shm->user_count; i++) {
        if (strcmp(shm->users[i].username, username) == 0) {
            // Retirer l'utilisateur de tous les groupes
            for (int j = 0; j < shm->group_count; j++) {
                group_remove_user(shm, shm->groups[j].name, username);
            }
            
            shm->users[i].active = 0;
            printf("Utilisateur %s désactivé\n", username);
            return 0;
        }
    }
    
    fprintf(stderr, "Utilisateur %s non trouvé\n", username);
    return -1;
}

User* user_find(SharedMemory *shm, const char *username) {
    for (int i = 0; i < shm->user_count; i++) {
        if (shm->users[i].active && strcmp(shm->users[i].username, username) == 0) {
            return &shm->users[i];
        }
    }
    return NULL;
}

int user_set_color(SharedMemory *shm, const char *username, const char *color) {
    User *user = user_find(shm, username);
    if (user == NULL) {
        fprintf(stderr, "Utilisateur %s non trouvé\n", username);
        return -1;
    }
    
    strncpy(user->color, color, sizeof(user->color) - 1);
    user->color[sizeof(user->color) - 1] = '\0';
    
    printf("Couleur de %s changée\n", username);
    return 0;
}
