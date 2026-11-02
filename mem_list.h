/*  Autor1: Sergio López Fontenla. sergio.lopez.fontenla@udc.es
    Autor2: Lucas Pastoriza Rosales. lucas.pastoriza@udc.es
*/

#ifndef MEM_LIST_H
#define MEM_LIST_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#define MNULL NULL

#ifndef MAXNOMBRE
#define MAXNOMBRE 1024
#endif

/* Tipos de bloque. T_TODOS solo se usa como filtro al imprimir. */
typedef enum
{
    T_MALLOC,
    T_SHARED,
    T_MMAP,
    T_TODOS
} tTipoM;

typedef struct
{
    void *dir;               /* direccion del bloque */
    size_t tam;              /* tamano en bytes */
    time_t hora;             /* instante de la asignacion */
    tTipoM tipo;             /* malloc, shared o mmap */
    key_t clave;             /* solo shared: clave */
    char fichero[MAXNOMBRE]; /* solo mmap: nombre del fichero */
    int df;                  /* solo mmap: descriptor del fichero */
} tItemM;

typedef struct tNodeM *tPosM;

struct tNodeM
{
    tItemM data;
    tPosM next;
};

typedef tPosM tListM;

void createEmptyListM(tListM *L);

/* {Tipo: Generadora.
    Objetivo: Crea una lista vacia y la inicializa.
    Entrada:
        L: Lista que vamos a inicializar.
    Poscondicion: La lista queda inicializada y vacia. } */

bool isEmptyListM(tListM L);

/* {Tipo: Observadora.
    Objetivo: Indicar si la lista esta vacia.
    Salida: Un booleano. } */

tPosM firstM(tListM L);

/* {Tipo: Observadora.
    Objetivo: Conocer la primera posicion de la lista.
    Salida: La primera posicion (MNULL si esta vacia).
    Precondicion: La lista ha de estar inicializada} */

tPosM lastM(tListM L);

/* {Tipo: Observadora.
    Objetivo: Conocer la ultima posicion de la lista.
    Salida: La ultima posicion (MNULL si esta vacia).
    Precondicion: La lista ha de estar inicializada} */

tPosM nextM(tPosM p, tListM L);

/* {Tipo: Observadora.
    Objetivo: Conocer la posicion siguiente a p.
    Salida: La siguiente posicion (MNULL si p es la ultima).
    Precondicion: La posicion debe de ser valida.} */

tPosM previousM(tPosM p, tListM L);

/* {Tipo: Observadora.
    Objetivo: Conocer la posicion anterior a p.
    Salida: La anterior posicion (MNULL si p es la primera).
    Precondicion: La posicion debe de ser valida.} */

bool insertItemM(tItemM d, tPosM p, tListM *L);

/* {Tipo: Generadora.
    Objetivo: Insertar un elemento en la lista. Si p es MNULL se inserta al final;
              si no, se inserta justo ANTES de la posicion p.
    Salida: Un booleano indicando si pudo ser insertado o no el objeto.
    Precondicion: La posicion debe de ser valida o MNULL.
    Postcondicion: Las posiciones de los elementos posteriores pueden haber variado.} */

void deleteAtPositionM(tPosM p, tListM *L);

/* {Tipo: Destructora.
    Objetivo: Eliminar el elemento de la posicion p. NO libera el bloque de memoria
              que describe (eso lo hace quien llama: free, shmdt, munmap).
    Precondicion: La posicion debe de ser valida.
    Postcondicion: Las posiciones de los elementos posteriores pueden haber variado.} */

void deleteListM(tListM *L);

/* {Tipo: Destructora.
    Objetivo: Liberar todos los nodos de la lista (no los bloques que describen).
    Salida: La lista queda vacia (MNULL). } */

tItemM getItemM(tPosM p, tListM L);

/* {Tipo: Observadora.
    Objetivo: Obtener el elemento almacenado en la posicion p.
    Precondicion: La posicion debe de ser valida.} */

tPosM findByAddrM(void *dir, tListM L);

/* {Tipo: Observadora.
    Objetivo: Encontrar la PRIMERA posicion cuyo bloque (de cualquier tipo) empieza en dir.
    Salida: La posicion (MNULL si no se encuentra).} */

tPosM findBySizeM(size_t tam, tListM L);

/* {Tipo: Observadora.
    Objetivo: Encontrar el PRIMER bloque de tipo malloc con tamano tam.
    Salida: La posicion (MNULL si no se encuentra).} */

tPosM findByKeyM(key_t clave, tListM L);

/* {Tipo: Observadora.
    Objetivo: Encontrar el PRIMER bloque de tipo shared con clave clave.
    Salida: La posicion (MNULL si no se encuentra).} */

tPosM findByFileM(const char *fichero, tListM L);

/* {Tipo: Observadora.
    Objetivo: Encontrar el PRIMER bloque de tipo mmap con ese nombre de fichero.
    Salida: La posicion (MNULL si no se encuentra).} */

void printListM(tListM L, tTipoM filtro);

/* {Tipo: Observadora.
    Objetivo: Imprimir los bloques del tipo filtro (T_TODOS = todos).} */

#endif