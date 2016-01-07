#ifndef PLAYER
#define PLAYER

#include "../map/Map.h"
#define MAX_PER_CASE 64
#define INV_SIZE 4

typedef struct invCase invCase;
struct invCase {
	block desc;
	int number;
};

typedef struct Player Player;
struct Player {
	char *name;
	int* position;
	invCase inventory[INV_SIZE];
	bool falling;
};

Player* createPlayer(char *playerName);
bool addBlockToInv(Player *player, block block);
bool removeBlockFromInv(Player *player, int index);
int caseNotFull(Player player, block block);
int *inInventory(Player player, block block);

#endif
