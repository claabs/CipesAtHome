#include <stdio.h>
#include <stdlib.h>
#include "calculator.h"
#include "FTPManagement.h"
#include "recipes.h"

#define NUM_RECIPES 58 // Including Dried Bouquet trade

// Total frames to choose an additional ingredient (as opposed to just a single ingredient)
// This does not include the additional frames needed to navigate to the items that you want to use
#define CHOOSE_2ND_INGREDIENT_FRAMES 56

// Total frames to toss any item (as opposed to the item being automatically placed in the inventory when a NULL space exists
// This does not include the additional frames needed to navigate to the item that you want to toss
#define TOSS_FRAMES 32

// Frames needed to sort the inventory by each method
#define ALPHA_SORT_FRAMES 38
#define REVERSE_ALPHA_SORT_FRAMES 40
#define TYPE_SORT_FRAMES 39
#define REVERSE_TYPE_SORT_FRAMES 41

// If the player does not toss the final output item, 5 extra frames are needed to obtain Jump Storage
#define JUMP_STORAGE_NO_TOSS_FRAMES 5

// Finished roadmaps can potentially have some legal moves rearranged to faster points in time
// Give the search space some buffer frames so that if a roadmap is discovered that is "close" to the frame record,
// perform optimal shuffling of the moves to find the best possible rearranged roadmap and evaluate for new records
#define BUFFER_SEARCH_FRAMES 200

// TODO: This should exactly replicate the formatting from the Python output file, though I haven't tested it yet
int printResults(char *filename, struct BranchPath *path) {
	FILE *fp = fopen(filename, "w");
	
	// Write header information
	fputs("Description\tFrames Taken\tTotal Frames", fp);
	struct Recipe *recipes = getRecipeList();
	for (int i = 0; i < 20; i++) {
		fprintf(fp, "\tSlot #%d", i+1);
	}
	for (int i = 0; i < NUM_RECIPES; i++) {
		fprintf(fp, "\t%s", getItemName(recipes[i].output.a_key));
	}
	fprintf(fp, "\n");
	
	// Print data information
	struct BranchPath *curNode = path;
	do {
		struct MoveDescription *desc = curNode->description;
		enum Action curNodeAction = desc->action;
		if (curNodeAction == Cook) {
			struct Cook *cookData = desc->data;
			fprintf(fp, "Use [%s] in slot %d ", getItemName(cookData->item1.a_key), cookData->itemIndex1 + 1);
			
			if (cookData->numItems == 2)
				fprintf(fp, "and [%s] in slot %d ", getItemName(cookData->item2.a_key), cookData->itemIndex2 + 1);
			
			fprintf(fp, "to make ");
			
			if (cookData->handleOutput == Toss)
				fprintf(fp, "(and toss) ");
			else if (cookData->handleOutput == Autoplace)
				fprintf(fp, "(and auto-place) ");
			
			fprintf(fp, "<%s>", getItemName(cookData->output.a_key));
			
			if (cookData->handleOutput == TossOther)
				fprintf(fp, ", toss [%s] in slot %d", getItemName(cookData->toss.a_key), cookData->indexToss + 1);
			fprintf(fp, "\t");
		}
		else if (curNodeAction == Ch5) {
			struct CH5 *ch5Data = desc->data;
			fprintf(fp, "Ch.5 Break: Replace #%d for DB, Replace #%d for CO, Sort (", ch5Data->indexDriedBouquet, ch5Data->indexCoconut);
			
			switch (ch5Data->ch5Sort) {
				case Sort_Alpha_Asc :
					fprintf(fp, "Alpha), ");
					break;
				case Sort_Alpha_Des :
					fprintf(fp, "Reverse-Alpha), ");
					break;
				case Sort_Type_Asc :
					fprintf(fp, "Type), ");
					break;
				case Sort_Type_Des :
					fprintf(fp, "Reverse-Type), ");
					break;
				default :
					fprintf(fp, "ERROR IN CH5SORT SWITCH CASE");
			};
			
			fprintf(fp, "Replace #%d for KM, Replace #%d for CS, Use TR in #%d %d %d\t", ch5Data->indexKeelMango, ch5Data->indexCourageShell, ch5Data->indexThunderRage, desc->framesTaken, desc->totalFramesTaken);
		}
		else {
			// Some type of sorting
			fprintf(fp, "Sort - ");
			switch (curNodeAction) {
				case Sort_Alpha_Asc :
					fprintf(fp, "Alphabetical ");
					break;
				case Sort_Alpha_Des :
					fprintf(fp, "Reverse Alphabetical ");
					break;
				case Sort_Type_Asc :
					fprintf(fp, "Type ");
					break;
				case Sort_Type_Des :
					fprintf(fp, "Reverse Type ");
					break;
				default :
					fprintf(fp, "ERROR IN HANDLING OF SORT");
			};
			
			fprintf(fp, "%d %d\t", desc->framesTaken, desc->totalFramesTaken);
		}


		// Print out inventory
		for (int i = 0; i < 20; i++) {
			if (curNode->inventory[i].a_key == -1) {
				if (i<9) 
					fprintf(fp, "NULL ");
				else if (i==0) 
					fprintf(fp, "NULL\t");
				else
					fprintf(fp, "BLOCKED ");
				continue;
			}
				
			fprintf(fp, "%s ", getItemName(curNode->inventory[i].a_key));
		}
		
		// Print out whether or not all 58 items were created
		for (int i = 0; i < NUM_RECIPES; i++) {
			if (curNode->outputCreated[i] == 1)
				fprintf(fp, "\tTrue");
			else
				fprintf(fp, "\tFalse");
		}
		
		fprintf(fp, "\n");
			
	} while (curNode->next != NULL);
	
	return 0;
}

