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
		printk("Copying at %p\n", (buf+offset));
		memcpy((buf+offset), temp->pattern, temp->len);
		if (!last_char_flag) {
			last_char_flag = 1;
		}
		//printk("Setting \n at %p\n", (buf + offset)[temp->len]);
		(buf + offset)[temp->len] = '\n';
		offset += (temp->len + 1);
		size -= (temp->len + 1);
		temp = temp->next;	
	}
	//buf[size] = '\0';
}

void print_pattern_db(char *buf, unsigned long size)
{
	unsigned offset = 0;
	while (size) {
		printk("%s", buf+offset);
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
    struct ListNode *prev, *curr = *head;
    if (!strncmp((*head)->pattern, pat, p_len)) {
        *head = (*head)->next;
        goto freeNode;
    }
    else {
        while (curr != NULL)
        {
            prev = curr;
            curr = (curr->next);
            if (!strncmp (curr->pattern, pat, p_len))
                break;
        }
        if (curr == NULL) { /* pattern not found in list */
            rc = -1;
            goto out;
        }
        else {
            (prev->next) = (curr->next);
            goto freeNode;
        }
    }
freeNode:
    kfree(curr);
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
        printk("KERN_AMFS: Patterns \n");
        while (temp != NULL) {
            printk("%s, len:%u\n", temp->pattern, temp->len);
            temp = (temp->next);
        }
    }
}

