#ifndef _AMFS_LIST_
#define _AMFS_LIST_

struct ListNode
{
	char *pattern;
	unsigned int len;
	struct ListNode *next;
};

//typedef struct L_List ListNode;

int addtoList(struct ListNode **head, char *pat, unsigned int p_len);
void delAllFromList(struct ListNode **head);
int deletePatternInList(struct ListNode **head, char *pat, unsigned int p_len);
void printList(struct ListNode **head);
unsigned long get_patterndb_len(struct ListNode *head);
void copy_pattern_db(struct ListNode *head, char *buf, unsigned long size);
void print_pattern_db(char *buf, unsigned long size);
#endif
