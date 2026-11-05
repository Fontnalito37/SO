/*  Autor1: Sergio López Fontenla. sergio.lopez.fontenla@udc.es
    Autor2: Lucas Pastoriza Rosales. lucas.pastoriza@udc.es
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "dynamic_list.h"
#include "mem_list.h"

#define MAXENTRADA 2048
#define MAXNOMBRE 1024

typedef struct
{
    tList abiertos;
    tListM memoria;

} tShellLists;

struct COMANDO
{
    char *nombre;
    void (*funcion)(char **, tShellLists *);
};

void MostrarDirActual()
{
    char dir[MAXNOMBRE];

    if (getcwd(dir, MAXNOMBRE) == NULL)
        perror("Imposible obtener directorio");
    else
        printf("%s\n", dir);
}

int ComprobarSegundoPlano(char *tr[])
{
    int i;
    for (i = 0; tr[i] != NULL; i++)
        if (!strcmp(tr[i], "&"))
        {                 /*& indica segund plano*/
            tr[i] = NULL; /*es el ultimo argumento*/
            return i;     /*si solo hay un & no se ejecuta nada en pplano*/
        }
    return 0;
}

void Proceso(char *tr[], int splano)
{
    pid_t pid;
    void Cmd_exec(char **, tShellLists *);
    int background = splano || ComprobarSegundoPlano(tr);
    if ((pid = fork()) == -1)
    {
        perror("Imposible crear proceso");
        return;
    }
    if (pid == 0)
    { /*proceso hijo*/
        Cmd_exec(tr, NULL);
        exit(255); /*por si falla exec*/
    }
    if (!background)
        waitpid(pid, NULL, 0);
}

int AnadirAbierto(int df, const char *nombre, int modo, tList *listaAbiertos)
{
    tItemL it;
    tPosL q;

    it.df = df;
    it.modo = modo;
    strncpy(it.nombre, nombre, MAXNOMBRE - 1);
    it.nombre[MAXNOMBRE - 1] = '\0';

    // inserta ordenado por descriptor: antes del primero con df mayor (LNULL = al final)
    for (q = first(*listaAbiertos); q != LNULL && q->data.df < df; q = next(q, *listaAbiertos))
        ;
    return insertItem(it, q, listaAbiertos) ? 0 : -1;
}

void EliminarAbierto(int df, tList *listaAbiertos)
{
    tPosL p = findItem(df, *listaAbiertos);
    if (p != LNULL)
        deleteAtPosition(p, listaAbiertos);
}

const char *ModoAString(int modo)
{
    static char buf[64];
    int acc = modo & O_ACCMODE;

    /* O_RDONLY vale 0 y la referencia no imprime ese modo; solo muestra
       los modos que tienen una bandera efectiva distinta de cero. */
    strcpy(buf, acc == O_RDWR ? "O_RDWR" : acc == O_WRONLY ? "O_WRONLY"
                                                           : "");
    if (modo & O_CREAT)
        strcat(buf, "|O_CREAT");
    if (modo & O_EXCL)
        strcat(buf, "|O_EXCL");
    if (modo & O_APPEND)
        strcat(buf, "|O_APPEND");
    if (modo & O_TRUNC)
        strcat(buf, "|O_TRUNC");
    return buf;
}

void InicializarLista(tList *listaAbiertos)
{
    int df, modo;
    char ruta[64], nombre[MAXNOMBRE];
    ssize_t n;

    createEmptyList(listaAbiertos);
    for (df = 0; df < 3; df++)
    {
        if ((modo = fcntl(df, F_GETFL)) == -1)
            continue; // no está abierto
        snprintf(ruta, sizeof(ruta), "/proc/self/fd/%d", df);
        n = readlink(ruta, nombre, MAXNOMBRE - 1);
        if (n == -1)
            strcpy(nombre, "?");
        else
            nombre[n] = '\0';

        /* Nombres que utiliza la referencia para los tres descriptores
           estándar, independientemente de si apuntan a una terminal, un
           fichero o una tubería. */
        if (df == STDIN_FILENO)
            strcpy(nombre, "entrada estandar");
        else if (df == STDOUT_FILENO)
            strcpy(nombre, "salida estandar");
        else if (df == STDERR_FILENO)
            strcpy(nombre, "error estandar");

        AnadirAbierto(df, nombre, modo, listaAbiertos);
    }
}

int BorrarArbol(const char *ruta)
{
    struct stat st;
    DIR *d;
    struct dirent *e;
    char hijo[MAXNOMBRE];
    int fallo = 0;

    if (lstat(ruta, &st) == -1)
    {
        printf("Imposible borrar %s: %s\n", ruta, strerror(errno));
        return -1;
    }
    if (!S_ISDIR(st.st_mode))
    {
        if (unlink(ruta) == -1)
        {
            printf("Imposible borrar %s: %s\n", ruta, strerror(errno));
            return -1;
        }
        return 0;
    }

    if ((d = opendir(ruta)) == NULL)
    {
        printf("Imposible borrar %s: %s\n", ruta, strerror(errno));
        return -1;
    }
    while ((e = readdir(d)) != NULL)
    {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue; /* nunca recursar en . ni .. */
        if (snprintf(hijo, sizeof(hijo), "%s/%s", ruta, e->d_name) >= (int)sizeof(hijo))
        {
            printf("Imposible borrar %s/%s: %s\n", ruta, e->d_name, strerror(ENAMETOOLONG));
            fallo = 1;
            continue;
        }
        if (BorrarArbol(hijo) == -1)
            fallo = 1;
    }
    closedir(d);

    if (fallo) // si algo dentro no se pudo borrar: el directorio no quedara vacio
        return -1;
    if (rmdir(ruta) == -1)
    {
        printf("Imposible borrar %s: %s\n", ruta, strerror(errno));
        return -1;
    }
    return 0;
}

