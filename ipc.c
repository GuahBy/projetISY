#include "messaging.h"

// ========== Gestion de la mémoire partagée ==========

int shm_create(key_t key, size_t size) {
    int shmid = shmget(key, size, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Erreur shmget");
        return -1;
    }
    return shmid;
}

int shm_attach(int shmid, SharedMemory **shm) {
    *shm = (SharedMemory *)shmat(shmid, NULL, 0);
    if ((int)(*shm) == -1) {
        perror("Erreur shmat");
        return -1;
    }
    return 0;
}

int shm_detach(SharedMemory *shm) {
    if (shmdt(shm) == -1) {
        perror("Erreur shmdt");
        return -1;
    }
    return 0;
}

int shm_destroy(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("Erreur shmctl IPC_RMID");
        return -1;
    }
    return 0;
}

// ========== Gestion des sémaphores ==========

int sem_create(key_t key) {
    int semid = semget(key, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("Erreur semget");
        return -1;
    }
    return semid;
}

int sem_init(int semid, int value) {
    union semun arg;
    arg.val = value;
    
    if (semctl(semid, 0, SETVAL, arg) == -1) {
        perror("Erreur semctl SETVAL");
        return -1;
    }
    return 0;
}

int sem_p(int semid) {
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = -1;  // Décrémente (P operation)
    op.sem_flg = 0;
    
    if (semop(semid, &op, 1) == -1) {
        perror("Erreur semop P");
        return -1;
    }
    return 0;
}

int sem_v(int semid) {
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = 1;   // Incrémente (V operation)
    op.sem_flg = 0;
    
    if (semop(semid, &op, 1) == -1) {
        perror("Erreur semop V");
        return -1;
    }
    return 0;
}

int sem_destroy(int semid) {
    if (semctl(semid, 0, IPC_RMID, NULL) == -1) {
        perror("Erreur semctl IPC_RMID");
        return -1;
    }
    return 0;
}
