#include "messaging.h"

// ========== Fonctions utilitaires ==========

void display_prompt(const char *username, const char *group, const char *color) {
    if (group != NULL && strlen(group) > 0) {
        printf("%s[%s@%s]> %s", color, username, group, COLOR_RESET);
    } else {
        printf("%s[%s]> %s", color, username, COLOR_RESET);
    }
    fflush(stdout);
}

void display_users(SharedMemory *shm) {
    printf("\n=== UTILISATEURS CONNECTÉS ===\n");
    int count = 0;
    
    for (int i = 0; i < shm->user_count; i++) {
        if (shm->users[i].active) {
            printf("  %s%s%s", 
                   shm->users[i].color, 
                   shm->users[i].username, 
                   COLOR_RESET);
            
            if (strlen(shm->users[i].current_group) > 0) {
                printf(" (groupe: %s)", shm->users[i].current_group);
            }
            printf("\n");
            count++;
        }
    }
    
    printf("Total: %d utilisateur(s)\n\n", count);
}

void display_groups(SharedMemory *shm) {
    printf("\n=== GROUPES DISPONIBLES ===\n");
    int count = 0;
    
    for (int i = 0; i < shm->group_count; i++) {
        if (shm->groups[i].active) {
            printf("  %s (%d membre(s))\n", 
                   shm->groups[i].name, 
                   shm->groups[i].user_count);
            
            if (shm->groups[i].user_count > 0) {
                printf("    Membres: ");
                for (int j = 0; j < shm->groups[i].user_count; j++) {
                    printf("%s", shm->groups[i].users[j]);
                    if (j < shm->groups[i].user_count - 1) {
                        printf(", ");
                    }
                }
                printf("\n");
            }
            count++;
        }
    }
    
    printf("Total: %d groupe(s)\n\n", count);
}

const char* get_timestamp_str(time_t timestamp) {
    static char buffer[64];
    struct tm *tm_info = localtime(&timestamp);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

void clear_screen(void) {
    printf("\033[2J\033[1;1H");
}

void log_event(FILE *logfile, const char *event_type, const char *details) {
    if (logfile == NULL) {
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(logfile, "[%s] [%s] %s\n", timestamp, event_type, details);
    fflush(logfile);
}
