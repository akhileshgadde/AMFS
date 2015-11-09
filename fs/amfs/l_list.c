#include "l_list.h"
#include "amfs.h"

/* To add new patterns to the linked list data structure */
int addtoList(struct ListNode **head, char *pat, unsigned int p_len)
{
    int rc = 0;
    struct ListNode *temp, *newNode;
    newNode = (struct ListNode *) kmalloc(sizeof(struct ListNode), GFP_KERNEL);
    if (!newNode) {
        rc = -ENOMEM;
        goto out;
    }
    newNode->pattern = (char *) kmalloc(p_len+1, GFP_KERNEL);
    if (!newNode->pattern) {
        rc = -ENOMEM;
        goto free_newNode;
    }
    strcpy(newNode->pattern, pat);
    newNode->next = NULL;
	newNode->len = p_len;
    newNode->pattern[p_len] = '\0';
    if (*head == NULL) {
        *head = newNode;
    }
    else {
        temp = (*head);
        while (temp->next != NULL)
            temp = (temp->next);
        (temp->next) = newNode;
    }
    goto out;   
free_newNode:
    kfree(newNode);
out:
    return rc;
}

/* Get the total size of buffer needed to store all patterns*/
unsigned long get_patterndb_len(struct ListNode *head)
{
    struct ListNode *temp;
    unsigned long count = 0;
    temp = head;
    while (temp != NULL)
    {
        count += (temp->len + 1);
        temp = temp->next;
    }
    return count;
}

/* Copy the pattern db into given char buffer */
void copy_pattern_db(struct ListNode *head, char *buf, unsigned long size)
{
	unsigned offset = 0;
	int last_char_flag = 0; 
	//int first_copy = 1;
	struct ListNode *temp = head;
	while ((size) && (temp != NULL)) {
		memcpy((buf+offset), temp->pattern, temp->len);
		if (!last_char_flag) {
			last_char_flag = 1;
		}
		(buf + offset)[temp->len] = '\n';
		offset += (temp->len + 1);
		size -= (temp->len + 1);
		temp = temp->next;	
	}
	buf[offset] = '\0';
}

void print_pattern_db(char *buf, unsigned long size)
{
	unsigned offset = 0;
	while (size) {
		size -= (strlen(buf) + 1);
		offset += (strlen(buf) + 1);
	}
}

/* To delete the complete linked list data structure */
void delAllFromList(struct ListNode **head)
{
    struct ListNode *prev, *curr;
    curr = (*head);
    while (curr != NULL)
    {
		prev = curr;
        curr = (curr->next);
		if (prev->pattern)
			kfree(prev->pattern);
        kfree(prev);
    }
    *head = NULL;
}

/* Delete a matching pattern and adjust the reminaing list */
int deletePatternInList(struct ListNode **head, char *pat, unsigned int p_len)
{
    int rc = 0;
    struct ListNode *prev = NULL, *curr = NULL;
    if (!strncmp((*head)->pattern, pat, p_len)) {
		curr = *head;
		*head = (*head)->next;
        kfree(curr);
    }
    else {
		prev = *head;
		curr = (*head)->next;
        while (curr != NULL) {
            if (!strncmp (curr->pattern, pat, p_len)) {
                break;
        	}
        prev = curr;
        curr = (curr->next);
		}
		if (curr == NULL) { /* pattern not found in list */
           	rc = -EINVAL;
           	goto out;
        }
        else {
           	(prev->next) = (curr->next);
			kfree(curr);
        }
    }
	//printList(head);
out:
    return rc;
}

/* print the complete list */
void printList(struct ListNode **head)
{
    struct ListNode *temp = *head;
    if (*head == NULL) {
        printk("KERN_AMFS: No Patterns to display\n");
        return;
    }
    else {
        printk("KERN_AMFS: Patterns List:\n");
        while (temp != NULL) {
            printk("%s\n", temp->pattern);
            temp = (temp->next);
        }
    }
}
