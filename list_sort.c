// SPDX-License-Identifier: GPL-2.0
#include "list_sort.h"
#include <linux/kernel.h>
#include <linux/string.h>
#include "list.h"
#include "queue.h"

/*
 * Returns a list organized in an intermediate format suited
 * to chaining of merge() calls: null-terminated, no reserved or
 * sentinel head node, "prev" links not maintained.
 */
__attribute__((nonnull(2, 3, 4))) static struct list_head *
merge(void *priv, list_cmp_func_t cmp, struct list_head *a, struct list_head *b)
{
    // cppcheck-suppress unassignedVariable
    struct list_head *head, **tail = &head;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            *tail = a;
            tail = &a->next;
            a = a->next;
            if (!a) {
                *tail = b;
                break;
            }
        } else {
            *tail = b;
            tail = &b->next;
            b = b->next;
            if (!b) {
                *tail = a;
                break;
            }
        }
    }
    return head;
}

/*
 * Combine final list merge with restoration of standard doubly-linked
 * list structure.  This approach duplicates code from merge(), but
 * runs faster than the tidier alternatives of either a separate final
 * prev-link restoration pass, or maintaining the prev links
 * throughout.
 */
__attribute__((nonnull(2, 3, 4, 5))) static void merge_final(
    void *priv,
    list_cmp_func_t cmp,
    struct list_head *head,
    struct list_head *a,
    struct list_head *b)
{
    struct list_head *tail = head;
    uint8_t count = 0;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            tail->next = a;
            a->prev = tail;
            tail = a;
            a = a->next;
            if (!a)
                break;
        } else {
            tail->next = b;
            b->prev = tail;
            tail = b;
            b = b->next;
            if (!b) {
                b = a;
                break;
            }
        }
    }

    /* Finish linking remainder of list b on to tail */
    tail->next = b;
    do {
        /*
         * If the merge is highly unbalanced (e.g. the input is
         * already sorted), this loop may run many iterations.
         * Continue callbacks to the client even though no
         * element comparison is needed, so the client's cmp()
         * routine can invoke cond_resched() periodically.
         */
        if (unlikely(!++count))
            cmp(priv, b, b);
        b->prev = tail;
        tail = b;
        b = b->next;
    } while (b);

    /* And the final links to make a circular doubly-linked list */
    tail->next = head;
    head->prev = tail;
}

