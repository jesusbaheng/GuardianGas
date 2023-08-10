# GuardianGas
Proyecto sobre un detector de gas usando internet de las cosas

## Introducción 

En la era de la tecnologia como la que estamos viviendo, en la cual todo esta conectado gracias al IoT de manera inalambrica se ha facilitado el poder monitorear condiciones del entorno de manera remota, en este caso se busca crear un sistema de alarma, el cual advierta de un cambio considerable del nivel de Gas LP (Gas Licuado de Petroleo), el cual es de uso cotidiano en hogares y comercios. 

El proposito de este proyecto es realizar un prototipo que detecte niveles peligrosos de gas LP los y poder alertar a aquellos que se encuentren en la zona de peligro, asi como emitir una alerta en caso de que no se encuentre nadie en dicha zona para poder tomar acciones, asi aumentando la seguridad al momento de usar este recurso.

## Material necesario

Para realizar el prototipo se necesitan los siguientes materiales:

- ESP 32 Dev kit
- Sensor de gas LP MQ6
- LED rojo
- Buzzer
- Servomotor
- Protoboard
- Jumpers de conexión
- [Ubuntu 20.04](https://releases.ubuntu.com/20.04/)
- [Docker Engine](https://docs.docker.com/engine/install/ubuntu/#install-using-the-convenience-script)
- [NodeRed](https://nodered.org/docs/getting-started/local)
- [Nodos dashboard](https://flows.nodered.org/node/node-red-dashboard)
- [MySQL](https://hub.docker.com/_/mysql)

### Material de referencia

En los siguientes enlaces puedes encontrar los enlaces en la plataforma de edu.codigoiot.com que te permitirán realizar las configuraciones necesarias 

- [Instalación de Virutal Box y Ubuntu 20.04](https://edu.codigoiot.com/course/view.php?id=812)
- [Introducción a Docker](https://edu.codigoiot.com/course/view.php?id=996)
- [Aplicacion multicontenedor de servidor IoT con Docker compose](https://edu.codigoiot.com/mod/lesson/view.php?id=3889&pageid=3804&startlastseen=no)
- [Servidor del internet de las cosas con Node-Red](https://edu.codigoiot.com/course/view.php?id=997)
- [Accediendo al servidor de Bases de Datos](https://edu.codigoiot.com/course/view.php?id=1001)
- [Configuración de la IDE de Arduino para el ESP32CAM](https://edu.codigoiot.com/course/view.php?id=850)

Así mismo, aqui se encuentra un enlace adicional el cual explica como configurar el bot de Telegram.

- [Bot de Telegram ESP32](https://randomnerdtutorials.com/telegram-control-esp32-esp8266-nodemcu-outputs/)

## Instrucciones 

### Requisitos previos

Para poder usar este repositorio necesitas lo siguiente

1. Docker Engine.
2. NodeRed por Docker Compose.
3. Contenedor de NodeRed con el volumen de data activado.
4. Contenedor de MySQL con el volumen de configuración y data activado. Configurar ek archivo `my.cnf` con un `bind-address = 0.0.0.0`
5. Contenedor de Mosquitto con el volumen de configuración activado. Configurar el archivo ```mosquitto.conf``` con un listener en el puerto ```1883``` para todas las IPs ```0.0.0.0```.
6. Tu usuario de linux debe ser parte del grupo sudoers y del grupo docker
    - Puedes comprobar que tu usuario está en ambos grupos con el comando 
    
        ```
        groups $USER
        ```

    - En caso de que tu usuari no forme parte de dichos grupos, puedes arreglarlo con los siguientes comandos
        ``` 
        su
        sudo usermod -aG sudo newuser
        exit
        ```
7. Crea una base de datos para este ejercicio. Usa los siguientes comandos
    - Entra a MySQL con el comando `docker exec -it [id_contenedor] mysql -p`
    - Usa la contraseña que configuraste en el archivo **compose.yaml**, si usaste mi repositorio [servidor-IoT-basico-docker-compose](https://github.com/hugoescalpelo/servidor-IoT-basico-docker-compose), la contraseña predeterminada es `my-secret-pw`
    - Crea una nueva base de datos  con el comando `CREATE DATABASE GuardianGas;`
    - Selecciona la base de datos con el comando `USE GuardianGas;`
    - Crea una tabla con el comando siguiente:
        ```
        CREATE TABLE status (id_status INT(6) UNSIGNED AUTO_INCREMENT PRIMARY KEY,
        estado VARCHAR(248) NOT NULL);
        ``` 
    - Haz un registro con el siguiente comando:
        ``` 
        INSERT INTO status(estado) VALUE ("excelente");
        ``` 
    - Haz un segundo registro con el siguiente comando:
        ``` 
        INSERT INTO status(estado) VALUE ("PELIGRO");
        ``` 
    - Crea una tabla con el comando siguiente:
        ```
        CREATE TABLE Cliente (
        id_cliente INT(6) UNSIGNED AUTO_INCREMENT PRIMARY KEY,
        fecha TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        nombre VARCHAR(248) NOT NULL,
        PPM FLOAT(4,2) NOT NULL,
        status_ID INT(6) UNSIGNED,
        FOREIGN KEY (status_ID) REFERENCES status(id_status)
        );
        ```
    - Crea un usuario con acceso remoto con el comando `CREATE USER 'usuario'@'%' IDENTIFIED BY 'password';` Cambia la palabra **usuario** y **password** para personalizar el usuario creado. Usa el comando sin quitar comillas simples. No olvides el punto y coma.
    - Da acceso al usuario creado con el comando `GRANT ALL PRIVILEGES ON *.* TO 'user'@'%';`    
    
### Instrucciones de preparación de entorno

Para arrancar el entorno necesario, puedes usar los siguientes comandos.

1. Comprueba que los contenedores de Mosquitto, NodeRed y MySQL estén funcionado. Puedes comprobarlo con el comando ```docker ps -a```. En caso de que tus contenedores no estén funcionando, puedes arrancarlos con el comando ```docker start $(docker ps -a -q)```.
2. Generar el broker publico con el siguiente comando en la ventana de comandos de Linux:
        ```
        nslookup broker.hivemq.com
        ```
3. Asegurate de tener instalados los nodos ```node-red-dashboard``` en nodeRed.

### Para la creación del flow en NodeRed

Estas son las instrucciones para crear el flow desde cero:

1. Crear un nuevo flow
2. Agregar un nodo MQTT
    - Server:```Broker Público``` Agregar el IP del broker generado anteriormente.
    - Tema: ```CodigoIoT/GuardianGas```
    - Output: a String
3. Agregar un nodo JSON, con la configuracion de siempre convertir a JavaScript Object
4. Agregar dos nodos function, los cuales serán los siguientes.

Nodo function PPM

```
msg.payload = msg.payload.ppm; 
msg.topic = "ppm"
return msg;
```
Nodo function Query

```
msg.topic = "INSERT INTO Cliente (nombre,PPM,status_ID) VALUES ('" + msg.payload.id +"'," + msg.payload.ppm + "," + msg.payload.estado + ");";
return msg;
```

5. Crear una pestaña y dos grupos de información 
- Pestaña: GuardianGas
- Grupo1: Indicador
- Grupo2: Historico
6. Agregar un nodo gauje y configurarlo
- Asociarlo al grupo Indicador
- Determine etiquetas y rangos
7. Agregar el nodo chart 
- Asociarlo al grupo Historico
8. Agregar el nodo MySQL
- Colocar el nombre de la base de datos ```GuardianGas```
- Colocar el Usuario y contraseña creados anteriormente.

9. Agregar el nodo de entrada de texto
- Asociarlo al grupo Historico
10. Agregar el nodo function 

Nodo function NameGG
```
global.set ("NameGG", msg.payload);
return msg;
```
11. Agregar el nodo button 
- Asociarlo al grupo Historico
12. Agregar el nodo function 

Nodo function Consultando 
```
msg.topic = "SELECT c.fecha, c.nombre, c.PPM, s.estado FROM Cliente c INNER JOIN status s ON c.status_ID = s.id_status WHERE c.status_ID = 2 AND c.nombre = '" + global.get("NameGG") + "' ORDER BY c.fecha DESC;"; 
return msg;
```
13. Agregar el nodo MySQL, el cual ya ha sido configurado en el paso 8
14. Agregar un nodo JSON, con la configuracion de siempre convertir a JavaScript Object
15. Agregar un nodo template

nodo template
```
    <table border="1" style="width:100px; margin-left:auto;margin-right:auto">
        <tr>
            <th>Fecha</th>
            <th>Nombre</th>
            <th>PPM</th>
            <th>Estado</th>
        </tr>
        <tr ng-repeat="dato in msg.payload">
            <td> {{dato.fecha}}</td>
            <td>{{dato.nombre}}</td>
            <td>{{dato.PPM}}</td>
            <td>{{dato.estado}}</td>
        </tr>
    </table>
<style>
    table{
    backgrond-color:white;
    width: 100%;
    text-align:left;
    border-collapse: collapse;
    }
    th, td{
    padding: 10px;
    }
    tr:nth-child(even){
    background-color:#D3D3D3;
    }
    tr:hover td{
    backgrond-color:darkgray;
    color:gray;
    }
</style>
```
16. Hacer click en Deploy
17. Consultar el [Dashboard](http://localhost:1880/ui)

### armado del circuito

El circuito a realizar se hara de acuerdo al siguiente diagrama.

![](https://github.com/jesusbaheng/GuardianGas/blob/main/Imagenes/diagrama%20de%20conexion.png?raw=true)

### Para el programa de la ESP32

1. con el IDE de arduino abrir el programa proyectoDiplomadoIoT.ino y realizar las configuraciones.
2. En la linea 50 y 51 poner el nombre de su red wifi y contraseña
3. En la linea 52 colocar el IP del broker publico
4. En la linea 60 colocar el mismo tema colocado en Node-Red
5. En las lineas 68 y 69 agregar tu token de telegram y el chat ID, ambos datos proporcionados por el BotFather
6. En la linea 164 cambiar ```Maquina``` por el nombre de tu preferencia.
7. subir el programa a la tarjeta empleada.

### Instrucciones de operación 

Para ver los resultados en las graficas del flow debes de asegurarte que el programa haya sido compilado exitosamente y haber colocado el mismo broker publico en ambos programas.

El circuito envia informacion cada 5 segundos, por lo que los resultados deberian de verse en tiempo real. 

Para hacer la consulta el sistema debera haber registrado niveles de gas LP superiores a 100 ppm (partes por millon) con colocar un encendedor cerca del sensor y presionar el boton que libere el gas se activaran las alarmas y tanto el chat de telegram como el servo responderan. 

finalmente la consulta se podra realizar colocando el nombre asignado a su maquina y podra consultar cuantas veces se encendieron las alarmas.

## Resultados

A continuación, podra verse una vista previa del resultado del flow, del ensamble del circuito y el bot de telegram.

![](https://github.com/jesusbaheng/GuardianGas/blob/main/Imagenes/Screenshot%20from%202023-08-09%2018-45-41.png?raw=true)
![](https://github.com/jesusbaheng/GuardianGas/blob/main/Imagenes/IMG_20230728_212311.jpg?raw=true)
![](https://github.com/jesusbaheng/GuardianGas/blob/main/Imagenes/chatTelegram.png?raw=true)

## Evidencias 

[Youtube](https://youtu.be/EcUepsirBcM)

# Notas
- Este repositorio cuenta con las instrucciones para crear el flow pero también incluye el archivo JSON resultante, así que solo tienes que importarlo a nodeRed y realizar las configuraciones necesarias.

# Créditos

Desarrollado por Jesús Bahena
- [Perfil de GitHub](https://github.com/jesusbaheng)
Desarrollado por Luis Felipe Saldivar
- [Perfil de GitHub](https://github.com/FelipeSaldivarOrtiz)