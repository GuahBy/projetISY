#ifndef MESSAGING_H
#define MESSAGING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

// Constantes
#define MAX_USERNAME 32
#define MAX_MESSAGE 256
#define MAX_GROUP_NAME 32
#define MAX_CLIENTS 50
#define MAX_GROUPS 10
#define PORT_BASE 8000
#define SHM_KEY_FILE "shm_key.txt"
#define SEM_KEY_FILE "sem_key.txt"

// Codes couleur ANSI
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_WHITE   "\x1b[37m"

// Types de messages
typedef enum {
    MSG_PUBLIC,        // Message public dans un groupe
    MSG_PRIVATE,       // Message privé entre utilisateurs
    MSG_JOIN,          // Rejoindre un groupe
    MSG_LEAVE,         // Quitter un groupe
    MSG_LIST_USERS,    // Lister les utilisateurs
    MSG_LIST_GROUPS,   // Lister les groupes
    MSG_CREATE_GROUP,  // Créer un groupe
    MSG_MERGE_GROUPS,  // Fusionner deux groupes
    MSG_CHANGE_COLOR,  // Changer la couleur du prompt
    MSG_DISCONNECT,    // Déconnexion
    MSG_KICK_USER,     // Exclure un utilisateur (admin uniquement)
    MSG_PROMOTE_ADMIN, // Promouvoir un utilisateur en administrateur (admin uniquement)
    MSG_DEMOTE_ADMIN,  // Rétrograder un administrateur (admin uniquement)
    MSG_LIST_USERS_RESPONSE,  // Réponse du serveur avec la liste des utilisateurs
    MSG_LIST_GROUPS_RESPONSE, // Réponse du serveur avec la liste des groupes
    MSG_CONNECT,       // Test de connexion au serveur
    MSG_CONNECT_ACK    // Accusé de réception de connexion
} MessageType;

// Structure pour un message
typedef struct {
    MessageType type;
    char sender[MAX_USERNAME];
    char recipient[MAX_USERNAME];  // Pour les messages privés
    char group[MAX_GROUP_NAME];
    char content[MAX_MESSAGE];
    time_t timestamp;
} Message;

// Structure pour un utilisateur
typedef struct {
    char username[MAX_USERNAME];
    char current_group[MAX_GROUP_NAME];
    struct sockaddr_in addr;
    int port;
    int active;
    char color[16];  // Code couleur pour le prompt
    time_t last_activity;
} User;

// Structure pour un groupe
typedef struct {
    char name[MAX_GROUP_NAME];
    int user_count;
    char users[MAX_CLIENTS][MAX_USERNAME];
    int admin_count;                         // Nombre d'administrateurs
    char admins[MAX_CLIENTS][MAX_USERNAME];  // Liste des administrateurs
    int active;
    char color[16];  // Couleur du groupe (partagée par tous les membres)
} Group;

// Structure de la mémoire partagée
typedef struct {
    User users[MAX_CLIENTS];
    Group groups[MAX_GROUPS];
    int user_count;
    int group_count;
} SharedMemory;

// Union pour semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Prototypes des fonctions - Gestion IPC
int shm_create(key_t key, size_t size);
int shm_attach(int shmid, SharedMemory **shm);
int shm_detach(SharedMemory *shm);
int shm_destroy(int shmid);

int sem_create(key_t key);
int sem_init(int semid, int value);
int sem_p(int semid);  // Verrouiller
int sem_v(int semid);  // Déverrouiller
int sem_destroy(int semid);

// Prototypes des fonctions - Gestion réseau
int socket_create_udp(void);
int socket_bind_udp(int sockfd, int port);
ssize_t socket_send(int sockfd, Message *msg, struct sockaddr_in *dest_addr);
ssize_t socket_receive(int sockfd, Message *msg, struct sockaddr_in *src_addr);

// Prototypes des fonctions - Gestion utilisateurs
int user_add(SharedMemory *shm, const char *username, struct sockaddr_in *addr, int port);
int user_remove(SharedMemory *shm, const char *username);
User* user_find(SharedMemory *shm, const char *username);
int user_set_color(SharedMemory *shm, const char *username, const char *color);

// Prototypes des fonctions - Gestion groupes
int group_create(SharedMemory *shm, const char *group_name, const char *creator);
int group_add_user(SharedMemory *shm, const char *group_name, const char *username);
int group_remove_user(SharedMemory *shm, const char *group_name, const char *username);
Group* group_find(SharedMemory *shm, const char *group_name);
int group_merge(SharedMemory *shm, const char *group1, const char *group2);
int group_is_admin(Group *group, const char *username);
int group_add_admin(Group *group, const char *username);
int group_remove_admin(Group *group, const char *username);
int group_kick_user(SharedMemory *shm, const char *group_name, const char *username);

// Prototypes des fonctions - Gestion messages
void message_create(Message *msg, MessageType type, const char *sender, 
                   const char *recipient, const char *group, const char *content);
void message_display(const Message *msg, const char *color);
int message_send_to_group(int sockfd, SharedMemory *shm, Message *msg);
int message_send_private(int sockfd, SharedMemory *shm, Message *msg);

// Prototypes des fonctions - Utilitaires
void display_prompt(const char *username, const char *group, const char *color);
void display_users(SharedMemory *shm);
void display_groups(SharedMemory *shm);
const char* get_timestamp_str(time_t timestamp);
void clear_screen(void);
void log_event(FILE *logfile, const char *event_type, const char *details);

#endif // MESSAGING_H