char LetraTF(mode_t m)
{
    switch (m & S_IFMT)
    { /*and bit a bit con los bits de formato,0170000 */
    case S_IFSOCK:
        return 's'; /*socket */
    case S_IFLNK:
        return 'l'; /*symbolic link*/
    case S_IFREG:
        return '-'; /* fichero normal*/
    case S_IFBLK:
        return 'b'; /*block device*/
    case S_IFDIR:
        return 'd'; /*directorio */
    case S_IFCHR:
        return 'c'; /*char device*/
    case S_IFIFO:
        return 'p'; /*pipe*/
    default:
        return '?'; /*desconocido, no deberia aparecer*/
    }
}

char *ConvierteModo2(mode_t m)
{
    static char permisos[12];
    strcpy(permisos, "---------- ");

    permisos[0] = LetraTF(m);
    if (m & S_IRUSR)
        permisos[1] = 'r'; /*propietario*/
    if (m & S_IWUSR)
        permisos[2] = 'w';
    if (m & S_IXUSR)
        permisos[3] = 'x';
    if (m & S_IRGRP)
        permisos[4] = 'r'; /*grupo*/
    if (m & S_IWGRP)
        permisos[5] = 'w';
    if (m & S_IXGRP)
        permisos[6] = 'x';
    if (m & S_IROTH)
        permisos[7] = 'r'; /*resto*/
    if (m & S_IWOTH)
        permisos[8] = 'w';
    if (m & S_IXOTH)
        permisos[9] = 'x';
    if (m & S_ISUID)
        permisos[3] = 's'; /*setuid, setgid y stickybit*/
    if (m & S_ISGID)
        permisos[6] = 's';
    if (m & S_ISVTX)
        permisos[9] = 't';

    return permisos;
}

const char *NombreUsuario(uid_t uid)
{
    struct passwd *p = getpwuid(uid);
    return (p != NULL) ? p->pw_name : "??????"; /* usuario que ya no existe */
}

const char *NombreGrupo(gid_t gid)
{
    struct group *g = getgrgid(gid);
    return (g != NULL) ? g->gr_name : "??????"; /* grupo que ya no existe */
}

const char *FechaHora(time_t t)
{
    static char buf[64];
    struct tm *lt = localtime(&t);

    if (lt == NULL)
        strcpy(buf, "?");
    else
        snprintf(buf, sizeof(buf), "%4d/%02d/%02d-%02d:%02d",
                 lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min);
    return buf;
}

char *InfoFichero(int corto, const char *dir, const char *nombre, int enlace, int acc)
{
    static char salida[MAXNOMBRE];
    char ruta[MAXNOMBRE], destino[MAXNOMBRE];
    struct stat st;
    ssize_t n;
    size_t len;

    if (dir != NULL)
        snprintf(ruta, sizeof(ruta), "%s/%s", dir, nombre);
    else
        snprintf(ruta, sizeof(ruta), "%s", nombre);

    if (lstat(ruta, &st) == -1)
    { /* lstat: nunca stat, para ver los enlaces como tales */
        snprintf(salida, sizeof(salida), " ****error al acceder a %s:%s", nombre, strerror(errno));
        return salida;
    }

    if (corto)
    {
        if (acc)
            snprintf(salida, sizeof(salida), " %7lu  %s %s",
                     (unsigned long)st.st_size, FechaHora(st.st_atime), nombre);
        else
            snprintf(salida, sizeof(salida), " %7lu  %s", (unsigned long)st.st_size, nombre);
    }
    else
    {
        snprintf(salida, sizeof(salida), "%s %3lu (%8lu)%9s%9s %s%8lu %s",
                 FechaHora(acc ? st.st_atime : st.st_mtime),
                 (unsigned long)st.st_nlink, (unsigned long)st.st_ino,
                 NombreUsuario(st.st_uid), NombreGrupo(st.st_gid),
                 ConvierteModo2(st.st_mode), (unsigned long)st.st_size, nombre);
    }

    if (enlace && S_ISLNK(st.st_mode))
    {
        if ((n = readlink(ruta, destino, sizeof(destino) - 1)) == -1)
            snprintf(destino, sizeof(destino), "**error al leer enlace: %s", strerror(errno));
        else
            destino[n] = '\0'; /* readlink no pone el '\0' */
        len = strlen(salida);
        snprintf(salida + len, sizeof(salida) - len, " -> %s", destino);
    }
    return salida;
}

