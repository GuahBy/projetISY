# 📝 Note Technique : Fondamentaux et Problématiques du Projet

## 🎯 Vue d'ensemble du projet

Ce projet implémente un **système de messagerie client-serveur distribué** en langage C, utilisant les mécanismes IPC (Inter-Process Communication) POSIX. Il permet à plusieurs utilisateurs de communiquer en temps réel via des groupes et des messages privés.

### Objectifs pédagogiques
- Maîtriser la **communication inter-processus** (mémoire partagée, sémaphores)
- Comprendre la **programmation réseau** (sockets UDP)
- Gérer la **concurrence** et la **synchronisation**
- Implémenter une architecture **client-serveur distribuée**

---

## 🏗️ Architecture Fondamentale

### 1. Modèle Client-Serveur

```
┌─────────────────────────────────────────────────┐
│              SERVEUR CENTRAL                     │
│  • Gère la mémoire partagée                     │
│  • Route les messages entre clients             │
│  • Maintient l'état global (users, groupes)    │
│  • Synchronise les accès avec sémaphores       │
└─────────────────────────────────────────────────┘
                      ↕ UDP
┌─────────────────────────────────────────────────┐
│              CLIENTS (N instances)               │
│  • Interface utilisateur                        │
│  • Thread principal + thread réception         │
│  • Accès lecture à la mémoire partagée         │
└─────────────────────────────────────────────────┘
```

### 2. Composants Clés

#### a) Mémoire Partagée (Shared Memory)
- **Rôle** : Zone mémoire commune accessible par le serveur et les clients
- **Contenu** :
  - Liste des 50 utilisateurs maximum
  - Liste des 10 groupes maximum
  - Métadonnées (compteurs, états)
- **Avantage** : Accès ultra-rapide aux données sans passer par le réseau
- **Challenge** : Nécessite une synchronisation stricte

#### b) Sémaphores
- **Rôle** : Mécanisme de verrouillage pour protéger la mémoire partagée
- **Opérations** :
  - `sem_p()` : Verrouiller (P = "Proberen" = essayer en néerlandais)
  - `sem_v()` : Déverrouiller (V = "Verhogen" = incrémenter)
- **Pattern** : Section critique protégée
  ```c
  sem_p(semid);           // LOCK
  // Accès à la mémoire partagée
  sem_v(semid);           // UNLOCK
  ```

#### c) Communication UDP
- **Choix UDP vs TCP** :
  - ✅ UDP : Rapide, sans connexion, adapté pour messagerie temps réel
  - ❌ TCP : Plus lourd, avec handshake, garantit la livraison
- **Compromis** : Pas de garantie de livraison mais performance optimale
- **Port** : 8000 par défaut

#### d) Multi-threading
- **Thread principal** : Lecture des commandes utilisateur
- **Thread réception** : Écoute continue des messages (boucle asynchrone)
- **Synchronisation** : Variables globales protégées par signaux

---

## ⚠️ Problèmes Techniques Rencontrés et Solutions

### 1. 🔒 **Race Conditions et Exclusion Mutuelle**

#### Problème
Lorsque plusieurs clients modifient simultanément la mémoire partagée (ex: rejoindre un groupe), des **conditions de course** peuvent survenir :
```
Client A : Lit user_count = 5
Client B : Lit user_count = 5  (en même temps)
Client A : Écrit user_count = 6
Client B : Écrit user_count = 6  (écrase la valeur de A!)
Résultat : Un utilisateur est perdu !
```

#### Solution
- Utilisation d'un **sémaphore binaire** pour garantir l'exclusion mutuelle
- Toute opération sur la mémoire partagée est encadrée par `sem_p()` / `sem_v()`
- Pattern systématique :
  ```c
  sem_p(semid);                    // VERROUILLER
  // Section critique : modifier la SHM
  Group *group = group_find(shm, "test");
  group_add_user(shm, "test", "alice");
  sem_v(semid);                    // DÉVERROUILLER
  ```