int alpha_sort(const void *elem1, const void *elem2) {
	struct Item item1 = *((struct Item*)elem1);
	struct Item item2 = *((struct Item*)elem2);
	// Handle case of null slots
	if (item1.a_key == -1) return -1;
	if (item2.a_key == -1) return 1;
	if (item1.a_key < item2.a_key) return -1;
	if (item1.a_key > item2.a_key) return 1;
	return 0;
}

int alpha_sort_reverse(const void *elem1, const void *elem2) {
	struct Item item1 = *((struct Item*)elem1);
	struct Item item2 = *((struct Item*)elem2);
	// Handle case of null slots
	if (item1.a_key == -1) return -1;
	if (item2.a_key == -1) return 1;
	if (item1.a_key < item2.a_key) return 1;
	if (item1.a_key > item2.a_key) return -1;
	return 0;
}

int type_sort(const void *elem1, const void *elem2) {
	struct Item item1 = *((struct Item*)elem1);
	struct Item item2 = *((struct Item*)elem2);
	// Handle case of null slots
	if (item1.t_key == -1) return -1;
	if (item2.t_key == -1) return 1;
	if (item1.t_key < item2.t_key) return -1;
	if (item1.t_key > item2.t_key) return 1;
	return 0;
}

int type_sort_reverse(const void *elem1, const void *elem2) {
	struct Item item1 = *((struct Item*)elem1);
	struct Item item2 = *((struct Item*)elem2);
	// Handle case of null slots
	if (item1.t_key == -1) return -1;
	if (item2.t_key == -1) return 1;
	if (item1.t_key < item2.t_key) return 1;
	if (item1.t_key > item2.t_key) return -1;
	return 0;
}

struct Item *getSortedInventory(struct Item *inventory, enum Action sort) {
	// We first need to copy the inventory to a new array
	struct Item *sorted_inventory = malloc(sizeof(struct Item) * 20);
	for (int i = 0; i < 20; i++) {
		sorted_inventory[i] = inventory[i];
	}

	// Use qsort and execute sort function depending on sort type
	if (sort == Sort_Alpha_Asc)
		qsort(sorted_inventory, 20, sizeof(struct Item), alpha_sort);
	else if (sort == Sort_Alpha_Des)
		qsort(sorted_inventory, 20, sizeof(struct Item), alpha_sort_reverse);
	else if (sort == Sort_Type_Asc)
		qsort(sorted_inventory, 20, sizeof(struct Item), type_sort);
	else if (sort == Sort_Type_Des)
		qsort(sorted_inventory, 20, sizeof(struct Item), type_sort_reverse);
	else
		return NULL;
	return sorted_inventory;
}

/*
int main() {
	//In the real main function from Python, we do:
	//if (getConfig("performUpdateCheck") == "True")	// LEAVE THIS OUT
		//checkForUpdates()
	//current_frame_record = getFastestRecordOnFTP
	
	//Then we call the main worker function
	//result = work(startingInventory, recipeList, invFrames, current_frame_record

	
}*/

int main() {
	struct Item inventory[] = {
		{POW_Block, POW_Block_t},
		{Icicle_Pop, Icicle_Pop_t},
		{Fright_Mask, Fright_Mask_t},
		{Spicy_Soup, Spicy_Soup_t},
		{Ink_Pasta, Ink_Pasta_t},
		{Couples_Cake, Couples_Cake_t},
		{Point_Swap, Point_Swap_t},
		{Space_Food, Space_Food_t},
		{Ultra_Shroom, Ultra_Shroom_t},
		{Golden_Leaf, Golden_Leaf_t},
		{Cake_Mix, Cake_Mix_t},
		{Courage_Shell, Courage_Shell_t},
		{Courage_Meal, Courage_Meal_t},
		{Thunder_Bolt, Thunder_Bolt_t},
		{Thunder_Rage, Thunder_Rage_t},
		{Koopa_Tea, Koopa_Tea_t},
		{Turtley_Leaf, Turtley_Leaf_t},
		{Koopasta, Koopasta_t},
		{Koopa_Bun, Koopa_Bun_t},
		{Spicy_Pasta, Spicy_Pasta_t}
	};
	
	struct Item *sorted_inventory = getSortedInventory(inventory, Sort_Type_Asc);
	printf("Before sort:");
	for (int i = 0; i < 20; i++) {
		printf("\nItem in slot %d: %d", i+1, inventory[i].a_key);
	}
	
	printf("\n\nAfter sort:");
	for (int i = 0; i < 20; i++) {
		printf("\nItem in slot %d: %d", i+1, sorted_inventory[i].a_key);
	}
	
	return 0;
}