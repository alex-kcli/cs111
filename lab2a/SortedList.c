#include <stdio.h>
#include <string.h>
#include <sched.h>
#include "SortedList.h"

void SortedList_insert(SortedList_t *list, SortedListElement_t *element)
{
    SortedListElement_t *curr = list->next;
    
    if (curr == list) {
        if (opt_yield & INSERT_YIELD) {
            sched_yield();
        }
        list->next = element;
        list->prev = element;
        element->next = list;
        element->prev = list;
        return;
    }
    
    while (strcmp(curr->key, element->key) < 0) {
        if (curr->next == list) {
            if (opt_yield & INSERT_YIELD) {
                sched_yield();
            }
            curr->next = element;
            element->next = list;
            element->prev = curr;
            list->prev = element;
            return;
        }
        curr = curr->next;
    }
    
    if (opt_yield & INSERT_YIELD) {
        sched_yield();
    }
    element->prev = curr->prev;
    curr->prev->next = element;
    curr->prev = element;
    element->next = curr;
}



int SortedList_delete(SortedListElement_t *element)
{
    if (opt_yield & DELETE_YIELD) {
        sched_yield();
    }
    
    if (element->next->prev != element || element->prev->next != element) {
        return 1;
    }
    else {
        element->prev->next = element->next;
        element->next->prev = element->prev;
        return 0;
    }
}



SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key)
{
    SortedListElement_t *curr = list->next;
    
    if (opt_yield & LOOKUP_YIELD) {
        sched_yield();
    }
    
    while (curr != list) {
        if (strcmp(curr->key, key) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}



int SortedList_length(SortedList_t *list)
{
    int count = 0;
    SortedListElement_t *curr = list->next;
    
    if (opt_yield & LOOKUP_YIELD) {
        sched_yield();
    }
    
    SortedListElement_t *marker = list->next;
    int flag = 0;
    while (curr != list) {
        if (curr == marker && flag == 1)
            return -1;
        if (curr->prev->next != curr || curr->next->prev != curr) {
            return -1;
        }
        flag = 1;
        count++;
        curr = curr->next;
    }
    return count;
}