#### Risque résiduel
- **Deadlock** : Si un processus plante entre `sem_p()` et `sem_v()`, le sémaphore reste verrouillé
- **Solution** : Gestion propre des signaux (`SIGINT`, `SIGTERM`) avec `cleanup_and_exit()`

---

### 2. 🌐 **Problèmes de Communication Réseau**

#### Problème A : VirtualBox et NAT
Dans un environnement VirtualBox avec mode réseau **NAT**, les clients ne peuvent pas atteindre le serveur :
- Le NAT bloque les connexions UDP entrantes
- Les paquets sont perdus silencieusement
- Le client envoie mais le serveur ne reçoit jamais

#### Solution
1. **Changer le mode réseau** : Utiliser "Bridged Adapter" au lieu de NAT
2. **Tester en local d'abord** : Vérifier avec `localhost` avant de tester en réseau
3. **Vérifier le pare-feu** : S'assurer que le port UDP 8000 est ouvert
4. **Logs de débogage** : Ajouter des logs `[DEBUG SEND]` et `[DEBUG RECV]`

#### Problème B : Paquets UDP Perdus
UDP ne garantit pas la livraison des paquets :
- Un message peut être perdu sur un réseau saturé
- Pas d'accusé de réception automatique

#### Solution
- **Messages critiques** : Envoyer une confirmation explicite (ex: `MSG_JOIN`)
- **Système de timeout** : Réessayer si pas de réponse après X secondes (non implémenté actuellement)
- **Logs côté serveur** : Tracer tous les messages reçus pour détecter les pertes

---

### 3. 🧵 **Gestion du Multi-threading**

#### Problème : Synchronisation Thread Principal ↔ Thread Réception

Le thread de réception peut afficher des messages **au milieu** de la saisie utilisateur :
```
Alice@general> Bonjour t[Bob]: Salut !
out le monde
```

#### Solution
- **Prompt re-affiché** : Après chaque message reçu, le prompt est réaffiché
- **Flush du buffer** : `fflush(stdout)` pour forcer l'affichage
- **Alternative avancée** : Utiliser `readline` ou `ncurses` (non implémenté)

#### Problème : Arrêt Propre des Threads

Quand l'utilisateur fait `Ctrl+C`, le thread de réception doit s'arrêter proprement :
```c
static int g_running = 1;  // Variable globale

void cleanup_and_exit(int signum) {
    g_running = 0;  // Signal au thread de s'arrêter
    // ... nettoyage ...
    exit(0);
}

void* receive_thread(void *arg) {
    while (g_running) {  // Boucle contrôlée
        // ... réception ...
    }
    return NULL;
}
```

#### Risque
- **Variables globales partagées** : `g_running` est modifiée par le thread principal et lue par le thread de réception
- **Solution partielle** : Utiliser `volatile` ou des mutex (non critique ici car simple flag booléen)

---

### 4. 💾 **Gestion de la Mémoire Partagée**

#### Problème A : Taille Limitée
La mémoire partagée a des limites fixes :
- **50 utilisateurs max** (`MAX_CLIENTS`)
- **10 groupes max** (`MAX_GROUPS`)

Si dépassé :
```c
if (shm->user_count >= MAX_CLIENTS) {
    return -1;  // Refuser l'ajout
}
```

#### Solution
- **Vérification avant ajout** : Toujours vérifier les limites
- **Message d'erreur explicite** : Informer l'utilisateur
- **Alternative** : Utiliser une allocation dynamique (complexe avec SHM)

#### Problème B : Fuite de Mémoire Partagée

Si le serveur crash sans nettoyage, la mémoire partagée reste allouée :
```bash
$ ipcs -m
------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status
0x12345678 32768      user       666        20480      0
```

#### Solution
- **Signal handlers** : Capturer `SIGINT`, `SIGTERM` pour nettoyer
- **Nettoyage manuel** : `ipcrm -m <shmid>` si nécessaire
- **Flags `IPC_CREAT | IPC_EXCL`** : Détection des segments orphelins

```c
void cleanup_and_exit(int signum) {
    shm_detach(g_shm);
    shm_destroy(g_shmid);
    sem_destroy(g_semid);
    exit(0);
}

signal(SIGINT, cleanup_and_exit);
signal(SIGTERM, cleanup_and_exit);
```

