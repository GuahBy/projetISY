# 📨 Système de Messagerie Distribuée

Projet de système de messagerie client-serveur en C avec gestion de groupes, messages publics/privés et mémoire partagée.

## 📋 Table des matières

- [Description](#-description)
- [Architecture](#-architecture)
- [Compilation](#-compilation)
- [Utilisation](#-utilisation)
  - [Démarrer le serveur](#démarrer-le-serveur)
  - [Démarrer un client](#démarrer-un-client)
- [Commandes du Client](#-commandes-du-client)
- [Types de Messages](#-types-de-messages)
- [Structures de Données](#-structures-de-données)
- [Fonctionnalités Avancées](#-fonctionnalités-avancées)

---

## 🎯 Description

Ce projet implémente un système de messagerie en temps réel permettant à plusieurs clients de communiquer via un serveur central. Le système supporte :

- **Messages publics** dans des groupes
- **Messages privés** entre utilisateurs
- **Gestion de groupes** (création, fusion, rejoindre, quitter)
- **Mémoire partagée** pour la synchronisation des données
- **Communication UDP** pour les échanges réseau
- **Multi-threading** pour la réception asynchrone des messages
- **Interface colorée** personnalisable

---

## 🏗️ Architecture

### Composants principaux

```
┌─────────────────────────────────────────────────┐
│                    SERVEUR                      │
│  • Port UDP : 8000                              │
│  • Mémoire partagée (50 users, 10 groupes)      │
│  • Sémaphore pour synchronisation               │
│  • Routage des messages                         │
└─────────────────────────────────────────────────┘
                        ↕ UDP
┌─────────────────────────────────────────────────┐
│                   CLIENTS                       │
│  • Thread principal : input utilisateur         │
│  • Thread réception : messages asynchrones      │
│  • Accès lecture à la mémoire partagée          │
└─────────────────────────────────────────────────┘
```

### Fichiers du projet

| Fichier | Description |
|---------|-------------|
| `messaging.h` | En-têtes et structures de données |
| `server.c` | Implémentation du serveur |
| `client.c` | Implémentation du client |
| `user.c` | Gestion des utilisateurs |
| `group.c` | Gestion des groupes |
| `message.c` | Gestion des messages |
| `ipc.c` | Communication inter-processus (SHM, sémaphores) |
| `network.c` | Gestion des sockets UDP |
| `utils.c` | Fonctions utilitaires (affichage, timestamp) |
| `Makefile` | Configuration de compilation |

---

## 🔨 Compilation

### Prérequis

- **GCC** (ou autre compilateur C99)
- **Make**
- **POSIX threads** (pthread)
- **Système Linux/Unix**

### Compilation

```bash
make
```

Cela compile :
- `server` : exécutable du serveur
- `client` : exécutable du client

### Nettoyage

```bash
make clean    # Supprime les fichiers .o
make fclean   # Supprime les exécutables et .o
make re       # Recompile tout
```

---

## 🚀 Utilisation

### Démarrer le serveur

```bash
./server [port]
```

**Arguments :**
- `port` (optionnel) : Port d'écoute UDP (par défaut : 8000)

**Exemple :**
```bash
./server           # Démarre sur le port 8000
./server 9000      # Démarre sur le port 9000
```

Le serveur affiche :
- Les messages reçus et traités
- Les utilisateurs qui se connectent/déconnectent
- Les créations de groupes
- Les erreurs éventuelles

**Arrêt du serveur :**
- Appuyez sur `Ctrl+C` pour arrêter proprement le serveur
- Les ressources (mémoire partagée, sémaphores) sont automatiquement libérées

---

### Démarrer un client

```bash
./client <username> <server_ip> [server_port]
```

**Arguments :**
- `username` (obligatoire) : Nom d'utilisateur (max 32 caractères)
- `server_ip` (obligatoire) : Adresse IP du serveur
- `server_port` (optionnel) : Port du serveur (par défaut : 8000)

**Exemples :**
```bash
./client alice localhost          # Alice se connecte au serveur local
./client bob 192.168.1.100        # Bob se connecte à un serveur distant
./client charlie 127.0.0.1 9000   # Charlie sur port personnalisé
```

---

## 💬 Commandes du Client

Une fois connecté, vous pouvez utiliser les commandes suivantes :

### 📋 Liste complète des commandes

| Commande | Syntaxe | Description |
|----------|---------|-------------|
| **Rejoindre un groupe** | `/join <groupe>` | Rejoindre un groupe existant ou en créer un nouveau. Si vous êtes déjà dans un groupe, vous le quittez automatiquement. |
| **Quitter un groupe** | `/leave` | Quitter le groupe actuel |
| **Message privé** | `/msg <user> <message>` | Envoyer un message privé à un utilisateur spécifique |
| **Créer un groupe** | `/create <groupe>` | Créer un nouveau groupe (ne vous y fait pas rejoindre) |
| **Fusionner des groupes** | `/merge <g1> <g2>` | Fusionner le groupe `g2` dans le groupe `g1` (tous les membres de g2 rejoignent g1) - **Admin uniquement** |
| **Exclure un utilisateur** | `/kick <user>` | Exclure un utilisateur du groupe actuel - **Admin uniquement** |
| **Promouvoir administrateur** | `/promote <user>` | Promouvoir un membre en administrateur du groupe - **Admin uniquement** |
| **Rétrograder administrateur** | `/demote <user>` | Rétrograder un administrateur en simple membre - **Admin uniquement** |
| **Lister les utilisateurs** | `/users` | Afficher tous les utilisateurs connectés avec leur groupe actuel |
| **Lister les groupes** | `/groups` | Afficher tous les groupes actifs avec le nombre de membres et d'admins |
| **Changer de couleur** | `/color <couleur>` | Changer la couleur du groupe (affecte tous les membres) - **Admin uniquement**. Couleurs disponibles : `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white` |
| **Effacer l'écran** | `/clear` | Effacer l'écran du terminal |
| **Aide** | `/help` | Afficher la liste des commandes disponibles |
| **Quitter** | `/quit` | Se déconnecter et quitter l'application |
| **Message public** | `<texte>` | Tout texte ne commençant pas par `/` est envoyé comme message public au groupe actuel |

---

### 📖 Exemples d'utilisation

#### 1. Rejoindre un groupe et envoyer des messages

```bash
# Alice rejoint le groupe "general"
/join general

# Alice envoie un message public au groupe
Bonjour tout le monde !

# Alice envoie un message privé à Bob
/msg bob Salut Bob, comment vas-tu ?
```

#### 2. Créer et gérer des groupes

```bash
# Bob crée et rejoint le groupe qu'il vient de créer
/join projet

# Charlie fusionne deux groupes (en tant qu'admin de 'general')
/merge general projet
```

#### 3. Gestion des administrateurs

```bash
# Alice (admin du groupe) promeut Bob en administrateur
/promote bob

# Alice vérifie la liste des groupes et leurs admins
/groups

# Alice rétrograde Charlie (qui était admin)
/demote charlie

# Alice exclut un membre problématique du groupe
/kick david
```

#### 4. Lister et personnaliser

```bash
# Voir tous les utilisateurs connectés
/users

# Voir tous les groupes actifs
/groups

# Changer la couleur du prompt en bleu
/color blue

# Effacer l'écran
/clear
```

---

## 📡 Types de Messages

Le système gère 17 types de messages différents (définis dans `messaging.h`) :

| Type | Valeur | Traité par | Description |
|------|--------|------------|-------------|
| `MSG_PUBLIC` | 0 | Serveur | Message public envoyé à tous les membres d'un groupe |
| `MSG_PRIVATE` | 1 | Serveur | Message privé entre deux utilisateurs |
| `MSG_JOIN` | 2 | Serveur | Demande de rejoindre un groupe |
| `MSG_LEAVE` | 3 | Serveur | Demande de quitter un groupe |
| `MSG_LIST_USERS` | 4 | Serveur | Demande la liste des utilisateurs |
| `MSG_LIST_GROUPS` | 5 | Serveur | Demande la liste des groupes |
| `MSG_CREATE_GROUP` | 6 | Serveur | Créer un nouveau groupe |
| `MSG_MERGE_GROUPS` | 7 | Serveur | Fusionner deux groupes (admin uniquement) |
| `MSG_CHANGE_COLOR` | 8 | Serveur | Changer la couleur du groupe (admin uniquement) |
| `MSG_DISCONNECT` | 9 | Serveur | Notification de déconnexion |
| `MSG_KICK_USER` | 10 | Serveur | Exclure un utilisateur d'un groupe (admin uniquement) |
| `MSG_PROMOTE_ADMIN` | 11 | Serveur | Promouvoir un utilisateur en administrateur (admin uniquement) |
| `MSG_DEMOTE_ADMIN` | 12 | Serveur | Rétrograder un administrateur (admin uniquement) |
| `MSG_LIST_USERS_RESPONSE` | 13 | Client | Réponse du serveur avec la liste des utilisateurs |
| `MSG_LIST_GROUPS_RESPONSE` | 14 | Client | Réponse du serveur avec la liste des groupes |
| `MSG_CONNECT` | 15 | Serveur | Test de connexion au serveur |
| `MSG_CONNECT_ACK` | 16 | Client | Accusé de réception de connexion |

### Flux de traitement

```
Client → Commande → Message UDP → Serveur → Mémoire Partagée → Broadcast/Routage → Clients
```

---

## 📊 Structures de Données

### Limites du système

```c
#define MAX_CLIENTS 50         // Nombre max d'utilisateurs simultanés
#define MAX_GROUPS 10          // Nombre max de groupes simultanés
#define MAX_USERNAME 32        // Longueur max du nom d'utilisateur
#define MAX_GROUP_NAME 32      // Longueur max du nom de groupe
#define MAX_MESSAGE 256        // Longueur max d'un message
#define PORT_BASE 8000         // Port par défaut du serveur
```

### Structure d'un message

```c
typedef struct {
    MessageType type;                  // Type de message (enum)
    char sender[MAX_USERNAME];         // Nom de l'expéditeur
    char recipient[MAX_USERNAME];      // Destinataire (pour MSG_PRIVATE)
    char group[MAX_GROUP_NAME];        // Groupe concerné
    char content[MAX_MESSAGE];         // Contenu du message
    time_t timestamp;                  // Horodatage
} Message;
```

### Structure d'un utilisateur

```c
typedef struct {
    char username[MAX_USERNAME];       // Nom d'utilisateur unique
    char current_group[MAX_GROUP_NAME]; // Groupe actuel
    struct sockaddr_in addr;           // Adresse réseau
    int port;                          // Port du client
    int active;                        // 1 = actif, 0 = inactif
    char color[16];                    // Code couleur ANSI
    time_t last_activity;              // Dernière activité
} User;
```

### Structure d'un groupe

```c
typedef struct {
    char name[MAX_GROUP_NAME];         // Nom du groupe
    int user_count;                    // Nombre de membres
    char users[MAX_CLIENTS][MAX_USERNAME]; // Liste des membres
    int active;                        // 1 = actif, 0 = inactif
} Group;
```

### Mémoire partagée

```c
typedef struct {
    User users[MAX_CLIENTS];           // Tableau de tous les utilisateurs
    Group groups[MAX_GROUPS];          // Tableau de tous les groupes
    int user_count;                    // Nombre d'utilisateurs actifs
    int group_count;                   // Nombre de groupes actifs
} SharedMemory;
```

---

## ⚡ Fonctionnalités Avancées

### 🔒 Synchronisation

- **Sémaphore binaire** : Protège l'accès concurrent à la mémoire partagée
- **Mémoire partagée** : Permet un accès rapide aux données des utilisateurs et groupes
- **Opérations atomiques** : `sem_p()` (lock) et `sem_v()` (unlock)

### 🎨 Couleurs ANSI

Le système supporte 7 couleurs pour personnaliser le prompt :

```
red     → \x1b[31m
green   → \x1b[32m
yellow  → \x1b[33m
blue    → \x1b[34m
magenta → \x1b[35m
cyan    → \x1b[36m
white   → \x1b[37m
```

### 🧵 Multi-threading

Chaque client utilise 2 threads :

1. **Thread principal** : Lit les commandes de l'utilisateur et les envoie au serveur
2. **Thread de réception** : Écoute les messages entrants en continu (non-bloquant)

### 🌐 Communication UDP

- **Non-bloquant** : Les sockets sont configurées en mode non-bloquant
- **Sans connexion** : UDP permet une communication rapide sans handshake
- **Broadcast** : Le serveur peut envoyer un message à plusieurs destinataires

### 🔑 Gestion des clés IPC

Le système utilise `ftok()` pour générer des clés IPC à partir de fichiers :

- `shm_key.txt` : Clé pour la mémoire partagée
- `sem_key.txt` : Clé pour le sémaphore

Ces fichiers sont créés automatiquement au premier lancement.

---

## 🛠️ Dépannage

### Le serveur ne démarre pas

- **Erreur "Address already in use"** : Le port est déjà utilisé
  - Solution : Utilisez un autre port ou arrêtez le processus utilisant le port 8000
  ```bash
  lsof -i :8000      # Voir qui utilise le port
  kill -9 <PID>      # Tuer le processus
  ```

- **Erreur "Permission denied"** : Vous n'avez pas les droits pour créer la mémoire partagée
  - Solution : Vérifiez les permissions ou utilisez `sudo`

### Le client ne se connecte pas

- **Erreur "Connection refused"** : Le serveur n'est pas démarré
  - Solution : Démarrez le serveur d'abord

- **Erreur "Cannot resolve hostname"** : L'adresse IP est invalide
  - Solution : Vérifiez l'adresse IP du serveur

### Les messages ne s'affichent pas

- Vérifiez que vous êtes bien dans un groupe (`/join <groupe>`)
- Vérifiez que le destinataire est connecté (`/users`)
- Vérifiez les logs du serveur pour voir les erreurs

### Problèmes de communication réseau (VirtualBox)

Si vous utilisez VirtualBox et que le serveur ne reçoit pas les messages du client :

**Symptômes :**
- Le client affiche "Demande de création du groupe..." mais rien ne se passe côté serveur
- Le client affiche "Tentative de connexion au groupe..." mais ne rejoint pas
- Aucun log ne s'affiche côté serveur

**Logs de débogage :**

Le système inclut maintenant des logs détaillés pour diagnostiquer les problèmes :
- `[DEBUG SEND]` : Affiche chaque envoi de message (type, expéditeur, destination)
- `[DEBUG RECV]` : Affiche chaque réception de message (taille, source, type)
- `[DEBUG HANDLER]` : Affiche chaque message traité par le serveur

**Étapes de diagnostic :**

1. **Vérifier la configuration réseau VirtualBox**
   ```bash
   # Sur la machine serveur, vérifier l'IP
   ip addr show

   # Sur la machine client, tester la connectivité
   ping 192.168.x.x
   ```

2. **Vérifier que le serveur écoute bien**
   ```bash
   # Sur la machine serveur, après avoir démarré le serveur
   netstat -uln | grep 8000
   # Devrait afficher: 0.0.0.0:8000
   ```

3. **Tester avec un client local d'abord**
   ```bash
   # Sur la machine serveur
   ./server 8000

   # Dans un autre terminal sur la même machine
   ./client testuser localhost 8000
   /create test
   ```

   Si cela fonctionne, le problème vient de la configuration réseau VirtualBox.

4. **Vérifier le pare-feu**
   ```bash
   # Désactiver temporairement le pare-feu pour tester
   sudo ufw disable
   # Ou autoriser le port UDP 8000
   sudo ufw allow 8000/udp
   ```

5. **Configurer VirtualBox en mode "Bridged Adapter"**
   - Dans les paramètres de la VM, onglet "Réseau"
   - Changez "NAT" en "Accès par pont" (Bridged Adapter)
   - Redémarrez la VM et notez la nouvelle IP

**Solutions possibles :**

- **Mode réseau VirtualBox :**
  - Utilisez "Bridged Adapter" pour que la VM soit sur le même réseau que l'hôte
  - Évitez "NAT" qui peut bloquer les connexions UDP entrantes

- **Firewall :**
  - Assurez-vous que le port UDP 8000 est ouvert sur les deux machines

- **Tester en local d'abord :**
  - Lancez serveur et client sur la même machine avec `localhost`
  - Si cela fonctionne, le problème est réseau, pas le code

---

## 📚 Documentation supplémentaire

Le projet inclut également :

- **Diagrammes UML** (dans `/UML/`)
  - Architecture système
  - Diagrammes de classes
  - Diagrammes de séquence
  - Diagrammes de cas d'utilisation

- **Énoncé du projet** (dans `/Enonce/`)
  - Spécifications complètes (PDF)
  - Cas d'utilisation illustrés

---

## 👥 Auteurs

Projet réalisé dans le cadre du cours ISY (Inter-process communication SYstems).

---

## 📝 Licence

Ce projet est à usage éducatif uniquement.