void ProcesarDirectorio(const char *dir, int corto, int enlace, int acc, int hid, int reca, int recb)
{
    DIR *d;
    struct dirent *ent;
    char ruta[MAXNOMBRE];
    struct stat st;

    if ((d = opendir(dir)) == NULL)
    {
        printf(" ****error al abrir directorio %s: %s\n", dir, strerror(errno));
        return;
    }

    /* 1. En -recb se visitan primero los subdirectorios, de modo que la
       cabecera del directorio actual se imprime después de las hijas. */
    if (recb)
    {
        while ((ent = readdir(d)) != NULL)
        {
            if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
                continue;
            if (!hid && ent->d_name[0] == '.')
                continue; // Ocultos

            snprintf(ruta, sizeof(ruta), "%s/%s", dir, ent->d_name);
            // Usamos lstat para comprobar si es un directorio, sin usar d_type
            if (lstat(ruta, &st) != -1 && S_ISDIR(st.st_mode))
            {
                ProcesarDirectorio(ruta, corto, enlace, acc, hid, reca, recb);
            }
        }
        rewinddir(d);
    }

    printf("************%s\n", dir);

    /* 2. Listado normal del contenido del directorio actual. Con -hid
       también se muestran . y ... */
    while ((ent = readdir(d)) != NULL)
    {
        if (!hid && (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")))
            continue;
        if (!hid && ent->d_name[0] == '.')
            continue;

        // Reutilizamos tu InfoFichero, pasándole el directorio padre
        puts(InfoFichero(corto, dir, ent->d_name, enlace, acc));
    }

    /* 3. En -reca se visitan los subdirectorios después del listado. */
    if (reca)
    {
        rewinddir(d);
        while ((ent = readdir(d)) != NULL)
        {
            if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
                continue;
            if (!hid && ent->d_name[0] == '.')
                continue;

            snprintf(ruta, sizeof(ruta), "%s/%s", dir, ent->d_name);
            if (lstat(ruta, &st) != -1 && S_ISDIR(st.st_mode))
            {
                ProcesarDirectorio(ruta, corto, enlace, acc, hid, reca, recb);
            }
        }
    }

    closedir(d);
}

/*********************************************/
/*************COMANDOS DEL SHELL************************/
void Cmd_fin(char *arg[], tShellLists *context) /*todos los cmd_ comparten prototipo*/
{
    (void)arg;
    deleteList(&context->abiertos);
    exit(0);
}

void Cmd_autores(char *arg[], tShellLists *context)
{
    (void)context;
    if (arg[0] == NULL)
    {
        printf("Sergio López Fontenla: sergio.lopez.fontenla@udc.es\n");
        printf("Lucas Pastoriza Rosales: lucas.pastoriza@udc.es\n");
    }
    else if (!strcmp(arg[0], "-l"))
    {
        printf("sergio.lopez.fontenla@udc.es\n");
        printf("lucas.pastoriza@udc.es\n");
    }
    else if (!strcmp(arg[0], "-n"))
    {
        printf("Sergio López Fontenla \n");
        printf("Lucas Pastoriza Rosales\n");
    }
}

void Cmd_exec(char *arg[], tShellLists *context)
{
    (void)context;
    if (execvp(arg[0], arg) == -1)
        perror("Imposible ejecutar");
}

void Cmd_splano(char *arg[], tShellLists *context)
{
    (void)context;
    Proceso(arg, 1);
}
void Cmd_pplano(char *arg[], tShellLists *context) /*  */
{
    (void)context;
    Proceso(arg, 0);
}

void Cmd_chdir(char *arg[], tShellLists *context)
{
    (void)context;
    if (arg[0] == NULL)
        MostrarDirActual();
    else if (chdir(arg[0]) == -1)
        perror("Imposible cambiar directorio");
}

void Cmd_pwd(char *arg[], tShellLists *context)
{
    (void)context;
    (void)arg;
    MostrarDirActual();
}

void Cmd_pid(char *arg[], tShellLists *context)
{
    (void)context;
    if (arg[0] != NULL && !strcmp(arg[0], "-p"))
        printf("El pid del proceso padre es %d\n", (int)getppid());
    else
        printf("El pid del proceso es %d\n", (int)getpid());
}

void Cmd_date(char *arg[], tShellLists *context)
{
    (void)context;
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);

    if (arg[0] == NULL || !strcmp(arg[0], "-d"))
        printf("%02d/%02d/%d\n", lt->tm_mday, lt->tm_mon + 1, lt->tm_year + 1900);

    if (arg[0] == NULL || !strcmp(arg[0], "-t"))
        printf("%02d:%02d:%02d\n", lt->tm_hour, lt->tm_min, lt->tm_sec);
}

void Cmd_sysinfo(char *arg[], tShellLists *context)
{
    (void)context;
    (void)arg;
    struct utsname info;

    if (uname(&info) == -1)
    {
        perror("Imposible obtener informacion del sistema");
        return;
    }

    printf("%s %s %s %s %s\n",
           info.sysname,  /* nombre del sistema operativo, p.ej. "Linux" */
           info.nodename, /* nombre de la maquina en la red */
           info.release,  /* version del kernel, p.ej. "6.8.0-45-generic" */
           info.version,  /* fecha/detalles de compilacion del kernel */
           info.machine); /* arquitectura, p.ej. "x86_64" */
}

void Cmd_makefile(char *arg[], tShellLists *context)
{
    (void)context;
    int fd;

    if (arg[0] == NULL)
    {
        printf("uso: makefile nombre\n");
        return;
    }

    fd = open(arg[0], O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd == -1)
    {
        printf("Imposible crear %s: %s\n", arg[0], strerror(errno));
        return;
    }
    close(fd);
}

void Cmd_makedir(char *arg[], tShellLists *context)
{
    (void)context;
    if (arg[0] == NULL)
    {
        printf("uso: makedir nombre\n");
        return;
    }

    if (mkdir(arg[0], 0755) == -1)
        printf("Imposible crear %s: %s\n", arg[0], strerror(errno));
}

void Cmd_listopen(char *arg[], tShellLists *context)
{
    (void)arg;
    tPosL p;

    for (p = first(context->abiertos); p != LNULL; p = next(p, context->abiertos))
    {
        printf("descriptor: %d, offset: (  )-> %s %s\n",
               p->data.df, p->data.nombre, ModoAString(p->data.modo));
    }
}

void Cmd_dup(char *arg[], tShellLists *context)
{
    char nombre[MAXNOMBRE];
    const char *orig;
    int df, nuevo, modo;
    tPosL p;

    if (arg[0] == NULL)
    {
        Cmd_listopen(arg, context);
        return;
    }

    df = atoi(arg[0]);

    if ((nuevo = dup(df)) == -1)
    {
        perror("Imposible duplicar descriptor");
        return;
    }

    p = findItem(df, context->abiertos);
    orig = (p != LNULL) ? p->data.nombre : "?";
    snprintf(nombre, sizeof(nombre), "duplicado de %d (%.*s)",
             df, (int)sizeof(nombre) - 32, orig);

    modo = fcntl(nuevo, F_GETFL);
    if (modo == -1 || AnadirAbierto(nuevo, nombre, modo, &context->abiertos) == -1)
        close(nuevo);
}

void Cmd_open(char *arg[], tShellLists *context)
{
    int i, modo = 0, df;

    if (arg[0] == NULL)
    { /* sin argumentos: lista los abiertos */
        Cmd_listopen(arg, context);
        return;
    }

    for (i = 1; arg[i] != NULL; i++)
    {
        if (!strcmp(arg[i], "cr"))
            modo |= O_CREAT;
        else if (!strcmp(arg[i], "ap"))
            modo |= O_APPEND;
        else if (!strcmp(arg[i], "ex"))
            modo |= O_EXCL;
        else if (!strcmp(arg[i], "ro"))
            modo |= O_RDONLY;
        else if (!strcmp(arg[i], "rw"))
            modo |= O_RDWR;
        else if (!strcmp(arg[i], "wo"))
            modo |= O_WRONLY;
        else if (!strcmp(arg[i], "tr"))
            modo |= O_TRUNC;
        else
            break;
    }

    if ((df = open(arg[0], modo, 0644)) == -1)
    {
        perror("Imposible abrir fichero");
        return;
    }
    if (AnadirAbierto(df, arg[0], modo, &context->abiertos) == -1)
    {
        close(df); /* si no cabe en la lista, no lo dejamos abierto */
        return;
    }
    printf("Anadida entrada %d a la tabla ficheros abiertos\n", df);
}

void Cmd_close(char *arg[], tShellLists *context)
{
    char *fin;
    int df;
    long v = -1;

    if (arg[0] != NULL)
    {
        v = strtol(arg[0], &fin, 10);
        if (*fin != '\0')
            v = -1; /* no es un numero */
    }
    if (v < 0)
    { /* sin parametro o invalido: lista los abiertos */
        Cmd_listopen(arg, context);
        return;
    }
    df = (int)v;

    if (close(df) == -1)
        perror("Imposible cerrar descriptor");
    else
        EliminarAbierto(df, &context->abiertos); /* solo si el sistema lo cerro de verdad */
}

void Cmd_help(char *arg[], tShellLists *context)
{
    (void)context;
    if (arg[0] == NULL)
    {
        printf("Comandos disponibles:\n");
        printf("  autores pid pwd chdir date sysinfo help\n");
        printf("  makefile makedir open close listopen dup lseek readstr writestr\n");
        printf("  delete deltree listfile list exec pplano splano\n");
        printf("  exit bye fin quit\n");
        return;
    }

    if (!strcmp(arg[0], "exit") || !strcmp(arg[0], "bye") || !strcmp(arg[0], "fin") || !strcmp(arg[0], "quit"))
        printf("%s: Termina la ejecucion del shell.\n", arg[0]);
    else if (!strcmp(arg[0], "help"))
        printf("help [cmd]: Muestra una lista de comandos disponibles, o ayuda especifica para el comando 'cmd'.\n");
    else if (!strcmp(arg[0], "authors"))
        printf("authors [-n|-l]: Muestra los nombres y/o logins de los autores del shell.\n");
    else if (!strcmp(arg[0], "pid"))
        printf("pid [-p]: Muestra el PID del shell. Con -p muestra el del proceso padre.\n");
    else if (!strcmp(arg[0], "pwd"))
        printf("pwd: Muestra el directorio actual de trabajo.\n");
    else if (!strcmp(arg[0], "chdir"))
        printf("chdir [dir]: Cambia el directorio actual a 'dir' o lo muestra si no hay argumentos.\n");
    else if (!strcmp(arg[0], "date"))
        printf("date [-d|-t]: Muestra la fecha (-d) y/o la hora (-t) actual.\n");
    else if (!strcmp(arg[0], "sysinfo"))
        printf("sysinfo: Muestra informacion del sistema (similar a uname -a).\n");
    else if (!strcmp(arg[0], "makefile"))
        printf("makefile [name]: Crea un fichero vacio con nombre 'name'.\n");
    else if (!strcmp(arg[0], "makedir"))
        printf("makedir [name]: Crea un directorio con nombre 'name'.\n");
    else if (!strcmp(arg[0], "open"))
        printf("open [file] [m1] [m2]...: Abre un fichero y lo anade a la lista de ficheros abiertos.\n  Modos: cr (O_CREAT), ap (O_APPEND), ex (O_EXCL), ro (O_RDONLY), rw (O_RDWR), wo (O_WRONLY), tr (O_TRUNC).\n");
    else if (!strcmp(arg[0], "close"))
        printf("close [df]: Cierra el descriptor de fichero 'df' y lo elimina de la lista.\n");
    else if (!strcmp(arg[0], "listopen"))
        printf("listopen: Lista los ficheros abiertos por el shell.\n");
    else if (!strcmp(arg[0], "dup"))
        printf("dup [df]: Duplica el descriptor de fichero 'df' y lo anade a la lista.\n");
    else if (!strcmp(arg[0], "lseek"))
        printf("lseek [df] [pos] [ref]: Cambia el offset del descriptor 'df' a 'pos'. ref puede ser SEEK_SET, SEEK_CUR o SEEK_END.\n");
    else if (!strcmp(arg[0], "readstr"))
        printf("readstr [df] [cont]: Lee 'cont' bytes del descriptor 'df' y los muestra por pantalla.\n");
    else if (!strcmp(arg[0], "writestr"))
        printf("writestr [df] [str]: Escribe la cadena 'str' en el descriptor 'df'.\n");
    else if (!strcmp(arg[0], "delete") || !strcmp(arg[0], "deltree") || !strcmp(arg[0], "listfile") || !strcmp(arg[0], "list"))
        printf("%s: Comando relacionado con la gestion de ficheros y directorios.\n", arg[0]);
    else if (!strcmp(arg[0], "exec") || !strcmp(arg[0], "pplano") || !strcmp(arg[0], "splano"))
        printf("%s: Ejecuta un programa externo.\n", arg[0]);
    else
        printf("Comando no encontrado. Escribe 'help' para ver la lista de comandos.\n");
}

void Cmd_readstr(char *arg[], tShellLists *context)
{
    (void)context;
    char *fin, *buf;
    long df, cont;
    ssize_t n;

    if (arg[0] == NULL || arg[1] == NULL)
    {
        printf("Parametros incorrectos\n");
        return;
    }
    df = strtol(arg[0], &fin, 10);
    if (*fin != '\0' || df < 0)
    {
        printf("Parametros incorrectos\n");
        return;
    }
    cont = strtol(arg[1], &fin, 10);
    if (*fin != '\0' || cont <= 0)
    {
        printf("Parametros incorrectos\n");
        return;
    }

    if ((buf = malloc((size_t)cont + 1)) == NULL)
    {
        printf("Imposible asignar memoria para la lectura\n");
        return;
    }
    if ((n = read((int)df, buf, (size_t)cont)) == -1)
    {
        printf("Error al intentar leer del descriptor %ld: %s\n", df, strerror(errno));
    }
    else
    {
        buf[n] = '\0'; /* para tratarlo como cadena */
        printf("%s\n", buf);
        printf("Leidos %zd bytes del descriptor %ld\n", n, df);
    }
    free(buf);
}

void Cmd_writestr(char *arg[], tShellLists *context)
{
    (void)context;
    char *fin;
    long df;
    size_t len;

    if (arg[0] == NULL || arg[1] == NULL)
    {
        printf("Parametros incorrectos\n");
        return;
    }
    df = strtol(arg[0], &fin, 10);
    if (*fin != '\0' || df < 0)
    {
        printf("Parametros incorrectos\n");
        return;
    }

    len = strlen(arg[1]); /* sin el '\0' final */
    if (write((int)df, arg[1], len) == -1)
        printf("Error intentar escribir %zu bytes en el descriptor %ld: %s\n",
               len, df, strerror(errno));
}

void Cmd_lseek(char *arg[], tShellLists *context)
{
    (void)context;
    char *fin;
    long df;
    long long off;
    off_t res;
    int ref;

    if (arg[0] == NULL || arg[1] == NULL || arg[2] == NULL)
    {
        printf("Parametros incorrectos\n");
        return;
    }
    df = strtol(arg[0], &fin, 10);
    if (*fin != '\0' || df < 0)
    {
        printf("Parametros incorrectos\n");
        return;
    }
    errno = 0;
    off = strtoll(arg[1], &fin, 10);
    if (*fin != '\0' || errno == ERANGE)
    {
        printf("Parametros incorrectos\n");
        return;
    }

    if (!strcmp(arg[2], "SEEK_SET"))
        ref = SEEK_SET;
    else if (!strcmp(arg[2], "SEEK_CUR"))
        ref = SEEK_CUR;
    else if (!strcmp(arg[2], "SEEK_END"))
        ref = SEEK_END;
    else
    {
        printf("Parametros incorrectos\n");
        return;
    }

    if ((res = lseek((int)df, (off_t)off, ref)) == (off_t)-1)
        printf("Error al intentar posicionar el descriptor %ld en el offset  %lld: %s\n",
               df, off, strerror(errno));
    else
        printf("Descriptor %ld posicionado en %lld\n", df, (long long)res);
}

void Cmd_delete(char *arg[], tShellLists *context)
{
    (void)context;
    int i, r;
    struct stat st;

    if (arg[0] == NULL)
    { /* sin nombres no hay nada que borrar */
        MostrarDirActual();
        return;
    }

    for (i = 0; arg[i] != NULL; i++)
    {
        if (lstat(arg[i], &st) == -1)
            r = -1;
        else if (S_ISDIR(st.st_mode))
            r = rmdir(arg[i]); /* directorio: solo si esta vacio */
        else
            r = unlink(arg[i]); /* fichero, enlace, etc. */

        if (r == -1)
            printf("Imposible borrar %s: %s\n", arg[i], strerror(errno));
    }
}

void Cmd_deltree(char *arg[], tShellLists *context)
{
    (void)context;
    int i;
    size_t len;
    const char *base;
    char nombre[MAXNOMBRE];

    if (arg[0] == NULL)
    { /* sin nombres: muestra el directorio actual */
        MostrarDirActual();
        return;
    }

    for (i = 0; arg[i] != NULL; i++)
    {
        snprintf(nombre, sizeof(nombre), "%s", arg[i]);
        len = strlen(nombre);
        while (len > 1 && nombre[len - 1] == '/') // "enlace/" seguiria el enlace se quita la barra
            nombre[--len] = '\0';

        base = strrchr(nombre, '/');
        base = (base != NULL && base[1] != '\0') ? base + 1 : nombre;
        if (!strcmp(nombre, "/") || !strcmp(base, ".") || !strcmp(base, ".."))
        {
            printf("Imposible borrar %s: %s\n", arg[i], strerror(EINVAL));
            continue;
        }
        BorrarArbol(nombre);
    }
}

void Cmd_listfile(char *arg[], tShellLists *context)
{
    (void)context;
    int i, corto = 1, enlace = 0, acc = 0;

    for (i = 0; arg[i] != NULL; i++)
    {
        if (!strcmp(arg[i], "-long"))
            corto = 0;
        else if (!strcmp(arg[i], "-link"))
            enlace = 1;
        else if (!strcmp(arg[i], "-acc"))
            acc = 1;
        else
            break;
    }

    if (arg[i] == NULL)
    { // sin nombres: muestra el directorio actual
        MostrarDirActual();
        return;
    }
    for (; arg[i] != NULL; i++)
        puts(InfoFichero(corto, NULL, arg[i], enlace, acc));
}

void Cmd_list(char *arg[], tShellLists *context)
{
    (void)context;
    int i, corto = 1, enlace = 0, acc = 0, hid = 0, reca = 0, recb = 0;
    struct stat st;

    // Procesamos todos los flags posibles
    for (i = 0; arg[i] != NULL; i++)
    {
        if (!strcmp(arg[i], "-long"))
            corto = 0;
        else if (!strcmp(arg[i], "-link"))
            enlace = 1;
        else if (!strcmp(arg[i], "-acc"))
            acc = 1;
        else if (!strcmp(arg[i], "-hid"))
            hid = 1;
        else if (!strcmp(arg[i], "-reca"))
            reca = 1;
        else if (!strcmp(arg[i], "-recb"))
            recb = 1;
        else
            break; // En cuanto haya un argumento que no sea flag, paramos
    }

    if (arg[i] == NULL)
    { // Sin argumentos: muestra el directorio actual
        MostrarDirActual();
        return;
    }

    // Iteramos sobre los nombres (ficheros o directorios)
    for (; arg[i] != NULL; i++)
    {
        if (lstat(arg[i], &st) == -1)
        { // lstat para evitar problemas con enlaces simbólicos
            printf(" ****error al acceder a %s: %s\n", arg[i], strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            // Si es un directorio, llamamos a la recursiva para listar su contenido
            ProcesarDirectorio(arg[i], corto, enlace, acc, hid, reca, recb);
        }
        else
        {
            // Si es un fichero normal, se imprime como listfile
            puts(InfoFichero(corto, NULL, arg[i], enlace, acc));
        }
    }
}

void Cmd_allocate(char *arg[], tShellLists *context)
{
    // Declaramos aquí las variables que pueden necesitar los distintos modos.
    char *fin;
    long tam, claveNumero;
    int id, proteccion, modo, fd;
    struct shmid_ds datos;
    struct stat st;
    void *direccion;
    size_t i;
    tItemM bloque;

    if (arg[0] == NULL)
    {
        // Sin opciones, mostramos los bloques que ya reservó allocate.
        printf("******Lista de bloques asignados para el proceso %d\n", (int)getpid());
        printListM(context->memoria, T_TODOS);
    }
    else
    {
        if (!strcmp(arg[0], "-malloc"))
        {
            // Para malloc necesitamos exactamente el tamaño que se quiere reservar.
            if (arg[1] == NULL || arg[2] != NULL)
            {
                printf("Uso: allocate -malloc tamaño\n");
                return;
            }

            // strtol convierte el texto a número y deja en fin lo que no pudo convertir.
            errno = 0;
            tam = strtol(arg[1], &fin, 10);
            // Rechazamos texto inválido, tamaños no positivos y números demasiado grandes.
            if (errno == ERANGE || fin == arg[1] || *fin != '\0' || tam <= 0)
            {
                printf("Tamaño no válido: %s\n", arg[1]);
                return;
            }

            // Reservamos la memoria y preparamos los datos que guardaremos sobre el bloque.
            bloque = (tItemM){0};
            bloque.dir = malloc((size_t)tam);
            if (bloque.dir == NULL)
            {
                perror("malloc");
                return;
            }

            bloque.tam = (size_t)tam;
            bloque.hora = time(NULL);
            bloque.tipo = T_MALLOC;

            // Si no se puede añadir a la lista, liberamos la reserva para no dejarla perdida.
            if (!insertItemM(bloque, MNULL, &context->memoria))
            {
                free(bloque.dir);
                printf("No se pudo guardar el bloque en la lista\n");
                return;
            }

            // Mostramos la misma información básica que presenta la shell de referencia.
            printf("Asignados %zu bytes en %p\n", bloque.tam, bloque.dir);
        }
        else if (!strcmp(arg[0], "-createshared"))
        {
            if (arg[1] == NULL || arg[2] == NULL || arg[3] != NULL)
            {
                printf("Uso: allocate -createshared clave tamaño\n");
                return;
            }

            bloque = (tItemM){0};

            // Convertimos la clave y el tamaño que escribió el usuario.
            errno = 0;
            claveNumero = strtol(arg[1], &fin, 0);
            if (errno == ERANGE || fin == arg[1] || *fin != '\0')
            {
                printf("Clave no válida: %s\n", arg[1]);
                return;
            }
            errno = 0;
            tam = strtol(arg[2], &fin, 10);
            if (errno == ERANGE || fin == arg[2] || *fin != '\0' || tam <= 0)
            {
                printf("Tamaño no válido: %s\n", arg[2]);
                return;
            }

            // shmget crea un segmento nuevo; IPC_EXCL hace que falle si la clave ya existe.
            id = shmget((key_t)claveNumero, (size_t)tam, IPC_CREAT | IPC_EXCL | 0777);
            if (id == -1)
            {
                perror("shmget");
                return;
            }
            // shmat conecta el segmento al proceso y devuelve su dirección.
            bloque.dir = shmat(id, NULL, 0);
            if (bloque.dir == (void *)-1)
            {
                perror("shmat");
                // IPC_RMID elimina el segmento nuevo que no se pudo conectar.
                shmctl(id, IPC_RMID, NULL);
                return;
            }

            bloque.tam = (size_t)tam;
            bloque.hora = time(NULL);
            bloque.tipo = T_SHARED;
            bloque.clave = (key_t)claveNumero;

            // Guardamos la conexión para poder desconectarla más adelante.
            if (!insertItemM(bloque, MNULL, &context->memoria))
            {
                // Quitamos la conexión y eliminamos el segmento que acabamos de crear.
                shmdt(bloque.dir);
                shmctl(id, IPC_RMID, NULL);
                printf("No se pudo guardar el bloque en la lista\n");
                return;
            }
            printf("Asignados %zu bytes en %p\n", bloque.tam, bloque.dir);
        }
        else if (!strcmp(arg[0], "-shared"))
        {
            if (arg[1] == NULL || arg[2] != NULL)
            {
                printf("Uso: allocate -shared clave\n");
                return;
            }

            bloque = (tItemM){0};

            // Convertimos la clave para localizar el segmento que ya existe.
            errno = 0;
            claveNumero = strtol(arg[1], &fin, 0);
            if (errno == ERANGE || fin == arg[1] || *fin != '\0')
            {
                printf("Clave no válida: %s\n", arg[1]);
                return;
            }
            // Con tamaño 0 y sin IPC_CREAT, shmget busca un segmento que ya exista.
            id = shmget((key_t)claveNumero, 0, 0777);
            if (id == -1)
            {
                perror("shmget");
                return;
            }
            // shmctl con IPC_STAT consulta los datos del segmento, incluido su tamaño.
            if (shmctl(id, IPC_STAT, &datos) == -1)
            {
                perror("shmctl");
                return;
            }
            // shmat conecta este proceso al segmento encontrado.
            bloque.dir = shmat(id, NULL, 0);
            if (bloque.dir == (void *)-1)
            {
                perror("shmat");
                return;
            }

            bloque.tam = datos.shm_segsz;
            bloque.hora = time(NULL);
            bloque.tipo = T_SHARED;
            bloque.clave = (key_t)claveNumero;

            // Registramos también los segmentos existentes a los que nos conectamos.
            if (!insertItemM(bloque, MNULL, &context->memoria))
            {
                // shmdt desconecta este proceso; el segmento compartido sigue existiendo.
                shmdt(bloque.dir);
                printf("No se pudo guardar el bloque en la lista\n");
                return;
            }
            printf("Memoria compartida de clave %d  en %p\n", (int)bloque.clave, bloque.dir);
        }
        else if (!strcmp(arg[0], "-mmap"))
        {
            if (arg[1] == NULL || arg[2] == NULL || arg[3] != NULL)
            {
                printf("Uso: allocate -mmap fichero permisos\n");
                return;
            }

            proteccion = 0;
            bloque = (tItemM){0};

            // Traducimos r, w y x a los permisos que entiende mmap.
            for (i = 0; arg[2][i] != '\0'; i++)
            {
                if (arg[2][i] == 'r')
                    proteccion |= PROT_READ;
                else if (arg[2][i] == 'w')
                    proteccion |= PROT_WRITE;
                else if (arg[2][i] == 'x')
                    proteccion |= PROT_EXEC;
                else
                {
                    printf("Permisos no válidos: %s\n", arg[2]);
                    return;
                }
            }

            // Abrimos con permiso de escritura solo si se pidió w.
            modo = (proteccion & PROT_WRITE) ? O_RDWR : O_RDONLY;
            // open devuelve un descriptor que identifica el archivo abierto.
            fd = open(arg[1], modo);
            if (fd == -1)
            {
                perror(arg[1]);
                return;
            }
            // fstat obtiene el tamaño del archivo usando el descriptor abierto.
            if (fstat(fd, &st) == -1)
            {
                perror("fstat");
                close(fd);
                return;
            }
            if (st.st_size <= 0)
            {
                printf("El fichero está vacío\n");
                close(fd);
                return;
            }

            // Mapeamos el contenido del archivo desde el principio.
            // mmap coloca el contenido del archivo dentro del espacio de memoria del proceso.
            direccion = mmap(NULL, (size_t)st.st_size, proteccion, MAP_PRIVATE, fd, 0);
            if (direccion == MAP_FAILED)
            {
                perror("mmap");
                close(fd);
                return;
            }

            bloque.dir = direccion;
            bloque.tam = (size_t)st.st_size;
            bloque.hora = time(NULL);
            bloque.tipo = T_MMAP;
            bloque.df = fd;
            strncpy(bloque.fichero, arg[1], MAXNOMBRE - 1);
            bloque.fichero[MAXNOMBRE - 1] = '\0';

            // Conservamos el descriptor abierto para poder cerrar el mapeo después.
            if (!insertItemM(bloque, MNULL, &context->memoria))
            {
                munmap(direccion, bloque.tam);
                close(fd);
                printf("No se pudo guardar el bloque en la lista\n");
                return;
            }
            printf("fichero %s mapeado en %p\n", bloque.fichero, bloque.dir);
        }
        else
        {
            printf("Uso: allocate [-malloc|-createshared|-shared|-mmap] ...\n");
        }
    }
}

void Cmd_deallocate(char *arg[], tShellLists *context)
{
    // Estas variables sirven para identificar el bloque que se quiere liberar.
    char *fin;
    long valor;
    key_t clave;
    int id;
    void *direccion;
    tPosM pos;
    tItemM bloque;

    if (arg[0] == NULL)
    {
        // Sin opciones, mostramos la lista de bloques asignados.
        printf("******Lista de bloques asignados para el proceso %d\n", (int)getpid());
        printListM(context->memoria, T_TODOS);
        return;
    }

    if (!strcmp(arg[0], "-malloc"))
    {
        if (arg[1] == NULL || arg[2] != NULL)
        {
            printf("Uso: deallocate -malloc tamaño\n");
            return;
        }

        // Convertimos el tamaño y comprobamos que sea válido.
        errno = 0;
        valor = strtol(arg[1], &fin, 10);
        if (errno == ERANGE || fin == arg[1] || *fin != '\0' || valor <= 0)
        {
            printf("Tamaño no válido: %s\n", arg[1]);
            return;
        }

        pos = findBySizeM((size_t)valor, context->memoria);
        if (pos == MNULL)
        {
            printf("No hay bloque de ese tamano asignado con malloc\n");
            return;
        }

        bloque = getItemM(pos, context->memoria);
        // free devuelve al gestor el bloque reservado con malloc.
        free(bloque.dir);
        deleteAtPositionM(pos, &context->memoria);
    }
    else if (!strcmp(arg[0], "-shared"))
    {
        if (arg[1] == NULL || arg[2] != NULL)
        {
            printf("Uso: deallocate -shared clave\n");
            return;
        }

        // Convertimos la clave para buscar el segmento conectado por este proceso.
        errno = 0;
        valor = strtol(arg[1], &fin, 0);
        if (errno == ERANGE || fin == arg[1] || *fin != '\0')
        {
            printf("Clave no válida: %s\n", arg[1]);
            return;
        }
        clave = (key_t)valor;
        pos = findByKeyM(clave, context->memoria);
        if (pos == MNULL)
        {
            printf("No hay bloque de esa clave mapeado en el proceso\n");
            return;
        }

        bloque = getItemM(pos, context->memoria);
        // shmdt desconecta este proceso, pero no elimina el segmento del sistema.
        if (shmdt(bloque.dir) == -1)
        {
            perror("shmdt");
            return;
        }
        deleteAtPositionM(pos, &context->memoria);
    }
    else if (!strcmp(arg[0], "-delkey"))
    {
        if (arg[1] == NULL || arg[2] != NULL)
        {
            printf("Uso: deallocate -delkey clave\n");
            return;
        }

        // Esta opción solo elimina la clave del sistema; no desconecta el segmento.
        errno = 0;
        valor = strtol(arg[1], &fin, 0);
        if (errno == ERANGE || fin == arg[1] || *fin != '\0' || (key_t)valor == IPC_PRIVATE)
        {
            printf("      delkey necesita clave_valida\n");
            return;
        }
        clave = (key_t)valor;
        id = shmget(clave, 0, 0666);
        if (id == -1)
        {
            perror("shmget: imposible obtener memoria compartida");
            return;
        }
        // IPC_RMID marca el segmento para eliminarlo cuando ya no esté conectado.
        if (shmctl(id, IPC_RMID, NULL) == -1)
            perror("shmctl: imposible eliminar memoria compartida");
    }
    else if (!strcmp(arg[0], "-mmap"))
    {
        if (arg[1] == NULL || arg[2] != NULL)
        {
            printf("Uso: deallocate -mmap fichero\n");
            return;
        }

        pos = findByFileM(arg[1], context->memoria);
        if (pos == MNULL)
        {
            printf("Fichero %s no mapeado\n", arg[1]);
            return;
        }

        bloque = getItemM(pos, context->memoria);
        // munmap quita el archivo mapeado del espacio de memoria del proceso.
        if (munmap(bloque.dir, bloque.tam) == -1)
        {
            perror("munmap");
            return;
        }
        close(bloque.df);
        deleteAtPositionM(pos, &context->memoria);
    }
    else if (arg[1] == NULL)
    {
        // Si no hay una opción, interpretamos el argumento como una dirección.
        if (sscanf(arg[0], "%p", &direccion) != 1)
        {
            printf("Direccion %s no asignada con malloc, shared o mmap\n", arg[0]);
            return;
        }

        pos = findByAddrM(direccion, context->memoria);
        if (pos == MNULL)
        {
            printf("Direccion %p no asignada con malloc, shared o mmap\n", direccion);
            return;
        }

        bloque = getItemM(pos, context->memoria);
        if (bloque.tipo == T_MALLOC)
        {
            // free libera el bloque malloc encontrado por su dirección.
            free(bloque.dir);
        }
        else if (bloque.tipo == T_SHARED)
        {
            // shmdt desconecta este proceso del segmento compartido.
            if (shmdt(bloque.dir) == -1)
            {
                perror("shmdt");
                return;
            }
        }
        else if (bloque.tipo == T_MMAP)
        {
            // munmap deshace el mapeo y después cerramos el archivo asociado.
            if (munmap(bloque.dir, bloque.tam) == -1)
            {
                perror("munmap");
                return;
            }
            close(bloque.df);
        }
        deleteAtPositionM(pos, &context->memoria);
    }
    else
    {
        printf("Uso: deallocate [-malloc|-shared|-delkey|-mmap|addr] ...\n");
    }
}

/**************************SHELL**************************/

static struct COMANDO C[] = {
    /*no declaro dimension, que la coge de la inicializacion*/
    {"fin", Cmd_fin},
    {"bye", Cmd_fin},
    {"exit", Cmd_fin},
    {"quit", Cmd_fin},
    {"pid", Cmd_pid},
    {"pwd", Cmd_pwd},
    {"chdir", Cmd_chdir},
    {"authors", Cmd_autores},
    {"exec", Cmd_exec},
    {"date", Cmd_date},
    {"pplano", Cmd_pplano},
    {"splano", Cmd_splano},
    {"sysinfo", Cmd_sysinfo},
    {"makefile", Cmd_makefile},
    {"makedir", Cmd_makedir},
    {"open", Cmd_open},
    {"listopen", Cmd_listopen},
    {"dup", Cmd_dup},
    {"close", Cmd_close},
    {"lseek", Cmd_lseek},
    {"readstr", Cmd_readstr},
    {"writestr", Cmd_writestr},
    {"help", Cmd_help},
    {"delete", Cmd_delete},
    {"deltree", Cmd_deltree},
    {"listfile", Cmd_listfile},
    {"list", Cmd_list},
    {"allocate", Cmd_allocate},
    {"deallocate", Cmd_deallocate},
    {NULL, NULL}, /*NULL marca el final del array*/
};

void DecidirComando(char *tr[], tShellLists *context)
{
    int i;

    if (tr[0] == NULL)
        return;                           /*por si luego quito lode TrocearCadena ==0*/
    for (i = 0; C[i].nombre != NULL; i++) /*recorro el array hasta el NULL*/
        if (!strcmp(C[i].nombre, tr[0]))
        {
            (*C[i].funcion)(tr + 1, context);
            return; /*si es algun cmd_ ya no miro mas*/
        }
    Proceso(tr, 0); /*si no es un comando, es un  ejecutable externo*/
}

int TrocearCadena(char *cadena, char *trozos[])
{
    int i = 1;
    if ((trozos[0] = strtok(cadena, " \n\t")) == NULL)
        return 0;
    while ((trozos[i] = strtok(NULL, " \n\t")) != NULL)
        i++;
    return i;
}
void ProcesarEntrada(char *entrada, tShellLists *context)
{
    char *tr[MAXENTRADA / 2];
    if (TrocearCadena(entrada, tr) == 0) /*no hay nada*/
        return;
    DecidirComando(tr, context);
}

int main(void)
{
    char entrada[MAXENTRADA];
    tShellLists context;

    InicializarLista(&context.abiertos);
    createEmptyListM(&context.memoria);

    while (1)
    {
        printf("-> ");
        if (fgets(entrada, MAXENTRADA, stdin) == NULL)
        {
            printf("\n");
            deleteList(&context.abiertos);
            deleteListM(&context.memoria);
            break;
        }
        ProcesarEntrada(entrada, &context);
    }
    return 0;
}
