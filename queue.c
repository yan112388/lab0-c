#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */


/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *head = malloc(sizeof(struct list_head));
    if (!head)
        return NULL;
    INIT_LIST_HEAD(head);
    return head;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (!l)
        return;
    element_t *pos, *next;
    list_for_each_entry_safe (pos, next, l, list) {
        q_release_element(pos);
    }
    free(l);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *newh = malloc(sizeof(element_t));

    if (!newh)
        return false;

    newh->value = strdup(s);
    if (!newh->value) {
        free(newh);
        return false;
    }
    list_add(&newh->list, head);
    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *newt = malloc(sizeof(element_t));

    if (!newt)
        return false;

    newt->value = strdup(s);
    if (!newt->value) {
        free(newt);
        return false;
    }
    list_add_tail(&newt->list, head);
    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    element_t *ele = list_first_entry(head, element_t, list);

    if (sp) {
        strncpy(sp, ele->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }
    list_del(&ele->list);
    return ele;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    element_t *ele = list_last_entry(head, element_t, list);

    if (sp) {
        strncpy(sp, ele->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }
    list_del(&ele->list);
    return ele;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head)
        return 0;

    int len = 0;
    struct list_head *li;

    list_for_each (li, head)
        len++;
    return len;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    if (!head || list_empty(head))
        return NULL;

    struct list_head *slow = head->next;
    for (struct list_head *fast = head->next;
         fast != head && fast->next != head; fast = fast->next->next)
        slow = slow->next;

    struct list_head *to_delete = slow;
    list_del_init(to_delete);
    q_release_element(container_of(to_delete, element_t, list));
    // https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    if (!head || list_empty(head))
        return false;
    bool is_dup = false;
    element_t *curr_ele, *next_ele;
    list_for_each_entry_safe (curr_ele, next_ele, head, list) {
        if (curr_ele->list.next != head &&
            strcmp(curr_ele->value, next_ele->value) == 0) {
            list_del(&curr_ele->list);
            q_release_element(curr_ele);
            is_dup = true;
        } else if (is_dup) {
            list_del(&curr_ele->list);
            q_release_element(curr_ele);
            is_dup = false;
        }
    }
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    struct list_head *first = head->next;
    while (first != head && first->next != head) {
        struct list_head *second = first->next;
        list_move(first, second);
        first = first->next;
    }
    // https://leetcode.com/problems/swap-nodes-in-pairs/
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    struct list_head *pre = head->prev, *curr = head, *nex = NULL;
    while (nex != head) {
        nex = curr->next;
        curr->next = pre;
        curr->prev = nex;
        pre = curr;
        curr = nex;
    }
}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    // https://leetcode.com/problems/reverse-nodes-in-k-group/
}


struct list_head *mergeTwo(struct list_head *left, struct list_head *right)
{
    struct list_head head, *curr = &head;
    if (!left && !right)
        return NULL;

    while (left && right) {
        if (strcmp(list_entry(left, element_t, list)->value,
                   list_entry(right, element_t, list)->value) < 0) {
            curr->next = left;
            left = left->next;
            curr = curr->next;
        } else {
            curr->next = right;
            right = right->next;
            curr = curr->next;
        }
    }

    if (left) {
        curr->next = left;
    } else if (right) {
        curr->next = right;
    }
    return head.next;
}


struct list_head *q_mergeSort(struct list_head *head)
{
    if (!head->next)
        return head;

    struct list_head *slow = head;
    for (struct list_head *fast = head->next; fast && fast->next;
         fast = fast->next->next)
        slow = slow->next;

    struct list_head *mid = slow->next;
    slow->next = NULL;
    struct list_head *left = q_mergeSort(head), *right = q_mergeSort(mid);

    return mergeTwo(left, right);
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend)
{
    if (!head || list_empty(head))
        return;

    head->prev->next = NULL;
    head->next = q_mergeSort(head->next);

    struct list_head *curr = head, *nex = head->next;
    while (nex) {
        nex->prev = curr;
        curr = nex;
        nex = nex->next;
    }
    curr->next = head;
    head->prev = curr;
}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;

    struct list_head *node = (head->prev);

    while (node->prev != (head)) {
        element_t *curr_ele = list_entry(node, element_t, list);
        element_t *prev_ele = list_entry(node->prev, element_t, list);
        if (strcmp(curr_ele->value, prev_ele->value) < 0) {
            list_del(node->prev);
            q_release_element(prev_ele);
        } else {
            node = node->prev;
        }
    }
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    return 0;
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;

    struct list_head *node = (head->prev);

    while (node->prev != (head)) {
        element_t *curr_ele = list_entry(node, element_t, list);
        element_t *prev_ele = list_entry(node->prev, element_t, list);
        if (strcmp(curr_ele->value, prev_ele->value) > 0) {
            list_del(node->prev);
            q_release_element(prev_ele);
        } else {
            node = node->prev;
        }
    }
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    return 0;
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/
    return 0;
}
