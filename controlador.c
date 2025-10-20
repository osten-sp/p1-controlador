/**
 * @file controlador.c
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

// Definición de constantes
#define ANTERIOR 0
#define VIVO     1
#define MUERTO   2

// Cabeceras de funciones
static void uso(void);
static void convertir(const char *fich_imagen, const char *dir_resultados);
void        manejador_signals(int sig);
void        crearTablaHijos(void);
void        imprimirTablaHijos(void);
void        anotarHijoMuerto(int pid_hijo);
void        matarHijos(void);
int         buscarPosicion(void);
void        anotarHijoVivo(int pid_hijo);
void        anotarPidEnTabla(int i, int pid_hijo);

// Variables globales
int terminar;
int max_Hijos;

// Estructuras
typedef struct
{
    int   stat; // 0: Anterior, 1: Vivo, 2: Muerto
    pid_t pid_hijo;
} hijo;

hijo *tablahijos;

/**
 * @brief Función principal del servidor
 *
 * @param argc Número de argumentos de la línea de comandos
 * @param argv Argumentos de la línea de comandos
 *
 * @return Código de salida del programa
 */
int main(int argc, char **argv)
{
    const char *dir_resultados;
    pid_t       pid_hijo;
    int         pid_hijo_muerto;
    int         hijos;
    int         posicion;
    int         i;

    i               = 3;
    terminar        = 0;
    pid_hijo_muerto = 0;
    hijos           = 0;
    max_Hijos       = atoi(argv[1]);

    signal(SIGTERM, manejador_signals);

    if (argc < 4)
    {
        uso();
        exit(EX_USAGE);
    }

    dir_resultados = argv[2];

    crearTablaHijos();
    imprimirTablaHijos();

    while ((i < argc) && (!terminar))
    {
        switch (pid_hijo = fork())
        {
        case -1: perror("Error al intentar crear hijos\n"); break;

        case 0: // caso del hijo, no tiene break
            signal(SIGTERM, SIG_DFL);
            convertir(argv[i], dir_resultados);
            exit(0);

        default:

            posicion = buscarPosicion();
            if (posicion != -1)
            {
                printf("se ha creado el hijo numero %d, con PID %d \n\n", posicion, pid_hijo);

                anotarPidEnTabla(posicion, pid_hijo);
                anotarHijoVivo(pid_hijo);
                hijos++;
                i++;
            }

            if (hijos == max_Hijos && terminar == 0)
            {
                // Espera a que un hijo muera
                do {
                    pid_hijo_muerto = wait(NULL);
                } while ((pid_hijo_muerto == -1) && (errno = EINTR));
                hijos--;
                anotarHijoMuerto(pid_hijo_muerto);
                printf("hijo muerto con PID %d \n\n", pid_hijo_muerto);
                imprimirTablaHijos();
            }

        } // switch

    } // while

    if (terminar == 1) { matarHijos(); }
    else
    {
        while (hijos > 0)
        {
            do {
                pid_hijo_muerto = wait(NULL);
            } while ((pid_hijo_muerto == -1) && (errno = EINTR));
            hijos--;
            anotarHijoMuerto(pid_hijo_muerto);

            printf("hijo muerto con PID %d \n\n", pid_hijo_muerto);
            imprimirTablaHijos();
        }
    }

    free(tablahijos);
    exit(EX_OK);
} // main

/**
 * @brief Crea una tabla en memoria compartida para almacenar los hijos.
 */
void crearTablaHijos(void)
{
    if ((tablahijos = (hijo *)calloc((size_t)max_Hijos, sizeof(hijo))) == NULL)
        perror("Error al crear la tabla de hijos");

    for (int j = 0; j < max_Hijos; j++)
    {
        tablahijos[j].stat = ANTERIOR; // antes de nacer
    }
}

/**
 * @brief Imprime por pantalla la tabla de hijos.
 */
void imprimirTablaHijos(void)
{
    printf("Soy el padre con PID %d\n", getpid());
    for (int z = 0; z < max_Hijos; z++)
    {
        printf("hijo numero %d estado %d con pid %d\n", z, tablahijos[z].stat, tablahijos[z].pid_hijo);
    }
}

/**
 * @brief Busca una posición vacía en la tabla para insertar un hijo.
 * @param pid_hijo PID del hijo a insertar
 *
 * @return int
 */
int buscarPosicion(void)
{
    for (int k = 0; k < max_Hijos; k++)
    {
        if ((tablahijos[k].stat == ANTERIOR) || (tablahijos[k].stat == MUERTO)) return k;
    }
    return -1;
}

/**
 * @brief Anota en la tabla la muerte de un hijo.
 * @param pid_hijo PID del hijo
 */
void anotarHijoMuerto(int pid_hijo)
{
    for (int h = 0; h < max_Hijos; h++)
    {
        if (tablahijos[h].pid_hijo == pid_hijo) { tablahijos[h].stat = MUERTO; }
    }
}

/**
 * @brief Anota en la tabla la creación de un hijo.
 * @param posicion Posición en la tabla
 * @param pid_hijo PID del hijo
 */
void anotarPidEnTabla(int posicion, int pid_hijo) { tablahijos[posicion].pid_hijo = pid_hijo; }

/**
 * @brief Anota en la tabla que el hijo está vivo.
 * @param pid_hijo PID del hijo
 */
void anotarHijoVivo(int pid_hijo)
{
    for (int l = 0; l < max_Hijos; l++)
    {
        if (tablahijos[l].pid_hijo == pid_hijo) { tablahijos[l].stat = VIVO; }
    }
}

/**
 * @brief Mata a todos los hijos que estén vivos.
 */
void matarHijos(void)
{
    for (int f = 0; f < max_Hijos; f++)
    {
        if (tablahijos[f].stat == VIVO) { kill(tablahijos[f].pid_hijo, SIGTERM); }
    }
}

/**
 * @brief Explica el funcionamiento del programa por la salida de error.
 */
static void uso(void)
{
    fprintf(stderr, "\nUso: paralelo dir_resultados fich_imagen...\n");
    fprintf(stderr, "\nEjemplo: paralelo difuminadas orig/*.jpg\n\n");
}

/**
 * Aplica un filtro a una imagen y la guarda.
 * @param fich_imagen La imagen a procesar.
 * @param dir_resultados El directorio donde guardar el resultado.
 */
static void convertir(const char *fich_imagen, const char *dir_resultados)
{

    const char *nombre_base;
    char        nombre_destino[MAXPATHLEN];
    char        orden[MAXPATHLEN * 3];

    nombre_base = strrchr(fich_imagen, '/');
    if (nombre_base == NULL) nombre_base = fich_imagen;
    else
        nombre_base++;
    snprintf(nombre_destino, sizeof(nombre_destino), "%s/%s", dir_resultados, nombre_base);
    execlp("convert", "convert", fich_imagen, "-blur", "0x8", nombre_destino, NULL);
}

/**
 * @brief Manejador de señales
 * @param sig Señal recibida
 */
void manejador_signals(int sig)
{
    pid_t pid_hijo;
    int   e;

    switch (sig)
    {
    case SIGTERM:
        terminar = 1;
        do {
            pid_hijo = wait3(&e, WNOHANG, NULL);
        } while (pid_hijo > (pid_t)0);
        printf("Soy el padre con PID %d y he recibido SIGTERM.", getpid());
        break;

    default: break;
    }
}

// usar valgrind
// hacer lo mismo en servidor
// Subir con el commit y a moodle
