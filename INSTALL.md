# Instrucciones de instalacion

## Drivers para comunicacion serial Win11

1. Descargar el `Driver universal` en este [enlace](https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads)
2. Descomprimir el archivo en algun lado (al finalizar el proceso se puede borrar el archivo original y el descomprimido)
3. Abrir Administrador de Dispositivos
4. Seleccionar `Puertos (COM & LPT)`
5. En una de las barras superiores de la ventana, seleccionar el icono del engranaje, la hoja y una flecha verde hacia arriba, que sería `Add drivers`
6. Seleccionar la carpeta del archivo descomprimido y hacer click en next
7. Drivers instalados

## Programar ESP32

1. Abrir VSCode con la extension de `PlatformIO` instalada (o instalarla y reiniciar)
2. Esperar a que PlatformIO se configure (abajo a la derecha muestra el estado, si no dice nada es que se configuró correctamente)
3. Abrir el archivo `main.cpp` y configurarlo como se desee (ej. wifi)
4. Conectar ESP32 a la PC
5. Mantener presionado el boton BOOT cada vez que se quiera cargar codigo
6. En la barra mas inferior de VSCode, seleccionar la flecha que apunta a la derecha para cargar el codigo
7. Una vez cargado en la placa, soltar el boton
8. Para corroborar que todo ande bien, en la barra inferior seleccionar el cable para abrir el monitor serial
9. Apretar el botón de RESET en la ESP
10. Se deberian ver mensajes
