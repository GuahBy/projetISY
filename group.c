#include "messaging.h"

// ========== Gestion des groupes ==========

int group_create(SharedMemory *shm, const char *group_name, const char *creator) {
    // Vérifier si le groupe existe déjà
    for (int i = 0; i < shm->group_count; i++) {
        if (strcmp(shm->groups[i].name, group_name) == 0) {
            if (shm->groups[i].active) {
                fprintf(stderr, "Groupe %s existe déjà\n", group_name);
                return -1;
            } else {
                // Réactiver le groupe
                shm->groups[i].active = 1;
                shm->groups[i].user_count = 0;
                shm->groups[i].admin_count = 0;
                strncpy(shm->groups[i].color, COLOR_GREEN, 15);
                shm->groups[i].color[15] = '\0';
                // Ajouter le créateur comme premier administrateur
                if (creator != NULL && strlen(creator) > 0) {
                    strncpy(shm->groups[i].admins[0], creator, MAX_USERNAME - 1);
                    shm->groups[i].admins[0][MAX_USERNAME - 1] = '\0';
                    shm->groups[i].admin_count = 1;
                }
                return i;
            }
        }
    }

    // Créer un nouveau groupe
    if (shm->group_count >= MAX_GROUPS) {
        fprintf(stderr, "Nombre maximum de groupes atteint\n");
        return -1;
    }

    int idx = shm->group_count;
    strncpy(shm->groups[idx].name, group_name, MAX_GROUP_NAME - 1);
    shm->groups[idx].name[MAX_GROUP_NAME - 1] = '\0';
    shm->groups[idx].user_count = 0;
    shm->groups[idx].admin_count = 0;
    shm->groups[idx].active = 1;
    strncpy(shm->groups[idx].color, COLOR_GREEN, 15);
    shm->groups[idx].color[15] = '\0';

    // Ajouter le créateur comme premier administrateur
    if (creator != NULL && strlen(creator) > 0) {
        strncpy(shm->groups[idx].admins[0], creator, MAX_USERNAME - 1);
        shm->groups[idx].admins[0][MAX_USERNAME - 1] = '\0';
        shm->groups[idx].admin_count = 1;
        printf("Groupe %s créé avec %s comme administrateur (index %d)\n", group_name, creator, idx);
    } else {
        printf("Groupe %s créé (index %d)\n", group_name, idx);
    }

    shm->group_count++;
    return idx;
}

int group_add_user(SharedMemory *shm, const char *group_name, const char *username) {
    Group *group = group_find(shm, group_name);
    if (group == NULL) {
        fprintf(stderr, "Groupe %s non trouvé\n", group_name);
        return -1;
    }
    
    User *user = user_find(shm, username);
    if (user == NULL) {
        fprintf(stderr, "Utilisateur %s non trouvé\n", username);
        return -1;
    }
    
    // Vérifier si l'utilisateur est déjà dans le groupe
    for (int i = 0; i < group->user_count; i++) {
        if (strcmp(group->users[i], username) == 0) {
            fprintf(stderr, "Utilisateur %s déjà dans le groupe %s\n", username, group_name);
            return -1;
        }
    }
    
    // Ajouter l'utilisateur au groupe
    if (group->user_count >= MAX_CLIENTS) {
        fprintf(stderr, "Groupe %s plein\n", group_name);
        return -1;
    }
    
    strncpy(group->users[group->user_count], username, MAX_USERNAME - 1);
    group->users[group->user_count][MAX_USERNAME - 1] = '\0';
    group->user_count++;
    
    // Mettre à jour le groupe actuel de l'utilisateur
    strncpy(user->current_group, group_name, MAX_GROUP_NAME - 1);
    user->current_group[MAX_GROUP_NAME - 1] = '\0';
    
    printf("Utilisateur %s ajouté au groupe %s\n", username, group_name);
    return 0;
}

int group_remove_user(SharedMemory *shm, const char *group_name, const char *username) {
    Group *group = group_find(shm, group_name);
    if (group == NULL) {
        return -1;
    }
    
    // Chercher et retirer l'utilisateur
    for (int i = 0; i < group->user_count; i++) {
        if (strcmp(group->users[i], username) == 0) {
            // Décaler les utilisateurs suivants
            for (int j = i; j < group->user_count - 1; j++) {
                strcpy(group->users[j], group->users[j + 1]);
            }
            group->user_count--;
            
            // Effacer le groupe actuel de l'utilisateur
            User *user = user_find(shm, username);
            if (user != NULL) {
                user->current_group[0] = '\0';
            }
            
            printf("Utilisateur %s retiré du groupe %s\n", username, group_name);
            return 0;
        }
    }
    
    return -1;
}

Group* group_find(SharedMemory *shm, const char *group_name) {
    for (int i = 0; i < shm->group_count; i++) {
        if (shm->groups[i].active && strcmp(shm->groups[i].name, group_name) == 0) {
            return &shm->groups[i];
        }
    }
    return NULL;
}

