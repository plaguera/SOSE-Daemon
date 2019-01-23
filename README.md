# SOSE : Demonio Linux
#### Pedro Lagüera Cabrera

## Instrucciones de Uso

En la careta src del proyecto viene incluido un Makefile que compila los archivos necesarios para generar el programa usando:

> Make

Iniciar el demonio:

> ./Daemon *ruta_directorio_1* *ruta_directorio_2* ...

Parar el demonio:

> ./Daemon stop

La estructura del proyecto es la siguiente:


>..
>
>├── log
>
>│   └── daemon.log
>
>├── README.md
>
>├── src
>
>│   ├── Daemon
>
>│   ├── Daemon.c
>
>│   ├── Daemon.o
>
>│   └── Makefile
>
>└── test

## Flujo de Ejecución del Programa

Al invocar al demonio, el primer paso es comprobar el número de argumentos que han sido pasados. Si no se han puestos argumentos o se ha utilizado `--help` se muestra la ayuda del programa, si se ha pasado el argumento stop se desea parar la ejecución del demonio y para ello se examinará el directorio `/proc` donde se encuentran los procesos en ejecución, si se encuentra un proceso con el mismo nombre que el demonio (`Daemon`) se lanzará una señal `SIGTERM` que hará que se termine la ejecución del demonio de manera no abrupta gracias a un `handler`. Si no existe ningún demonio en ejecución se termina con un error.

La principal característica de un demonio es que se ejecuta en segundo plano, para ello se hace uso de `fork()`, una función que inicia un nuevo proceso hijo dentro de su padre. Si se realiza con éxito, el proceso padre termina mostrando por pantalla el `PID` (Process ID) de su hijo, y su hijo pasa a ejecutar el código *significativo* del demonio.

En primer lugar, se establece un `handler` para recibir señales `SIGTERM` como la vista anteriormente y se comprueba que cada uno de los argumentos recibidos se corresponde con un directorio válido. Al utilizar el tabulador para autocompletar rutas al lanzar el demonio, una ruta puede terminar en `'/'`, esto puede causar complicaciones al concatenar `strings` más adelante, por lo que si existe un `'/'` al final de la ruta, se elimina.

Para poder continuar es necesario comprender la estructura utilizada, se denomina `WatchList` y consiste en un `array` de `node` y en la que cada uno de ellos almacenará un `Watch Descriptor` generado por `inotify` y la ruta del directorio que representan relativa al directorio de ejecución del demonio.

A continuación, se inicializan los descriptores para el archivo que almacenará los `logs` y para `inotify`, es relevante saber que cada vez que se lanza el demonio se crea un `log` nuevo con la fecha y hora de su creación. Una vez, inicializados los descriptores se añadirán los directorios raíz a la `WatchList` e `inotify`, dado que estos directorios pueden contener otros directorios, esta adición es recursiva, es decir, se viaja a través de todo el árbol de carpetas desde las raíces y se añaden a la `WatchList`.

Finalizada la inicialización, se entra en la ejecución del bucle principal del demonio. En el bucle se espera a la recepción de mensajes de `inotify`, y según las acciones que hayan sido realizadas se ejecutan ciertas funciones, y en todo caso se transcribirán al `log`.
 
  - Si se crea un nuevo directorio (`IN_CREATE`, `IN_ISDIR`): se añade a la `WatchList`.
  - Si se borra un directorio (`IN_DELETE_SELF`): se elimina de la `WatchList`.

\* La eliminación recursiva no es necesaria, dado que al tener a todos los subdirectorios en la `WatchList`, con vigilar la bandera `IN_DELETE_SELF` basta.

\*\* No es posible añadir un directorio que ya se encuentra en la `WatchList`.
