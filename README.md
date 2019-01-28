# SOSE : Demonio Linux
#### Pedro Lagüera Cabrera

## Instrucciones de Uso

En la carpeta src del proyecto viene incluido un Makefile que compila los archivos necesarios para generar el programa.

##### Compilar el demonio (cliente):

> make (Daemon)

##### Compilar el Servidor:

> make Server

##### Iniciar el Demonio:

> Daemon (-s dirección_servidor) (-p número_puerto) *ruta_directorio_1* *ruta_directorio_2* ...

##### Iniciar el Servidor:

> Server número_puerto

##### Parar el Demonio:

> Daemon stop

##### Parar el Servidor:

> Server stop

##### Estructura del proyecto:


> 		.
>		├── log
> 		├── README.md
> 		└── src
>     		 ├── Daemon.c
>     		 ├── Daemon.h
>     		 ├── Makefile
>     		 ├── network
>     		 │   ├── Server.c
>     		 │   └── Server.h
>     		 ├── Utils.c
>     		 └── Utils.h
>
> 3 directories, 10 files


## Flujo de Ejecución del Demonio

Al invocar al demonio, el primer paso es comprobar el número de argumentos que han sido pasados. Si no se han puesto argumentos o se ha utilizado `--help` se muestra la ayuda del programa, si se ha pasado el argumento stop se desea parar la ejecución del demonio y para ello se examinará el directorio `/proc` donde se encuentran los procesos en ejecución, si se encuentra un proceso con el mismo nombre que el demonio (`Daemon`) se lanzará una señal `SIGTERM` que hará que se termine la ejecución del demonio de manera no abrupta gracias a un `handler`. Si no existe ningún demonio en ejecución se termina con un error. Si se han utilizando los argumentos `-s` o `-p` para indicar los datos del servidor se guardan para ser usados más adelante, de lo contrario se utilizan los valores por defecto.

La principal característica de un demonio es que se ejecuta en segundo plano, para ello se hace uso de `fork()`, una función que inicia un nuevo proceso hijo dentro de su padre. Si se realiza con éxito, el proceso padre termina mostrando por pantalla el `PID` (Process ID) de su hijo, y su hijo pasa a ejecutar el código *significativo* del demonio.

En primer lugar, se establece un `handler` para recibir señales `SIGTERM` como la vista anteriormente y se comprueba que cada uno de los argumentos recibidos se corresponde con un directorio válido. Al utilizar el tabulador para autocompletar rutas al lanzar el demonio, una ruta puede terminar en `'/'`, esto puede causar complicaciones al concatenar `strings` más adelante, por lo que si existe un `'/'` al final de la ruta, se elimina.

El siguiente paso consiste en establecer una conexión bidireccional entre el demonio y un servidor para poder enviar mensajes entre ellos, para ello se crea un `socket` en el demonio y se intenta establecer la conexión, en el caso de que se produzca un fallo en `connect` el demonio aborta la ejecución con error.

Para poder continuar es necesario comprender la estructura utilizada, se denomina `WatchList` y consiste en un `array` de `node` y en la que cada uno de ellos almacenará un `Watch Descriptor` generado por `inotify` y la ruta del directorio que representan relativa al directorio de ejecución del demonio.

A continuación, se inicializan los descriptores para el archivo que almacenará los `logs` y para `inotify`, es relevante saber que cada vez que se lanza el demonio se crea un `log` nuevo con la fecha y hora de su creación. Una vez, inicializados los descriptores se añadirán los directorios raíz a la `WatchList` e `inotify`, dado que estos directorios pueden contener otros directorios, esta adición es recursiva, es decir, se viaja a través de todo el árbol de carpetas desde las raíces y se añaden a la `WatchList`.

Finalizada la inicialización, se entra en la ejecución del bucle principal del demonio. En el bucle se espera a la recepción de mensajes de `inotify`, y según las acciones que hayan sido realizadas se ejecutan ciertas funciones, y en todo caso se transcribirán al `log` y serán enviadas al servidor que las almacenará y transcribirá en sus propios `logs` y devolverá un mensaje de confirmación al demonio si todo ha ido bien.
 
  - Si se crea un nuevo directorio (`IN_CREATE`, `IN_ISDIR`): se añade a la `WatchList`.
  - Si se borra un directorio (`IN_DELETE_SELF`): se elimina de la `WatchList`.

\* La eliminación recursiva no es necesaria, dado que al tener a todos los subdirectorios en la `WatchList`, con vigilar la bandera `IN_DELETE_SELF` basta.

\*\* No es posible añadir un directorio que ya se encuentra en la `WatchList`.


## Flujo de Ejecución del Servidor

El inicio de la ejecución del servidor es muy similar al del demonio, primero se comprueban los argumentos, parando el servidor si es `stop`. A continuación, se utiliza `fork()` para ejecutar el servidor en segundo plano, se establece el `handler`para `SIGTERM`, se abre un nuevo archivo para almacenar los `logs` que se reciban durante esta ejecución, y se abre un `socket` que escuche por nuevos clientes que quieran estaablecer una comunicación.

En este caso el bucle principal tiene la función de escuchar constantemente por nuevos clientes y en el caso de que se encuentre uno, crear un nuevo hilo de ejecución que maneje la comunicación entre ese cliente y el servidor. Como se puede deducir, el servidor soporta la comunicación simultánea con varios clientes, sin embargo, el servidor no *loguea* los mensajes que recibe sino que los almacena y una vez cada 60 segundos (en otro hilo distinto) escribe las estadísticas de cada uno de los clientes que se han conectado desde el inicio de su ejecución.

\* La interferencia entre hilos es controlada mediante un mutex
