/*Autor1: Sergio López Fontenla. sergio.lopez.fontenla@udc.es
  Autor2: Lucas Pastoriza Rosales. lucas.pastoriza@udc.es
*/

#include "dynamic_list.h"

#include <stdio.h>

static bool createNode(tPosL *p)
{
    *p = (tPosL)malloc(sizeof(struct tNode));
    return *p != LNULL;
}

void createEmptyList(tList *L)
{
    *L = LNULL;
}

bool isEmptyList(tList L)
{
    return L == LNULL;
}

int countElements(tList L)
{
    int count = 0;
    for (tPosL p = first(L); p != LNULL; p = p->next)
        count++;
    return count;
}

tPosL first(tList L)
{
    return L;
}

tPosL last(tList L)
{
    tPosL p = L;
    if (p == LNULL)
        return LNULL;
    while (p->next != LNULL)
    {
        p = p->next;
    }
    return p;
}

tPosL next(tPosL p, tList L)
{
    (void)L;
    return p->next;
}

tPosL previous(tPosL p, tList L)
{
    tPosL pAux = L;
    if (p == L)
    {
        pAux = LNULL;
    }
    else
    {
        while (pAux->next != p)
        {
            pAux = pAux->next;
        }
    }
    return pAux;
}

void updateItem(tItemL d, tPosL p, tList *L)
{
    (void)L;
    p->data = d; // copia por valor: no hay fugas
}

bool insertItem(tItemL d, tPosL p, tList *L)
{
    bool aux = true;
    tPosL q;
    if (!createNode(&q))
    {
        // Si el nodo no se pudo crear devuelve false
        aux = false;
    }
    else
    {
        q->data = d;
        q->next = LNULL;
        if (isEmptyList(*L))
        {
            // Si la lista esta vacia crea el nodo y lo añade manualmente
            *L = q;
        }
        else
        {
            if (p == LNULL)
            {
                // Lo añade al final
                tPosL pAux = last(*L);
                pAux->next = q;
            }
            else
            {
                // En lugar de hallar el anterior de p, lo añadimos despues e intercambiamos informacion entre ellos
                tItemL auxItem;
                q->next = p->next;
                auxItem = p->data;
                p->data = q->data;
                q->data = auxItem;
                p->next = q;
            }
        }
    }
    return aux;
}

void deleteAtPosition(tPosL p, tList *L)
{
    tPosL q = p;
    tItemL d;
    if (p == *L)
    {
        *L = p->next;
    }
    else
    {
        if (p->next == LNULL)
        {
            // Va al ultimo elemento y lo borras
            previous(p, *L)->next = LNULL;
        }
        else
        {
            // En lugar de hallar el anterior de p, intercambiamos informacion con el siguiente para poder borrar ese nodo
            p = p->next;
            d = p->data;
            p->data = q->data;
            q->data = d;
            q->next = p->next;
        }
    }
    free(p);
}

void deleteList(tList *L)
{
    tPosL p;
    while (*L != LNULL)
    {
        p = *L;
        *L = p->next;
        free(p);
    }
}

tItemL getItem(tPosL p, tList L)
{
    (void)L;
    return p->data;
}

tPosL findItem(int df, tList L)
{
    tPosL p;
    for (p = L; (p != LNULL) && p->data.df != df; p = p->next)
    {
        /* recorre hasta encontrarlo o llegar al final */
    }
    return p;
}
