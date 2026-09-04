#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    char *value;
    struct Node *next;
    struct Node *previous;
} Node;

typedef struct {
    Node *head;
    Node *tail;
    size_t size;
} StringList;

static char *copy_string(const char *source) {
    size_t length = strlen(source) + 1;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, source, length);
    }
    
    return copy;
} 

void list_init(StringList *list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

bool list_insert(StringList *list, const char *value) {
    if (list == NULL || value == NULL) {
        return false;
    }

    Node *node = malloc(sizeof(*node));
    if (node == NULL) {
        return false;
    }

    node->value = copy_string(value);
    if(node->value == NULL) {
        free(node);
        return false;
    }

    node->previous = list->tail;
    node->next = NULL;

    if (list->tail != NULL) {
        list->tail->next = node;
    } else {
        list->head = node;
    }

    list->tail = node;
    list->size++;
    return true;
}

Node *list_find(StringList *list, const char *value)
{
    if (list == NULL || value == NULL) {
        return NULL;
    }

    Node *current = list->head;

    while (current != NULL) {
        if (strcmp(current->value, value) == 0) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

bool list_delete(StringList *list, const char *value)
{
    Node *node = list_find(list, value);
    if (node == NULL) {
        return false;
    }

    if (node->previous != NULL) {
        node->previous->next = node->next;
    } else {
        list->head = node->next;
    }

    if (node->next != NULL) {
        node->next->previous = node->previous;
    } else {
        list->tail = node->previous;
    }

    free(node->value);
    free(node);
    list->size--;
    return true;
}

void list_destroy(StringList *list)
{
    if (list == NULL) {
        return;
    }

    Node *current = list->head;
    while (current != NULL) {
        Node *next = current->next;
        free(current->value);
        free(current);
        current = next;
    }
    list_init(list);
}

static void test_list(void)
{
    StringList list;
    list_init(&list);

    /* Test insertion and independently allocated string storage. */
    char editable[] = "beta";
    assert(list_insert(&list, "alpha"));
    assert(list_insert(&list, editable));
    assert(list_insert(&list, "gamma"));
    editable[0] = 'z';

    assert(list.size == 3);
    assert(strcmp(list.head->value, "alpha") == 0);
    assert(strcmp(list.tail->value, "gamma") == 0);
    assert(strcmp(list.head->next->value, "beta") == 0);
    assert(list.head->previous == NULL);
    assert(list.tail->next == NULL);

    /* Test successful and unsuccessful searches. */
    Node *beta = list_find(&list, "beta");
    assert(beta != NULL);
    assert(beta->previous == list.head);
    assert(beta->next == list.tail);
    assert(list_find(&list, "missing") == NULL);

    /* Test deletion from the middle, head, and tail. */
    assert(list_delete(&list, "beta"));
    assert(list.size == 2);
    assert(list.head->next == list.tail);
    assert(list.tail->previous == list.head);

    assert(list_delete(&list, "alpha"));
    assert(list.size == 1);
    assert(list.head == list.tail);
    assert(list.head->previous == NULL);

    assert(list_delete(&list, "gamma"));
    assert(list.size == 0);
    assert(list.head == NULL && list.tail == NULL);
    assert(!list_delete(&list, "missing"));

    /* Test cleanup of a nonempty list. */
    assert(list_insert(&list, "one"));
    assert(list_insert(&list, "two"));
    list_destroy(&list);
    assert(list.size == 0);
    assert(list.head == NULL && list.tail == NULL);
}

int main(void)
{
    test_list();
    puts("All doubly linked list tests passed.");
    return 0;
}