---

### 5. 🎨 **Problèmes d'Interface et UX**

#### Problème A : Couleurs ANSI Non Supportées

Sur certains terminaux (Windows CMD), les codes ANSI ne fonctionnent pas :
```
\x1b[31mRouge\x1b[0m  →  Affiche le code brut au lieu de colorer
```

#### Solution
- **Détection du terminal** : Vérifier `$TERM` ou `isatty()`
- **Fallback** : Désactiver les couleurs si non supporté
- **Recommandation** : Utiliser un terminal moderne (Linux/macOS, Windows Terminal)

#### Problème B : Buffer de Saisie

Le buffer d'entrée standard peut causer des artefacts :
- Messages affichés pendant la saisie
- Buffer non vidé après une erreur

#### Solution
- **`fflush(stdout)`** : Forcer l'affichage immédiat
- **`setbuf(stdin, NULL)`** : Désactiver le buffering (optionnel)

---

### 6. 🔐 **Sécurité et Vulnérabilités**

#### Problème A : Pas d'Authentification

N'importe qui peut usurper un nom d'utilisateur :
```bash
./client alice localhost  # Première instance
./client alice localhost  # Deuxième instance avec le même nom !
```

#### Solution Possible
- **Vérification d'unicité** : Refuser si le nom existe déjà (partiellement implémenté)
- **Mot de passe** : Ajouter un hash de mot de passe (hors scope du projet)
- **Token de session** : Générer un ID unique par connexion

#### Problème B : Injection de Commandes

Un utilisateur malveillant peut envoyer des messages malformés :
```c
Message msg;
strcpy(msg.content, "A" * 1000);  // Buffer overflow potentiel
```

#### Solution
- **Validation des entrées** : Toujours limiter la taille avec `strncpy()`
- **Sanitization** : Vérifier les caractères spéciaux
- **Exemple dans le code** :
  ```c
  strncpy(msg.content, input, MAX_MESSAGE - 1);
  msg.content[MAX_MESSAGE - 1] = '\0';  // Null terminator
  ```

#### Problème C : Données Sensibles en Clair

Les messages circulent en **clair** sur le réseau (UDP non chiffré) :
- Écoute réseau facile avec `tcpdump` ou `Wireshark`

#### Solution (Avancée)
- **Chiffrement** : TLS/DTLS pour UDP (complexe)
- **Alternative** : Utiliser un VPN ou réseau sécurisé

---

### 7. 📊 **Problèmes de Scalabilité**

#### Problème A : Limite de 50 Clients

Avec la mémoire partagée fixe, impossible de dépasser 50 utilisateurs.

#### Solution
- **Augmenter les constantes** : Changer `MAX_CLIENTS` (mais augmente la taille SHM)
- **Architecture distribuée** : Plusieurs serveurs avec load balancing (hors scope)

#### Problème B : Broadcast Inefficace

Quand un message est envoyé à un groupe de 50 personnes, le serveur fait 50 `sendto()` :
```c
for (int i = 0; i < group->user_count; i++) {
    socket_send(sockfd, msg, &user->addr);  // 50 appels réseau !
}
```

#### Solution
- **Multicast UDP** : Utiliser `IP_ADD_MEMBERSHIP` pour envoyer à plusieurs clients en un seul paquet
- **Compromis** : Plus complexe à configurer

---

### 8. 🐛 **Bugs Classiques et Débogage**

#### Problème A : "Address Already in Use"

Le serveur ne démarre pas car le port est déjà utilisé :
```bash
bind: Address already in use
```

#### Solution
```bash
# Trouver le processus utilisant le port
lsof -i :8000
# Tuer le processus
kill -9 <PID>
# Ou utiliser SO_REUSEADDR
int opt = 1;
setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

#### Problème B : Segmentation Fault

Souvent causé par :
- **Déréférencement de NULL** : `user->username` quand `user == NULL`
- **Buffer overflow** : Écrire au-delà d'un tableau

#### Débogage
```bash
# Compiler avec symboles de débogage
gcc -g -o server server.c