/**
 * list_sort - sort a list
 * @priv: private data, opaque to list_sort(), passed to @cmp
 * @head: the list to sort
 * @cmp: the elements comparison function
 *
 * The comparison funtion @cmp must return > 0 if @a should sort after
 * @b ("@a > @b" if you want an ascending sort), and <= 0 if @a should
 * sort before @b *or* their original order should be preserved.  This
 * gives extra flexibility over a simple ascending/descending/stable sort.
 *
 * A good way to write a comparison function is to define an element type
 * with a 'key' field of the appropriate type, and then use a macro to
 * cast a pointer to your element type and dereference the key, like this
 * (this example assumes an int key):
 *
 *     #define mycmp(priv, a, b) ({       \
 *             const typeof(*(a)) *__a = (a);  \
 *             const typeof(*(b)) *__b = (b);  \
 *             (void) (priv);      \
 *             __a->key - __b->key;     \
 *         })
 *     slist_sort(NULL, list, mycmp);
 *
 * This macro evaluates to an unparenthesized compound statement, so you
 * may use it either as a macro expression or as the single expression in
 * an expression statement.
 *
 * The comparison function's parameters are named @a and @b according to
 * the direction of the desired sort, so an ascending sort uses "(a > b)"
 * while a descending sort uses "(b > a)".  And the private data is named
 * @priv so a pointer named 'priv' will not collide with it; thus you may
 * define other private state as appropriate, either as a structure type
 * or using another name.
 *
 * Simple sort example:
 *
 *     DEFINE_SLIST_HEAD(slist);
 *
 *     struct element {
 *             int key;
 *             struct slist_node list;
 *     };
 *
 *     int main()
 *     {
 *         struct element *p, *q, *e;
 *         int i;
 *
 *         for (i = 0; i < 10; ++i) {
 *             e = malloc(sizeof(*e));
 *             e->key = 10 - i;
 *             slist_add_tail(&e->list, &slist);
 *         }
 *
 *         #define element_cmp(priv, a, b) ({  \
 *                 const struct element *__a = (a); \
 *                 const struct element *__b = (b); \
 *                 (void) (priv);          \
 *                 __a->key - __b->key;        \
 *             })
 *         list_sort(NULL, &slist, element_cmp);
 *
 *         slist_for_each_entry(p, q, &slist, list) {
 *             printf("p->key = %d\n", p->key);
 *         }
 *         return 0;
 *     }
 *
 * This is an optimized implementation equivalent to the simple mergesort
 * illustrated at
 * http://en.wikipedia.org/wiki/Merge_sort#Top-down_implementation
 *
 * The principal advantage of list_sort() is that it is guaranteed to
 * be a stable sort.
 *
 * The runtime is O(n log n) (i.e. logarithmic) for all inputs,
 * with a worst-case complexity of O(2n log n) compared to O(n log n)
 * average for qsort().
 *
 * The sort is in-place, so no extra memory is needed.
 *
 * The sort is also stable, so order of equal elements is preserved.
 *
 * The merge step of the algorithm has been optimized so that it is
 * faster than qsort() on arrays of elements that are all distinct.
 *
 * A bottom-up mergesort would be faster (in the asymptotic sense) on
 * large lists that have many shared elements.  But it can have an
 * order-of-magnitude higher worst-case time than top-down mergesort,
 * so list_sort() is better overall.
 *
 * The comparison count during merge will be almost exactly n log n,
 * which is the theoretical minimum.  Measurements suggest it will
 * typically be very close to 0.25 * n, so the merge is a factor of
 * 4 faster than a simple mergesort for normal singly-linked or
 * doubly-linked lists. (But still 20-30% slower on large data sets
 * with many equal keys than a bottom-up mergesort is.)
 *
 * The algorithm below is the "natural" variant of top-down mergesort.
 * It is based on the observation that merges can be done directly in
 * place by maintaining a "pending" list of the next element from each
 * sublist to merge.  It merges each pair of pending elements and adds
 * the new sublist head to the pending list.  When the pending list is
 * reduced to one element, the merge is complete.  The algorithm is
 * faster than the "standard" variant because it requires fewer copies.
 *
 * The use of a small pending array instead of per-list next pointers
 * makes the algorithm faster for small lists, which is where the
 * runtime is most critical.  For large lists, it is just as fast on
 * average, and has better worst-case performance than algorithms based
 * on per-element comparisons.
 *
 * The algorithm is deterministic, so it is guaranteed to finish within
 * the theoretical worst case time bound of O(2n log n).
 *
 * The algorithm's merge tree is highly skewed, so the working set
 * of size 2^k varies from 2^(k-1) (cases 3 and 5 when x == 0) to
 * 2^(k+1) - 1 (second merge of case 5 when x == 2^(k-1) - 1).
 */
__attribute__((nonnull(2, 3))) void list_sort(void *priv,
                                              struct list_head *head,
                                              list_cmp_func_t cmp)
{
    struct list_head *list = head->next, *pending = NULL;
    size_t count = 0; /* Count of pending */

    if (list == head->prev) /* Zero or one elements */
        return;

    /* Convert to a null-terminated singly-linked list. */
    head->prev->next = NULL;

    /*
     * Data structure invariants:
     * - All lists are singly linked and null-terminated; prev
     *   pointers are not maintained.
     * - pending is a prev-linked "list of lists" of sorted
     *   sublists awaiting further merging.
     * - Each of the sorted sublists is power-of-two in size.
     * - Sublists are sorted by size and age, smallest & newest at front.
     * - There are zero to two sublists of each size.
     * - A pair of pending sublists are merged as soon as the number
     *   of following pending elements equals their size (i.e.
     *   each time count reaches an odd multiple of that size).
     *   That ensures each later final merge will be at worst 2:1.
     * - Each round consists of:
     *   - Merging the two sublists selected by the highest bit
     *     which flips when count is incremented, and
     *   - Adding an element from the input as a size-1 sublist.
     */
    do {
        size_t bits;
        struct list_head **tail = &pending;

        /* Find the least-significant clear bit in count */
        for (bits = count; bits & 1; bits >>= 1)
            tail = &(*tail)->prev;
        /* Do the indicated merge */
        if (likely(bits)) {
            struct list_head *a = *tail, *b = a->prev;

            a = merge(priv, cmp, b, a);
            /* Install the merged result in place of the inputs */
            a->prev = b->prev;
            *tail = a;
        }

        /* Move one element from input list to pending */
        list->prev = pending;
        pending = list;
        list = list->next;
        pending->next = NULL;
        count++;
    } while (list);

    /* End of input; merge together all the pending lists. */
    list = pending;
    pending = pending->prev;
    for (;;) {
        struct list_head *next = pending->prev;

        if (!next)
            break;
        list = merge(priv, cmp, pending, list);
        pending = next;
    }
    /* The final merge, rebuilding prev links */
    merge_final(priv, cmp, head, pending, list);
}
// EXPORT_SYMBOL(list_sort);
