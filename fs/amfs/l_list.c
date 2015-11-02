#include "l_list.h"
#include "amfs.h"

/* To add new patterns to the linked list data structure */
int addtoList(struct ListNode **head, char *pat, int p_len)
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
    newNode->pattern[p_len] = '\0';
    if (*head == NULL) {
        *head = newNode;
        goto out;
    }
    else {
        temp = (*head);
        while (temp->next != NULL)
            temp = (temp->next);
        (temp->next) = newNode;
        goto out;   
    }
free_newNode:
    kfree(newNode);
out:
    return rc;
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
        kfree(prev);
    }
    *head = NULL;
}

/* Delete a matching pattern and adjust the reminaing list */
int deletePatternInList(struct ListNode **head, char *pat, int p_len)
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
            printk("%s\n", temp->pattern);
            temp = (temp->next);
        }
    }
}

