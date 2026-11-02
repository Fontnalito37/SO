/*  Autor1: Sergio López Fontenla. sergio.lopez.fontenla@udc.es
    Autor2: Lucas Pastoriza Rosales. lucas.pastoriza@udc.es
*/

#include "mem_list.h"

static bool createNodeM(tPosM *p)
{
    *p = (tPosM)malloc(sizeof(struct tNodeM));
    return *p != MNULL;
}

void createEmptyListM(tListM *L)
{
    *L = MNULL;
}

bool isEmptyListM(tListM L)
{
    return L == MNULL;
}

tPosM firstM(tListM L)
{
    return L;
}

tPosM lastM(tListM L)
{
    tPosM p = L;
    if (p == MNULL)
        return MNULL;
    while (p->next != MNULL)
    {
        p = p->next;
    }
    return p;
}

tPosM nextM(tPosM p, tListM L)
{
    (void)L;
    return p->next;
}

tPosM previousM(tPosM p, tListM L)
{
    tPosM pAux = L;
    if (p == L)
    {
        pAux = MNULL;
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

bool insertItemM(tItemM d, tPosM p, tListM *L)
{
    bool aux = true;
    tPosM q;
    if (!createNodeM(&q))
    {
        // Si el nodo no se pudo crear devuelve false
        aux = false;
    }
    else
    {
        q->data = d;
        q->next = MNULL;
        if (isEmptyListM(*L))
        {
            // Si la lista esta vacia crea el nodo y lo añade manualmente
            *L = q;
        }
        else
        {
            if (p == MNULL)
            {
                // Lo añade al final
                tPosM pAux = lastM(*L);
                pAux->next = q;
            }
            else
            {
                // En lugar de hallar el anterior de p, lo añadimos despues e intercambiamos informacion entre ellos
                tItemM auxItem;
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

void deleteAtPositionM(tPosM p, tListM *L)
{
    tPosM q = p;
    tItemM d;
    if (p == *L)
    {
        *L = p->next;
    }
    else
    {
        if (p->next == MNULL)
        {
            // Va al ultimo elemento y lo borras
            previousM(p, *L)->next = MNULL;
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

void deleteListM(tListM *L)
{
    tPosM p;
    while (*L != MNULL)
    {
        p = *L;
        *L = p->next;
        free(p);
    }
}

tItemM getItemM(tPosM p, tListM L)
{
    (void)L;
    return p->data;
}

tPosM findByAddrM(void *dir, tListM L)
{
    tPosM p;
    for (p = L; (p != MNULL) && p->data.dir != dir; p = p->next)
    {
        /* recorre hasta encontrarlo o llegar al final */
    }
    return p;
}

tPosM findBySizeM(size_t tam, tListM L)
{
    tPosM p;
    for (p = L; (p != MNULL) && !(p->data.tipo == T_MALLOC && p->data.tam == tam); p = p->next)
    {
    }
    return p;
}

tPosM findByKeyM(key_t clave, tListM L)
{
    tPosM p;
    for (p = L; (p != MNULL) && !(p->data.tipo == T_SHARED && p->data.clave == clave); p = p->next)
    {
    }
    return p;
}

tPosM findByFileM(const char *fichero, tListM L)
{
    tPosM p;
    for (p = L; (p != MNULL) && !(p->data.tipo == T_MMAP && !strcmp(p->data.fichero, fichero)); p = p->next)
    {
    }
    return p;
}

void printListM(tListM L, tTipoM filtro)
{
    tPosM p;
    char fecha[64];
    struct tm *lt;

    for (p = firstM(L); p != MNULL; p = nextM(p, L))
    {
        if (filtro != T_TODOS && p->data.tipo != filtro)
            continue;

        lt = localtime(&p->data.hora);
        if (lt == NULL)
            strcpy(fecha, "?");
        else
            strftime(fecha, sizeof(fecha), "%b %d %H:%M", lt);

        printf("\t%p\t%10zu %s ", p->data.dir, p->data.tam, fecha);
        switch (p->data.tipo)
        {
        case T_MALLOC:
            printf("malloc\n");
            break;
        case T_SHARED:
            printf("shared (key %d)\n", (int)p->data.clave);
            break;
        case T_MMAP:
            printf("%s (descriptor %d)\n", p->data.fichero, p->data.df);
            break;
        default:
            printf("?\n");
        }
    }
}
