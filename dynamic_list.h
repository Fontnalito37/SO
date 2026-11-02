/*  Autor1: Sergio López Fontenla. sergio.lopez.fontenla@udc.es
    Autor2: Lucas Pastoriza Rosales. lucas.pastoriza@udc.es
*/

#ifndef DYNAMIC_LIST_H
#define DYNAMIC_LIST_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define LNULL NULL

#ifndef MAXNOMBRE
#define MAXNOMBRE 1024
#endif

typedef struct {
    int df;                   /* descriptor de fichero */
    char nombre[MAXNOMBRE];   /* nombre del fichero */
    int modo;                 /* flags con los que se abrio (O_RDWR|O_CREAT...) */
} tItemL;

typedef struct tNode *tPosL;

struct tNode {
    tItemL data;
    tPosL next;
};

typedef tPosL tList;

void createEmptyList(tList *L);

/* {Tipo: Generadora.
    Objetivo: Crea una lista vacia y la inicializa.
    Entrada:
        L: Lista donde vamos a insertar.
    Salida: Una lista vacia.
    Poscondicion: La lista queda inicializada. } */

bool isEmptyList(tList L);

/* {Tipo: Observadora.
    Objetivo: Indicar si la lista esta vacia.
    Entrada:
        L: Lista que vamos a comprobar.
    Salida: Un booleano. } */

int countElements(tList L);

/* {Tipo: Observadora.
    Objetivo: El numero de elementos de la lista.
    Entrada:
        L: Lista que vamos a comprobar.
    Salida: Un entero (0 si la lista esta vacia). } */

tPosL first(tList L);

/* {Tipo: Observadora.
    Objetivo: Conocer la primera posicion de la lista.
    Entrada:
        L: Lista donde vamos a mirar la posicion del primer elemento.
    Salida: La primera posicion util de la lista (LNULL si esta vacia).
    Precondicion: La lista ha de estar inicializada} */

tPosL last(tList L);

/* {Tipo: Observadora.
    Objetivo: Conocer la ultima posicion de la lista.
    Entrada:
        L: Lista donde vamos a mirar la posicion del ultimo elemento.
    Salida: La ultima posicion util de la lista (LNULL si esta vacia).
    Precondicion: La lista ha de estar inicializada} */

tPosL next(tPosL p, tList L);

/* {Tipo: Observadora.
    Objetivo: Conocer la siguiente posicion de la posicion que nos pasen en la lista.
    Entrada:
        L: Lista donde vamos a mirar la posicion del siguiente elemento.
        p: Posicion de la cual queremos conocer la siguiente posicion.
    Salida: La siguiente posicion util de la lista (en caso de ser la posterior a la ultima posicion devuelve LNULL).
    Precondicion: La posicion debe de ser valida.} */

tPosL previous(tPosL p, tList L);

/* {Tipo: Observadora.
    Objetivo: Conocer la anterior posicion de la posicion que nos pasen en la lista.
    Entrada:
        L: Lista donde vamos a mirar la posicion del anterior elemento.
        p: Posicion de la cual queremos conocer la anterior posicion.
    Salida: La anterior posicion util de la lista (en caso de ser la anterior a la primera posicion devuelve LNULL).
    Precondicion: La posicion debe de ser valida.} */

void updateItem(tItemL d, tPosL p, tList *L);

/* {Tipo: Modificadora.
    Objetivo: Modificar el contenido la posicion que nos pasen en la lista.
    Entrada:
        L: Lista donde vamos a insertar el objeto.
        p: Posicion donde queremos insertar el objeto.
        d: Objeto a insertar en la lista.
    Salida: Lista modificada con el objeto nuevo incorrporado en la posicion indicada.
    Precondicion: La posicion debe de ser valida.
    Postcondicion: La posicion de los objetos de la lista no se ve modificado.} */

bool insertItem(tItemL d, tPosL p, tList *L);

/* {Tipo: Generadora.
    Objetivo: Insertar un elemento en la lista. Si p es LNULL se inserta al final;
              si no, se inserta justo ANTES de la posicion p.
    Entrada:
        L: Lista donde vamos a insertar el objeto.
        p: Posicion donde queremos insertar el objeto (LNULL = al final).
        d: Objeto a insertar en la lista.
    Salida: Un booleano indicando si pudo ser intertado o no el objeto.
    Precondicion: La posicion debe de ser valida o LNULL.
    Postcondicion: Las posiciones de los elementos de la lista posteriores a la del elemento insertado pueden haber variado.} */

void deleteAtPosition(tPosL p, tList *L);

/* {Tipo: Destructora.
    Objetivo: Eliminar el contenido la posicion que nos pasen en la lista.
    Entrada:
        L: Lista donde vamos a eliminar la posicion.
        p: Posicion que queremos eliminar.
    Salida: La lista sin la posicion eliminada.
    Precondicion: La posicion debe de ser valida.
    Postcondicion: Las posiciones de los elementos de la lista posteriores a la de la posición eliminada pueden haber variado.} */

void deleteList(tList *L);

/* {Tipo: Destructora.
    Objetivo: Liberar toda la lista.
    Salida: La lista queda vacia (LNULL). } */

tItemL getItem(tPosL p, tList L);

/* {Tipo: Observadora.
    Objetivo: Obtener el objeto.
    Entrada:
        L: Lista de donde vamos a obtener el objeto.
        p: Posicion de donde queremos obtener el objeto.
    Salida: El objeto almacenado en la posicion indicada.
    Precondicion: La posicion debe de ser valida.} */

tPosL findItem(int df, tList L);

/* {Tipo: Observadora.
    Objetivo: Encontrar la PRIMERA posicion donde se halla el descriptor df.
    Entrada:
        L: Lista donde vamos a buscar el objeto.
        df: Descriptor de fichero a buscar.
    Salida: La posicion del PRIMER objeto con ese descriptor (En caso de no encontrarlo devuelve LNULL).} */

#endif