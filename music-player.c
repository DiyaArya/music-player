#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ao/ao.h>
#include <sndfile.h>

#define MAX_TITLE_LENGTH 100
#define MAX_FILENAME_LENGTH 100
#define MAX_PLAYLIST_NAME_LENGTH 100
#define MAX_LINE_LENGTH 256

typedef struct {
    char title[MAX_TITLE_LENGTH];
    char artist[MAX_TITLE_LENGTH];
    char duration[MAX_TITLE_LENGTH];
    char filename[MAX_FILENAME_LENGTH];
} Song;

typedef struct Node {
    Song song;
    struct Node* next;
    struct Node* prev;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int numSongs;
    Node* currentSong;
    int isPlaying;
    char playlistName[MAX_PLAYLIST_NAME_LENGTH];
    ao_device *device;
} MusicPlayer;

Node* createNode(Song song) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        printf("Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    newNode->song = song;
    newNode->next = newNode->prev = NULL;
    return newNode;
}

void addSong(MusicPlayer* player, Song song) {
    Node* newNode = createNode(song);
    if (player->head == NULL) {
        player->head = player->tail = newNode;
    } else {
        newNode->prev = player->tail;
        player->tail->next = newNode;
        player->tail = newNode;
    }
    player->numSongs++;
}

void displayPlaylist(const MusicPlayer* player) {
    printf("Current Playlist (%s):\n", player->playlistName);
    Node* current = player->head;
    int index = 1;
    while (current != NULL) {
        printf("%d. %s by %s (%s)\n", index, current->song.title, current->song.artist, current->song.duration);
        current = current->next;
        index++;
    }
}

void playSong(const char* filename, MusicPlayer* player) {
    printf("Attempting to play file: %s\n", filename); // Debug statement

    SF_INFO sfinfo;
    SNDFILE *sndfile = sf_open(filename, SFM_READ, &sfinfo);
    if (sndfile == NULL) {
        printf("Failed to open the file: %s\n", filename);
        return;
    }

    int default_driver = ao_default_driver_id();
    ao_sample_format format;
    memset(&format, 0, sizeof(format));
    format.bits = 16;
    format.channels = sfinfo.channels;
    format.rate = sfinfo.samplerate;
    format.byte_format = AO_FMT_NATIVE;

    player->device = ao_open_live(default_driver, &format, NULL);
    if (player->device == NULL) {
        printf("Error opening device.\n");
        sf_close(sndfile);
        return;
    }

    printf("Playing: %s\n", filename);

    int buf_size = sfinfo.frames * sfinfo.channels;
    short *buffer = malloc(buf_size * sizeof(short));
    if (buffer == NULL) {
        printf("Memory allocation failed.\n");
        ao_close(player->device);
        sf_close(sndfile);
        return;
    }

    sf_read_short(sndfile, buffer, buf_size);
    ao_play(player->device, (char *)buffer, buf_size * sizeof(short));

    free(buffer);
    ao_close(player->device);
    sf_close(sndfile);
}

void stopSong(MusicPlayer* player) {
    if (player->device != NULL) {
        ao_close(player->device);
        player->device = NULL;
    }
    printf("Song stopped.\n");
}

// Function to create a deep copy of the playlist for shuffling
Node* copyPlaylist(Node* head) {
    if (head == NULL) return NULL;
    Node* current = head;
    Node* newHead = NULL, *newTail = NULL;
    while (current != NULL) {
        Node* newNode = createNode(current->song);
        if (newHead == NULL) {
            newHead = newNode;
        } else {
            newTail->next = newNode;
            newNode->prev = newTail;
        }
        newTail = newNode;
        current = current->next;
    }
    return newHead;
}

// Function to free the copied playlist
void freeCopiedPlaylist(Node* head) {
    Node* current = head;
    while (current != NULL) {
        Node* temp = current;
        current = current->next;
        free(temp);
    }
}

// Function to shuffle and display the copied playlist
void displayShuffledPlaylist(MusicPlayer* player) {
    Node* copiedHead = copyPlaylist(player->head);
    Node* current = copiedHead;
    int numSongs = player->numSongs;
    for (int i = 0; current != NULL && i < numSongs - 1; i++, current = current->next) {
        int r = i + rand() % (numSongs - i);
        Node* swapNode = copiedHead;
        for (int j = 0; j < r; j++) {
            swapNode = swapNode->next;
        }
        Song temp = current->song;
        current->song = swapNode->song;
        swapNode->song = temp;
    }
    printf("Shuffled Playlist (%s):\n", player->playlistName);
    current = copiedHead;
    int index = 1;
    while (current != NULL) {
        printf("%d. %s by %s (%s)\n", index++, current->song.title, current->song.artist, current->song.duration);
        current = current->next;
    }
    freeCopiedPlaylist(copiedHead);
}