int group_merge(SharedMemory *shm, const char *group1_name, const char *group2_name) {
    Group *group1 = group_find(shm, group1_name);
    Group *group2 = group_find(shm, group2_name);

    if (group1 == NULL) {
        fprintf(stderr, "Groupe %s non trouvé\n", group1_name);
        return -1;
    }

    if (group2 == NULL) {
        fprintf(stderr, "Groupe %s non trouvé\n", group2_name);
        return -1;
    }

    if (strcmp(group1_name, group2_name) == 0) {
        fprintf(stderr, "Impossible de fusionner un groupe avec lui-même\n");
        return -1;
    }

    // Transférer tous les utilisateurs de group2 vers group1
    for (int i = 0; i < group2->user_count; i++) {
        char username[MAX_USERNAME];
        strncpy(username, group2->users[i], MAX_USERNAME - 1);
        username[MAX_USERNAME - 1] = '\0';

        // Vérifier si l'utilisateur n'est pas déjà dans group1
        int already_in_group1 = 0;
        for (int j = 0; j < group1->user_count; j++) {
            if (strcmp(group1->users[j], username) == 0) {
                already_in_group1 = 1;
                break;
            }
        }

        if (!already_in_group1) {
            if (group1->user_count < MAX_CLIENTS) {
                strncpy(group1->users[group1->user_count], username, MAX_USERNAME - 1);
                group1->users[group1->user_count][MAX_USERNAME - 1] = '\0';
                group1->user_count++;

                // Mettre à jour le groupe actuel de l'utilisateur
                User *user = user_find(shm, username);
                if (user != NULL) {
                    strncpy(user->current_group, group1_name, MAX_GROUP_NAME - 1);
                    user->current_group[MAX_GROUP_NAME - 1] = '\0';
                }
            }
        }
    }

    // Transférer les administrateurs de group2 vers group1
    for (int i = 0; i < group2->admin_count; i++) {
        char admin_name[MAX_USERNAME];
        strncpy(admin_name, group2->admins[i], MAX_USERNAME - 1);
        admin_name[MAX_USERNAME - 1] = '\0';

        // Vérifier si l'admin n'est pas déjà dans la liste des admins de group1
        int already_admin = 0;
        for (int j = 0; j < group1->admin_count; j++) {
            if (strcmp(group1->admins[j], admin_name) == 0) {
                already_admin = 1;
                break;
            }
        }

        if (!already_admin && group1->admin_count < MAX_CLIENTS) {
            strncpy(group1->admins[group1->admin_count], admin_name, MAX_USERNAME - 1);
            group1->admins[group1->admin_count][MAX_USERNAME - 1] = '\0';
            group1->admin_count++;
        }
    }

    // Désactiver group2
    group2->active = 0;
    group2->user_count = 0;
    group2->admin_count = 0;

    printf("Groupes %s et %s fusionnés dans %s\n", group1_name, group2_name, group1_name);
    return 0;
}

// Vérifier si un utilisateur est administrateur d'un groupe
int group_is_admin(Group *group, const char *username) {
    if (group == NULL || username == NULL) {
        return 0;
    }

    for (int i = 0; i < group->admin_count; i++) {
        if (strcmp(group->admins[i], username) == 0) {
            return 1;
        }
    }
    return 0;
}

// Ajouter un utilisateur comme administrateur
int group_add_admin(Group *group, const char *username) {
    if (group == NULL || username == NULL) {
        return -1;
    }

    // Vérifier si l'utilisateur est déjà administrateur
    if (group_is_admin(group, username)) {
        fprintf(stderr, "Utilisateur %s est déjà administrateur\n", username);
        return -1;
    }

    // Vérifier si l'utilisateur est dans le groupe
    int user_in_group = 0;
    for (int i = 0; i < group->user_count; i++) {
        if (strcmp(group->users[i], username) == 0) {
            user_in_group = 1;
            break;
        }
    }

    if (!user_in_group) {
        fprintf(stderr, "Utilisateur %s n'est pas membre du groupe\n", username);
        return -1;
    }

    // Ajouter l'utilisateur comme administrateur
    if (group->admin_count >= MAX_CLIENTS) {
        fprintf(stderr, "Nombre maximum d'administrateurs atteint\n");
        return -1;
    }

    strncpy(group->admins[group->admin_count], username, MAX_USERNAME - 1);
    group->admins[group->admin_count][MAX_USERNAME - 1] = '\0';
    group->admin_count++;

    printf("Utilisateur %s promu administrateur du groupe %s\n", username, group->name);
    return 0;
}

// Retirer les droits d'administrateur à un utilisateur
int group_remove_admin(Group *group, const char *username) {
    if (group == NULL || username == NULL) {
        return -1;
    }

    // Vérifier si l'utilisateur est administrateur
    int admin_index = -1;
    for (int i = 0; i < group->admin_count; i++) {
        if (strcmp(group->admins[i], username) == 0) {
            admin_index = i;
            break;
        }
    }

    if (admin_index == -1) {
        fprintf(stderr, "Utilisateur %s n'est pas administrateur\n", username);
        return -1;
    }

    // Empêcher de rétrograder le dernier administrateur
    if (group->admin_count <= 1) {
        fprintf(stderr, "Impossible de rétrograder le dernier administrateur du groupe\n");
        return -1;
    }

    // Retirer l'utilisateur de la liste des admins
    for (int i = admin_index; i < group->admin_count - 1; i++) {
        strcpy(group->admins[i], group->admins[i + 1]);
    }
    group->admin_count--;

    printf("Utilisateur %s n'est plus administrateur du groupe %s\n", username, group->name);
    return 0;
}

// Exclure un utilisateur d'un groupe (avec gestion des admins)
int group_kick_user(SharedMemory *shm, const char *group_name, const char *username) {
    Group *group = group_find(shm, group_name);
    if (group == NULL) {
        return -1;
    }

    // Retirer l'utilisateur de la liste des admins s'il en fait partie
    for (int i = 0; i < group->admin_count; i++) {
        if (strcmp(group->admins[i], username) == 0) {
            // Décaler les admins suivants
            for (int j = i; j < group->admin_count - 1; j++) {
                strcpy(group->admins[j], group->admins[j + 1]);
            }
            group->admin_count--;
            break;
        }
    }

    // Retirer l'utilisateur du groupe
    return group_remove_user(shm, group_name, username);
}
