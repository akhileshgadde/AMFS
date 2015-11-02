#ifndef _AMFS_LIST_
#define _AMFS_LIST_

struct ListNode
{
	char *pattern;
	struct ListNode *next;
};

//typedef struct L_List ListNode;

int addtoList(struct ListNode **head, char *pat, int p_len);
void delAllFromList(struct ListNode **head);
int deletePatternInList(struct ListNode **head, char *pat, int p_len);
void printList(struct ListNode **head);

#endif
