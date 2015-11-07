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
		//printk("Copying at %p\n", (buf+offset));
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
	buf[offset] = '\0';
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
        //printk("Deleting from list\n");
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
        printk("DEL: found matching %s in head\n", (*head)->pattern);
		curr = *head;
		*head = (*head)->next;
        kfree(curr);
    }
    else {
		prev = *head;
		curr = (*head)->next;
        while (curr != NULL) {
			printk("Comparing %s with %s, plen: %u\n", curr->pattern, pat, p_len);
            if (!strncmp (curr->pattern, pat, p_len)) {
				printk("DEL: Found matching pattern: %s\n", curr->pattern);
                break;
        	}
        prev = curr;
        curr = (curr->next);
		}
		if (curr == NULL) { /* pattern not found in list */
			printk("DEL: Pattern not found in list\n");
           	rc = -1;
           	goto out;
        }
        else {
           	(prev->next) = (curr->next);
    		printk("Del: Freeing curr\n");
			kfree(curr);
        }
    }
	printList(head);
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
            printk("%s\n", temp->pattern);
            temp = (temp->next);
        }
    }
}
#if 0
/* Allocate memory and copy to struct pointer */
int struct_alloc_copy(struct pat_struct **p_struct)
{
	int ret = 0;
	*p_struct = (struct pat_struct *) kmalloc(sizeof (struct pat_struct), GFP_KERNEL);
	if (!*p_struct) {
		err = -ENOMEM;
		goto out;
	}
	if (copy_from_user(*p_struct, (struct pat_struct *) arg, sizeof (struct pat_struct))) {
		printk("Copy_from_user failed for p_struct\n");
		err = -EACCES;
		goto free_pat_struct;
	}
	(*p_struct)->pattern = NULL;
	printk("Size after copying to ker_buf: %u\n", p_struct->size);
	(*p_struct)->pattern = (char *) kmalloc((*p_struct)->size + 1, GFP_KERNEL);
	if (!(*p_struct)->pattern) {
		err = -ENOMEM;
		goto free_pat_struct;
	}
	if (copy_from_user((*p_struct)->pattern, STRUCT_PAT(arg), (*p_struct)->size)) {
		err = -EACCES;
		goto ret;
	}

out: 
	return ret;
}
#endif