# Lancer avec gdb
gdb ./server
(gdb) run
(gdb) bt  # Backtrace après crash
```

#### Problème C : Messages Manquants

Le client envoie mais le serveur ne reçoit pas :

#### Checklist de débogage
1. ✅ Serveur démarré avant client ?
2. ✅ Bonne IP/port ?
3. ✅ Pare-feu ouvert ?
4. ✅ Mode réseau VirtualBox correct ?
5. ✅ Logs `[DEBUG SEND]` / `[DEBUG RECV]` ?

---

## 🎓 Concepts Clés à Retenir

### 1. IPC (Inter-Process Communication)

| Mécanisme | Usage | Avantages | Inconvénients |
|-----------|-------|-----------|---------------|
| **Mémoire partagée** | Partage de données | Très rapide | Nécessite synchronisation |
| **Sémaphores** | Synchronisation | Robuste | Risque de deadlock |
| **Sockets UDP** | Communication réseau | Rapide, sans état | Pas de garantie |
| **Sockets TCP** | Communication fiable | Garantit livraison | Plus lent, avec état |

### 2. Patterns de Synchronisation

```c
// ❌ MAUVAIS : Race condition
user_count++;

// ✅ BON : Section critique protégée
sem_p(semid);
user_count++;
sem_v(semid);
```

### 3. Architecture Événementielle

```
Thread Principal (Événements utilisateur)
    ↓
Envoyer message → Socket UDP
                      ↓
                  Serveur reçoit
                      ↓
                  Routage + SHM
                      ↓
Thread Réception ← Socket UDP
    ↓
Afficher à l'utilisateur
```

---

## 🚀 Évolutions Possibles

### Améliorations Fonctionnelles
1. **Persistance** : Sauvegarder l'historique des messages (fichier ou base de données)
2. **Fichiers** : Transférer des fichiers entre utilisateurs
3. **Voix** : Ajouter de l'audio via RTP/UDP
4. **Notifications** : Son ou vibration à la réception

### Améliorations Techniques
1. **Chiffrement** : DTLS pour sécuriser UDP
2. **Accusés de réception** : Garantir la livraison des messages critiques
3. **Heartbeat** : Détecter les clients déconnectés
4. **Load balancing** : Répartir la charge sur plusieurs serveurs

### Améliorations UX
1. **Interface graphique** : GTK ou Qt au lieu de CLI
2. **Émojis** : Support UTF-8 et émojis
3. **Markdown** : Formatage des messages (gras, italique)
4. **Historique** : Flèches haut/bas pour parcourir les commandes

---

## 📚 Références et Ressources

### Documentation Système
- `man 2 shmget` : Création de mémoire partagée
- `man 2 semget` : Création de sémaphores
- `man 2 socket` : Création de sockets
- `man 7 udp` : Protocole UDP

### Concepts Clés
- **Race condition** : Conflit d'accès concurrent
- **Deadlock** : Blocage mutuel entre processus
- **Exclusion mutuelle** : Un seul processus dans la section critique
- **Atomicité** : Opération indivisible

### Outils de Débogage
- `gdb` : Débogueur C
- `valgrind` : Détection de fuites mémoire
- `strace` : Tracer les appels système
- `netstat` : État des sockets
- `tcpdump` : Capture de paquets réseau

---

## ✅ Conclusion

Ce projet illustre les **défis réels** de la programmation système :
- **Concurrence** : Gérer plusieurs processus simultanés
- **Synchronisation** : Protéger les ressources partagées
- **Réseau** : Gérer l'incertitude de la communication
- **Robustesse** : Anticiper les erreurs et les cas limites

Les problèmes rencontrés (race conditions, paquets perdus, deadlocks) sont **typiques** des systèmes distribués et préparent à comprendre des technologies modernes (Docker, Kubernetes, microservices).

**Apprentissage clé** : La programmation système nécessite une **pensée défensive** et une **attention aux détails** que les langages de haut niveau masquent souvent.
