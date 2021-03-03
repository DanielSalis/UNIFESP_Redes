# GUIA DE UTILIZAÇÃO

## No Servidor

###  1 - Compilando (utilizando threads)
`gcc -Wall -pthread  -o file-server file-server.c` 

###  2 - Executando
`./file-server <porta>`

## No cliente (métodos)
### 1 - GET
`GET <diretorio_do_arquivo>`

### 2 - CREATE
`CREATE <diretorio_do_arquivo>`

### 3 - APPEND
`APPEND <diretorio_do_arquivo> <conteudo_do_arquivo>`

### 3 - REMOVE
`REMOVE <diretorio_do_arquivo>`