int main() {
    srand((unsigned int)time(NULL)); // Seed the random number generator

    MusicPlayer player = {0}; // Initialize the player
    player.head = player.tail = player.currentSong = NULL;
    player.numSongs = 0;
    player.isPlaying = 0;
    player.device = NULL;

    ao_initialize(); // Initialize audio output library

    char input[MAX_LINE_LENGTH];

    // Get the playlist name from the user
    printf("Enter the name of the playlist: ");
    fgets(player.playlistName, MAX_PLAYLIST_NAME_LENGTH, stdin);
    player.playlistName[strcspn(player.playlistName, "\n")] = '\0';

    // Construct the filename from the playlist name
    char filename[MAX_FILENAME_LENGTH];
    snprintf(filename, MAX_FILENAME_LENGTH, "%s.csv", player.playlistName);
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file: %s\n", filename);
        return 1;
    }

    // Read and add songs from the CSV file
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        char title[MAX_TITLE_LENGTH], artist[MAX_TITLE_LENGTH], duration[MAX_TITLE_LENGTH], filename[MAX_FILENAME_LENGTH];
        if (sscanf(line, "%99[^,],%99[^,],%99[^,],%99[^\n]", title, artist, duration, filename) == 4) {
            Song song;
            strncpy(song.title, title, MAX_TITLE_LENGTH - 1);
            song.title[MAX_TITLE_LENGTH - 1] = '\0';
            strncpy(song.artist, artist, MAX_TITLE_LENGTH - 1);
            song.artist[MAX_TITLE_LENGTH - 1] = '\0';
            strncpy(song.duration, duration, MAX_TITLE_LENGTH - 1);
            song.duration[MAX_TITLE_LENGTH - 1] = '\0';
            strncpy(song.filename, filename, MAX_FILENAME_LENGTH - 1);
            song.filename[MAX_FILENAME_LENGTH - 1] = '\0';
            addSong(&player, song);
        } else {
            printf("Error reading line from file: %s", line);
        }
    }
    fclose(file);

    // Menu for player control
    int choice;
    while (1) {
        printf("\nMenu:\n");
        printf("1. Display Playlist\n");
        printf("2. Play Song\n");
        printf("3. Stop Song\n");
        printf("4. Next Song\n");
        printf("5. Previous Song\n");
        printf("7. Display Shuffled Playlist\n");
        printf("6. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        while (getchar() != '\n'); // Clear the input buffer

        switch (choice) {
            case 1:
                displayPlaylist(&player);
                break;
            case 2:
                if (player.isPlaying) {
                    stopSong(&player);
                    player.isPlaying = 0;
                } else {
                    printf("Enter song number to play: ");
                    if (fgets(input, sizeof(input), stdin) == NULL) {
                        printf("Error reading input.\n");
                        continue;
                    }
                    int songIndex = atoi(input);
                    if (songIndex > 0 && songIndex <= player.numSongs) {
                        Node* current = player.head;
                        for (int i = 1; i < songIndex; i++) {
                            current = current->next;
                        }
                        player.currentSong = current;
                        playSong(player.currentSong->song.filename, &player);
                        player.isPlaying = 1;
                    } else {
                        printf("Invalid song number. Please enter a valid number between 1 and %d.\n", player.numSongs);
                    }
                }
                break;
            case 3:
                if (player.isPlaying) {
                    stopSong(&player);
                    player.isPlaying = 0;
                } else {
                    printf("No song is currently playing.\n");
                }
                break;
            case 4:
                // Code to move to the next song
                break;
            case 5:
                // Code to move to the previous song
                break;
            case 7:
                displayShuffledPlaylist(&player);
                break;
            case 6:
                printf("Exiting the music player.\n");
                // Clean up the dynamically allocated memory
                Node* current = player.head;
                while (current != NULL) {
                    Node* temp = current;
                    current = current->next;
                    free(temp);
                }
                ao_shutdown();
                exit(0);
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    return 0;
}
