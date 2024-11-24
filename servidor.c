/******************************************************************************
 ** ISCTE-IUL: Trabalho prático 3 de Sistemas Operativos
 **
 ** Aluno: Nº: 110976      Nome: Diogo Pedro Cordeiro Pereira
 ** Nome do Módulo: servidor.c v2 (https://moodle22.iscte-iul.pt/mod/forum/discuss.php?d=6117#p10339)
 ** Descrição/Explicação do Módulo:
 **
 **
 ******************************************************************************/
#include "common.h"
// #define SO_HIDE_DEBUG   // Uncomment this line to hide all @DEBUG statements
#include "so_utils.h"
#include <sys/wait.h>

/* Variáveis globais */
int shmId = RET_ERROR;                  // Variável que tem o ID da Shared Memory
int msgId = RET_ERROR;                  // Variável que tem o ID da Message Queue
int semId = RET_ERROR;                  // Variável que tem o ID do Grupo de Semáforos
MsgContent msg;                         // Variável que serve para todas as mensagens trocadas entre Cliente e Servidor
DadosServidor *db = NULL;               // Variável que vai ficar com UM POINTER PARA a memória partilhada
int indexClient;                        // Índice do cliente que fez o pedido ao servidor/servidor dedicado na BD
// int nrServidoresDedicados = 0;          // Número de Servidores Dedicados criados.

/* Protótipos de funções. Os alunos não devem alterar estes protótipos. */
int initShm_S1();                       // S1:   Função a ser completada pelos alunos (definida abaixo)
int initMsg_S2();                       // S2:   Função a ser completada pelos alunos (definida abaixo)
int initSem_S3();                       // S3:   Função a ser completada pelos alunos (definida abaixo)
int triggerSignals_S4();                // S4:   Função a ser completada pelos alunos (definida abaixo)
MsgContent receiveClientLogin_S5();     // S5:   Função a ser completada pelos alunos (definida abaixo)
int createServidorDedicado_S6();        // S6:   Função a ser completada pelos alunos (definida abaixo)
void shutdownAndExit_S7();              // S7:   Função a ser completada pelos alunos (definida abaixo)
void handleSignalSIGINT_S8(int);        // S8:   Função a ser completada pelos alunos (definida abaixo)
void handleSignalSIGCHLD_S9(int);       // S9:   Função a ser completada pelos alunos (definida abaixo)
int triggerSignals_SD10();              // SD10: Função a ser completada pelos alunos (definida abaixo)
int validateRequest_SD11(Login);        // SD11: Função a ser completada pelos alunos (definida abaixo)
int searchUserDB_SD12(Login);           // SD12: Função a ser completada pelos alunos (definida abaixo)
int reserveUserDB_SD13(int, int);       // SD13: Função a ser completada pelos alunos (definida abaixo)
int sendProductList_SD14(int, int);     // SD14: Função a ser completada pelos alunos (definida abaixo)
MsgContent receiveClientOrder_SD15();   // SD15: Função a ser completada pelos alunos (definida abaixo)
int sendPurchaseAck_SD16(int, int);     // SD16: Função a ser completada pelos alunos (definida abaixo)
void shutdownAndExit_SD17();            // SD17: Função a ser completada pelos alunos (definida abaixo)
void handleSignalSIGUSR1_SD18(int);     // SD18: Função a ser completada pelos alunos (definida abaixo)

/**
 * Main: Processamento do processo Servidor e dos processos Servidor Dedicado
 *       Não é suposto que os alunos alterem nada na função main()
 * @return int Exit value (RET_SUCCESS | RET_ERROR)
 */
int main() {
    if (    RET_ERROR == (shmId = initShm_S1()) ||
            RET_ERROR == (msgId = initMsg_S2()) ||
            RET_ERROR == (semId = initSem_S3()) ||
            RET_ERROR == triggerSignals_S4()  ) {
        shutdownAndExit_S7();
    }
    while (TRUE) {  // O processamento do Servidor é cíclico e iterativo
        msg = receiveClientLogin_S5();    // Convenciona-se que se houver problemas, esta função coloca msg.msgData.infoLogin.nif = USER_NOT_FOUND
        if (USER_NOT_FOUND == msg.msgData.infoLogin.nif)
            continue;   // Caso a leitura tenha tido problemas, não avança, e lê o próximo pedido

        int pidFilho = createServidorDedicado_S6();
        if (RET_ERROR == pidFilho)
            shutdownAndExit_S7();            // Caso o fork() tenha tido problemas, termina
        else if (PROCESSO_FILHO == pidFilho)
            break;                           // Se for o PROCESSO FILHO, sai do ciclo
    }                                        // Caso contrário, o PROCESSO PAI recomeça no passo S5

    // Por exclusão de partes, este código é apenas corrido pelo PROCESSO Servidor Dedicado (FILHO)
    if (    RET_ERROR == triggerSignals_SD10() ||
            RET_ERROR == validateRequest_SD11(msg.msgData.infoLogin)) {
        shutdownAndExit_SD17();
    }
    indexClient = searchUserDB_SD12(msg.msgData.infoLogin);
    if (    USER_NOT_FOUND == indexClient ||
            RET_ERROR == reserveUserDB_SD13(indexClient, msg.msgData.infoLogin.pidCliente) ||
            RET_ERROR == sendProductList_SD14(indexClient, msg.msgData.infoLogin.pidCliente)) {
        shutdownAndExit_SD17();
    }
    msg = receiveClientOrder_SD15();    // Convenciona-se que se houver problemas, esta função coloca msg.msgData.infoLogin.nif = USER_NOT_FOUND
    if (USER_NOT_FOUND == msg.msgData.infoLogin.nif) {
        shutdownAndExit_SD17();
    }
    sendPurchaseAck_SD16(msg.msgData.infoProduto.idProduto, msg.msgData.infoLogin.pidCliente);
    shutdownAndExit_SD17();
    so_exit_on_error(RET_ERROR, "ERRO: O servidor dedicado nunca devia chegar a este passo");
    return RET_ERROR;
}

/******************************************************************************
 * FUNÇÕES IPC SHARED MEMORY (cópia adaptada de /home/so/reference/shm-operations.c )
 *****************************************************************************/

/**
 * @brief Internal Private Function, not to be used by the students.
 */
int __shmGet( int nrElements, int shmFlags ) {
    int shmId = shmget( IPC_KEY, nrElements * sizeof(DadosServidor), shmFlags );
    if ( shmId < 0 ) {
        so_debug( "Could not create/open the Shared Memory with key=0x%x", IPC_KEY );
    } else {
        so_debug( "Using the Shared Memory with key=0x%x and id=%d", IPC_KEY, shmId );
    }
    return shmId;
}

/**
 * @brief Creates an IPC Shared Memory exclusively, associated with the IPC_KEY, of an array of size multiple of sizeof(DadosServidor)
 *
 * @param nrElements Size of the array
 * @return int shmId. In case of error, returns -1
 */
int shmCreate( int nrElements ) {
    return __shmGet( nrElements, IPC_CREAT | IPC_EXCL | 0600 );
}

/**
 * @brief Opens an already created IPC Shared Memory associated with the IPC_KEY
 *
 * @return int shmId. In case of error, returns -1
 */
int shmGet() {
    return __shmGet( 0, 0 );
}

/**
 * @brief Removes the IPC Shared Memory associated with the IPC_KEY
 *
 * @return int 0 if success or if the Shared Memory already did not exist, or -1 if the Shared Memory exists and could not be removed
 */
int shmRemove() {
    int shmId = shmGet();
    // Ignore any errors here, as this is only to check if the shared memory exists and remove it
    if ( shmId > 0 ) {
        // If the shared memory with IPC_KEY already exists, remove it
        int result = shmctl( shmId, IPC_RMID, 0 );
        if ( result < 0) {
            so_debug( "Could not remove the Shared Memory with key=0x%x and id=%d", IPC_KEY, shmId );
        } else {
            so_debug( "Removed the Shared Memory with key=0x%x and id=%d", IPC_KEY, shmId );
        }
        return result;
    }
    return 0;
}

/**
 * @brief Attaches a shared memory associated with shmId with a local address
 *
 * @param shmId Id of the Shared Memory
 * @return DadosServidor*
 */
DadosServidor* shmAttach( int shmId ) {
    DadosServidor* array = (DadosServidor*) shmat( shmId, 0, 0 );
    if ( NULL == array ) {
        so_debug( "Could not Attach the Shared Memory with id=%d", shmId );
    } else {
        so_debug( "The Shared Memory with id=%d was Attached on the local address %p", shmId, array );
    }
    return array;
}

/**
 * @brief Função utilitária, fornecida pelos vossos queridos professores de SO... Utility to Display the values of the shared memory
 *
 * @param shm Shared Memory
 * @param ignoreInvalid Do not display the elements that have the default value
 */
void shmView(DadosServidor* shm, int ignoreInvalid) {
    so_debug("Conteúdo da SHM Users:");
    for (int i = 0; i < MAX_USERS; ++i) {
        if (!ignoreInvalid || USER_NOT_FOUND != shm->listUsers[i].nif) {
            so_debug("Posição %2d: %9d | %-9s | %-24s | %3d | %2d | %2d", i, shm->listUsers[i].nif, shm->listUsers[i].senha, shm->listUsers[i].nome, shm->listUsers[i].saldo, shm->listUsers[i].pidCliente, shm->listUsers[i].pidServidorDedicado);
        }
    }
    so_debug("Conteúdo da SHM Produtos:");
    for (int i = 0; i < MAX_PRODUCTS; ++i) {
        if (!ignoreInvalid || PRODUCT_NOT_FOUND != shm->listProducts[i].idProduto) {
            so_debug("Posição %2d: %d | %-20s | %-7s | %d | %2d", i, shm->listProducts[i].idProduto, shm->listProducts[i].nomeProduto, shm->listProducts[i].categoria, shm->listProducts[i].preco, shm->listProducts[i].stock);
        }
    }
}

/******************************************************************************
 * FUNÇÕES IPC MESSAGE QUEUES (cópia de /home/so/reference/msg-operations.c )
 *****************************************************************************/

/**
 * @brief Internal Private Function, not to be used by the students.
 */
int __msgGet( int msgFlags ) {
    int msgId = msgget( IPC_KEY, msgFlags );
    if ( msgId < 0 ) {
        so_debug( "Could not create/open the Message Queue with key=0x%x", IPC_KEY );
    } else {
        so_debug( "Using the Message Queue with key=0x%x and id=%d", IPC_KEY, msgId );
    }
    return msgId;
}

/**
 * @brief Creates an IPC Message Queue NON-exclusively, associated with the IPC_KEY, of an array of size multiple of sizeof(Aluno)
 *
 * @return int msgId. In case of error, returns -1
 */
int msgCreate() {
    return __msgGet( IPC_CREAT | 0600 );
}

/**
 * @brief Opens an already created IPC Message Queue associated with the IPC_KEY
 *
 * @return int msgId. In case of error, returns -1
 */
int msgGet() {
    return __msgGet( 0 );
}

/**
 * @brief Removes the IPC Message Queue associated with the IPC_KEY
 *
 * @param msgId IPC MsgID
 * @return int 0 if success or if the Message Queue already did not exist, or -1 if the Message Queue exists and could not be removed
 */
int msgRemove( int msgId ) {
    // Ignore any errors here, as this is only to check if the Message Queue exists and remove it
    if ( msgId > 0 ) {
        // If the Message Queue with IPC_KEY already exists, remove it
        int result = msgctl( msgId, IPC_RMID, 0 );
        if ( result < 0) {
            so_debug( "Could not remove the Message Queue with key=0x%x and id=%d", IPC_KEY, msgId );
        } else {
            so_debug( "Removed the Message Queue with key=0x%x and id=%d", IPC_KEY, msgId );
        }
        return result;
    }
    return 0;
}

/**
 * @brief Sends the passed message to the IPC Message Queue associated with the IPC_KEY
 *
 * @param msgId IPC MsgID
 * @param msg Message to be sent
 * @param msgType Message Type (or Address)
 * @return int success
 */
int msgSend(int msgId, MsgContent msg, int msgType) {
    msg.msgType = msgType;
    int result = msgsnd(msgId, &msg, sizeof(msg.msgData), 0);
    if (result < 0) {
        so_debug( "Could not send the Message to the Message Queue with key=0x%x and id=%d", IPC_KEY, msgId );
    } else {
        so_debug( "Sent the Message to the Message Queue with key=0x%x and id=%d", IPC_KEY, msgId );
    }
    return result;
}

/**
 * @brief Reads a message of the passed tipoMensagem from the IPC Message Queue associated with the IPC_KEY
 *
 * @param msgId IPC MsgID
 * @param msg Pointer to the Message to be filled
 * @param msgType Message Type (or Address)
 * @return int Number of bytes read, or -1 in case of error
 */
int msgReceive(int msgId, MsgContent* msg, int msgType) {
    int result = msgrcv(msgId, msg, sizeof(msg->msgData), msgType, 0);
    if ( result < 0) {
        so_debug( "Could not Receive the Message of Type %d from the Message Queue with key=0x%x and id=%d", msgType, IPC_KEY, msgId );
    } else {
        so_debug( "Received a Message of Type %d from the Message Queue with key=0x%x and id=%d, with %d bytes", msgType, IPC_KEY, msgId, result );
    }
    return result;
}

/******************************************************************************
 * FUNÇÕES IPC SEMÁFOROS (cópia de /home/so/reference/sem-operations.c )
 *****************************************************************************/

/**
 * @brief Internal Private Function, not to be used by the students.
 */
int __semGet( int nrSemaforos, int semFlags ) {
    int semId = semget( IPC_KEY, nrSemaforos, semFlags );
    if ( semId < 0 ) {
        so_debug( "Could not create/open the Semaphores Group with key=0x%x", IPC_KEY );
    } else {
        so_debug( "Using the Semaphores Group with key=0x%x and id=%d", IPC_KEY, semId );
    }
    return semId;
}

/**
 * @brief Creates an IPC Semaphores Group non-exclusively, associated with the IPC_KEY, with the passed number of Semaphores
 *
 * @param nrElements Number of Semaphores of the Group
 * @return int semId. In case of error, returns -1
 */
int semCreate( int nrElements ) {
    return __semGet( nrElements, IPC_CREAT | 0600 );
}

/**
 * @brief Opens an already created IPC Semaphores Group associated with the IPC_KEY
 *
 * @return int semId. In case of error, returns -1
 */
int semGet() {
    return __semGet( 0, 0 );
}

/**
 * @brief Removes the IPC Semaphores Group associated with the IPC_KEY
 *
 * @param semId IPC SemId
 * @return int 0 if success or if the Semaphores Group already did not exist, or -1 if the Semaphores Group exists and could not be removed
 */
int semRemove( int semId ) {
    // Ignore any errors here, as this is only to check if the semaphore group exists and remove it
    if ( semId > 0 ) {
        // If the semaphore group with IPC_KEY already exists, remove it.
        int result = semctl( semId, 0, IPC_RMID, 0 );
        if ( result < 0) {
            so_debug( "Could not remove the Semaphores Group with key=0x%x and id=%d", IPC_KEY, shmId );
        } else {
            so_debug( "Removed the Semaphores Group with key=0x%x and id=%d", IPC_KEY, shmId );
        }
        return result;
    }
    return 0;
}

/**
 * @brief Sets the value of the Semaphore semNr of the Semaphore Group associated with IPC_KEY
 *
 * @param semId IPC SemId
 * @param semNr Index of the Semaphore to set value (starting in 0)
 * @param value Value to be defined in the Semaphore semNr
 * @return int success
 */
int semNrSetValue( int semId, int semNr, int value ) {
    int result = semctl( semId, semNr, SETVAL, value );
    if ( result < 0) {
        so_debug( "Could not set the value of the Semaphore %d of this Semaphore Group", semNr );
    } else {
        so_debug( "The Semaphore %d of this Semaphore Group was set with value %d", semNr, value );
    }
    return result;
}

/**
 * @brief Sets the value of the Semaphore 0 of the Semaphore Group associated with IPC_KEY
 *
 * @param semId IPC SemId
 * @param value Value to be defined in the Semaphore semNr
 * @return int success
 */
int semSetValue( int semId, int value ) {
    return semNrSetValue( semId, 0, value );
}

/**
 * @brief Gets the value of the Semaphore semNr of the Semaphore Group associated with IPC_KEY
 *
 * @param semId IPC SemId
 * @param semNr Index of the Semaphore to set value (starting in 0)
 * @return int Value of the Semaphore, or -1 in case of error
 */
int semNrGetValue( int semId, int semNr ) {
    int result = semctl( semId, semNr, GETVAL, 0 );
    if ( result < 0 ) {
        so_debug( "Could not get the value of the Semaphore %d of this Semaphore Group", semNr );
    } else {
        so_debug( "The Semaphore %d of this Semaphore Group has the value %d", semNr, result );
    }
    return result;
}

/**
 * @brief Gets the value of the Semaphore 0 of the Semaphore Group associated with IPC_KEY
 *
 * @param semId IPC SemId
 * @return int Value of the Semaphore, or -1 in case of error
 */
int semGetValue( int semId ) {
    return semNrGetValue( semId, 0 );
}

/**
 * @brief Adds a value to the current value the Semaphore semNr of the Semaphore Group associated with IPC_KEY
 *
 * @param semId IPC SemId
 * @param semNr Index of the Semaphore to set value (starting in 0)
 * @param value Value to be added to the value of the Semaphore semNr
 * @return int success
 */
int semNrAddValue( int semId, int semNr, int addValue ) {
    int result = semctl( semId, semNr, GETVAL, 0 );
    so_exit_on_error( result, "Error on semctl" );
    so_debug( "The Semaphore %d of this Semaphore Group had the value %d", semNr, result );

    struct sembuf operation = { semNr, addValue, 0 };
    result = semop( semId, &operation, 1 );

    if ( result < 0 ) {
        so_debug( "Could not add the value %d to the value of the Semaphore %d of this Semaphore Group", addValue, semNr );
    } else {
        result = semctl( semId, semNr, GETVAL, 0 );
        so_exit_on_error( result, "Error on semctl" );
        so_debug( "The Semaphore %d of this Semaphore Group now has the value %d", semNr, result );
    }
    return result;
}

/**
 * @brief Adds a value to the current value the Semaphore 0 of the Semaphore Group associated with IPC_KEY
 *
 * @param semId IPC SemId
 * @param value Value to be added to the value of the Semaphore semNr
 * @return int success
 */
int semAddValue( int semId, int addValue ) {
    return semNrAddValue( semId, 0, addValue );
}

/**
 * @brief Adds 1 (one) to the current value the Semaphore semNr of the Semaphore Group associated with IPC_KEY
 *
 * @param semId IPC SemId
 * @param semNr Index of the Semaphore to set value (starting in 0)
 * @return int success
 */
int semNrUp( int semId, int semNr ) {
    return semNrAddValue( semId, semNr, 1 );
}

/**
 * @brief Adds -1 (minus one) to the current value the Semaphore semNr of the Semaphore Group associated with IPC_KEY
 *
 * @param semId IPC SemId
 * @param semNr Index of the Semaphore to set value (starting in 0)
 * @return int success
 */
int semNrDown( int semId, int semNr ) {
    return semNrAddValue( semId, semNr, -1 );
}

/**
 * @brief Adds 1 (one) to the current value the Semaphore 0 of the Semaphore Group associated with IPC_KEY
 *
 * @param semId IPC SemId
 * @return int success
 */
int semUp( int semId ) {
    return semAddValue( semId, 1 );
}

/**
 * @brief Adds -1 (minus one) to the current value the Semaphore 0 of the Semaphore Group associated with IPC_KEY
 *
 * @param semId IPC SemId
 * @return int success
 */
int semDown( int semId ) {
    return semAddValue( semId, -1 );
}

/******************************************************************************
 * MÓDULO SERVIDOR
 *****************************************************************************/

/**
 *  O módulo Servidor é responsável pelo processamento das autenticações dos utilizadores.
 *  Está dividido em duas partes, o Servidor (pai) e zero ou mais Servidores Dedicados (filhos).
 *  Este módulo realiza as seguintes tarefas:
 */

/**
 * @brief S1    Abre/Cria a Shared Memory (SHM) do projeto, que tem a KEY IPC_KEY definida em
 *              common.h (alterar esta KEY para ter o valor do nº do aluno, como indicado nas
 *              aulas), realizando as seguintes operações:
 *        S1.1  Tenta abrir a Shared Memory (SHM) IPC com a referida KEY IPC_KEY. Em caso de
 *              sucesso na abertura da SHM, liga a variável db a essa SHM. Se não encontrar nenhum
 *              erro, dá so_success <shmId> (i.e., so_success("S1.1", "%d", shmId)), e retorna o ID
 *              da SHM (vai para S2). Caso contrário, dá so_error, (i.e., so_error("S1.1", "")).
 *        S1.2  Valida se o erro anterior foi devido a não existir ainda nenhuma SHM criada com
 *              essa KEY (testando o valor da variável errno). Se o problema não foi esse (mas
 *              outro qualquer), então dá so_error e retorna erro (vai para S7). Caso contrário
 *              (o problema foi não haver SHM criada), dá so_success.
 *        S1.3  Cria uma Shared Memory com a KEY IPC_KEY definida em common.h e com o tamanho para
 *              conter as duas listas do Servidor: uma de utilizadores (Login), que comporta um
 *              máximo de MAX_USERS elementos, e outra de produtos (Produto), que comporta um
 *              máximo de MAX_PRODUCTS elementos. Em caso de erro, dá so_error e retorna erro
 *              (vai para S7). Senão, dá so_success <shmId>.
 *        S1.4  Inicia a Lista de Utilizadores, preenchendo em todos os elementos o campo
 *              nif=USER_NOT_FOUND (“Limpa” a lista de utilizadores), e a Lista de Produtos,
 *              preenchendo em todos os elementos o campo idProduto=PRODUCT_NOT_FOUND (“Limpa” a
 *              lista de produtos). No final, dá so_success.
 *        S1.5  Lê o ficheiro FILE_DATABASE_USERS e preenche a lista de utilizadores na memória
 *              partilhada com a informação dos utilizadores, mas preenchendo sempre os campos
 *              pidCliente e pidServidorDedicado com o valor -1. Em caso de qualquer erro, dá
 *              so_error e retorna erro (vai para S7). Caso contrário, dá so_success.
 *        S1.6  Lê o ficheiro FILE_DATABASE_PRODUCTS e preenche a lista de produtos na memória
 *              partilhada com a informação dos produtos. Em caso de qualquer erro, dá so_error e
 *              retorna erro (vai para S7). Caso contrário, dá so_success e retorna o ID da SHM.
 *
 * @return int RET_ERROR em caso de erro, ou a shmId em caso de sucesso
 */
int initShm_S1() {
    int result = RET_ERROR;    // Por omissão retorna erro
    so_debug("<");

    // S1.1 //
    shmId = shmGet();
    if(shmId < 0){
        so_error("S1.1","");
        // S1.2 //
        if(errno == ENOENT){
            so_success("S1.2","");
            // S1.3 //
            shmId = shmCreate(1);
            db = shmAttach(shmId);
            if(shmId < 0 || db == NULL){
                so_error("S1.3","");
            }else{
                so_success("S1.3","%d",shmId);
                // S1.4 //
                for(int i = 0; i < MAX_USERS; i++){
                    db->listUsers[i].nif = USER_NOT_FOUND;
                }
                for(int i = 0; i < MAX_PRODUCTS; i++){
                    db->listProducts[i].idProduto = PRODUCT_NOT_FOUND;
                }
                so_success("S1.4","");
                // S1.5 //
                FILE *fileU;
                if((fileU = fopen(FILE_DATABASE_USERS,"r"))!=NULL){
                    Login user;
                    int index=0;
                    while(fread(&user, sizeof(Login), 1, fileU) == 1){
                        db->listUsers[index] = user;
                        db->listUsers[index].pidCliente = -1;
                        db->listUsers[index].pidServidorDedicado = -1;
                        index ++;
                        fseek(fileU,index*sizeof(Login),SEEK_SET);
                    }   
                    fclose(fileU);
                    so_success("S1.5","");
                    // S1.6 //
                    Produto item;
                    index = 0;
                    FILE* fileP;
                    if((fileP = fopen(FILE_DATABASE_PRODUCTS,"r"))!=NULL){
                        while(fread(&item, sizeof(Produto), 1, fileP) == 1){
                            db->listProducts[index] = item;
                            index ++;
                            fseek(fileP,index*sizeof(Produto),SEEK_SET);
                        }   
                        fclose(fileP);
                        so_success("S1.6","");
                        result = shmId;
                    }else{
                        so_error("S1.6", "");
                    }
                }else{
                    so_error("S1.5","");
                }
            }
        }else{
            so_error("S1.2","");
        }
    }else{
        so_success("S1.1","%d",shmId);
        db = shmAttach(shmId);
        result = shmId;
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief S2    Cria a Message Queue (MSG) do projeto, que tem a KEY IPC_KEY, realizando as
 *              seguintes operações:
 *        S2.1  Se já existir, deve apagar a fila de mensagens. Em caso de qualquer erro, dá
 *              so_error e retorna erro (vai para S7). Caso contrário, dá so_success.
 *        S2.2  Cria a Message Queue com a KEY IPC_KEY. Em caso de erro, dá so_error e retorna
 *              erro (vai para S7). Caso contrário, dá so_success <msgId> e retorna o ID da MSG.
 *
 * @return int RET_ERROR em caso de erro, ou a msgId em caso de sucesso
 */
int initMsg_S2() {
    int result = RET_ERROR;    // Por omissão retorna erro
    so_debug("<");

    // S2.1 //
    int idMsg = msgGet();
    if(idMsg > 0){
        if(msgRemove(idMsg) < 0){
            so_error("S2.1","");
            return result;
        }
        so_success("S2.1","");
    }else{
        so_success("S2.1","");
    }
    // S2.2 //
    idMsg = msgCreate();
    if(idMsg < 0){
        so_error("S2.2","");
    }else{
        so_success("S2.2","%d",idMsg);
        result = idMsg;
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief S3    Cria um grupo de semáforos (SEM) que tem a KEY IPC_KEY, realizando as seguintes
 *              operações:
 *        S3.1  Se já existir, deve apagar o grupo de semáforos. Em caso de qualquer erro, dá
 *              so_error e retorna erro (vai para S7). Caso contrário, dá so_success.
 *        S3.2  Cria um grupo de três semáforos com a KEY IPC_KEY. Em caso de qualquer erro, dá
 *              so_error e retorna erro (vai para S7). Caso contrário, dá so_success <semId>.
 *        S3.3  Inicia o valor dos semáforos SEM_USERS e SEM_PRODUCTS para que possam trabalhar em
 *              modo “exclusão mútua”, e inicia o valor do semáforo SEM_NR_SRV_DEDICADOS com o
 *              valor 0. Em caso de erro, dá so_error e retorna erro (vai para S7).
 *              Caso contrário, dá so_success e retorna o ID do SEM.
 *
 * @return int RET_ERROR em caso de erro, ou a semId em caso de sucesso
 */
int initSem_S3() {
    int result = RET_ERROR;    // Por omissão retorna erro
    so_debug("<");

    // S3.1 //
    int idSem = semGet();
    if(idSem != -1){
        if(semRemove(idSem) < 0){
            so_error("S3.1","");
            return result;
        }else{
            so_success("S3.1","");
        }
    }else{
        so_success("S3.1","");
    }
    // S3.2 //
    idSem = semCreate(3);
    if(idSem < 0){
        so_error("S3.2","");
    }else{
        so_success("S3.2","%d",idSem);
        // S3.3 //
        if(semNrSetValue(idSem, SEM_USERS, SEM_MUTEX_INITIAL_VALUE) < 0 
            || semNrSetValue(idSem, SEM_PRODUCTS, SEM_MUTEX_INITIAL_VALUE) < 0 
                || semNrSetValue(idSem, SEM_NR_SRV_DEDICADOS, 0) < 0){
            so_error("S3.3","");
        }else{
            so_success("S3.3","");
            result = idSem;
        }
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief S4    Arma e trata os sinais SIGINT (ver S8) e SIGCHLD (ver S9). Em caso de qualquer
 *              erro a armar os sinais, dá so_error e retorna erro (vai para S7).
 *              Caso contrário, dá so_success e retorna sucesso.
 *
 * @return int Sucesso (RET_SUCCESS | RET_ERROR)
 */
int triggerSignals_S4() {
    int result = RET_ERROR;    // Por omissão retorna erro
    so_debug("<");

    if(signal(SIGINT, handleSignalSIGINT_S8) == SIG_ERR || signal(SIGCHLD, handleSignalSIGCHLD_S9) == SIG_ERR) {
        so_error("S4", "");
    }else{  
        so_success("S4", "");
        result = RET_SUCCESS;
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief S5    Lê da Message Queue um pedido, ou seja, uma mensagem do tipo MSGTYPE_LOGIN. Se
 *              houver erro, dá so_error e retorna erro (reinicia o processo neste mesmo passo S5,
 *              lendo um novo pedido). Caso contrário, dá so_success <nif> <senha> <pidCliente>.
 *
 * @return MsgContent Elemento com os dados preenchidos. Se nif==USER_NOT_FOUND, significa que o elemento é inválido
 */
MsgContent receiveClientLogin_S5() {
    MsgContent msg;
    msg.msgData.infoLogin.nif = USER_NOT_FOUND;   // Por omissão retorna erro
    so_debug("<");

    if(msgReceive(msgId, &msg, MSGTYPE_LOGIN) < 0){
        so_error("S5","");
    }else{
        so_success("S5","%d %s %d", msg.msgData.infoLogin.nif, msg.msgData.infoLogin.senha, msg.msgData.infoLogin.pidCliente);
    }

    so_debug("> [@return nif:%d, senha:%s, pidCliente:%d]", msg.msgData.infoLogin.nif,
                        msg.msgData.infoLogin.senha, msg.msgData.infoLogin.pidCliente);
    return msg;
}

/**
 * @brief S6    Cria um processo filho (fork) Servidor Dedicado. Se houver erro, dá so_error e
 *              retorna erro (vai para S7). Caso contrário, o processo Servidor Dedicado (filho)
 *              continua no passo SD10, enquanto o processo Servidor (pai) dá so_success "Servidor
 *              Dedicado: PID <pidServidorDedicado>",
 *              e retorna o PID do novo processo Servidor Dedicado (recomeça no passo S5).
 *
 * @return int PID do processo filho, se for o processo Servidor (pai),
 *             PROCESSO_FILHO se for o processo Servidor Dedicado (filho),
 *             ou RET_ERROR em caso de erro.
 */
int createServidorDedicado_S6() {
    int pid_filho = RET_ERROR;    // Por omissão retorna erro
    so_debug("<");

    pid_filho = fork();
    if(pid_filho < 0){
        so_error("S6","");
    }else{
        if(pid_filho > 0){
            so_success("S6", "Servidor Dedicado: PID %d", pid_filho);
        }else{
            pid_filho = PROCESSO_FILHO;
        }
    }

    // ++nrServidoresDedicados;    // Incrementa o Número de Servidores Dedicados criados
    so_debug("> [@return:%d]", pid_filho);
    return pid_filho;
}

/**
 * @brief S7    Passo terminal para fechar o Servidor: dá so_success "Shutdown Servidor", e faz as
 *              seguintes ações:
 *        S7.1  Verifica se existe SHM aberta e alocada. Se não existir, dá so_error e passa para
 *              o passo S7.5, caso contrário, dá so_success.
 *        S7.2  Percorre a lista de utilizadores. A cada utilizador que tenha um Servidor Dedicado
 *              associado (significando que está num processo de compra de produtos), envia ao PID
 *              desse Servidor Dedicado o sinal SIGUSR1. Quando tiver processado todos os
 *              utilizadores existentes, dá so_success.
 *        S7.3  Dá so_success.
 *        S7.4  Reescreve o ficheiro bd_produtos.dat, mas incluindo apenas os produtos existentes
 *              na lista de produtos que tenham stock > 0. Em caso de qualquer erro, dá so_error.
 *              Caso contrário, dá so_success.
 *        S7.5  Apaga todos os elementos IPC (SHM, SEM e MSG) que tenham sido criados pelo Servidor
 *              com a KEY IPC_KEY. Em caso de qualquer erro, dá so_error. Caso contrário, dá
 *              so_success.
 *        S7.6  Termina o processo Servidor.
 */
void shutdownAndExit_S7() {
    so_debug("<");

    // S7 //
    so_success("S7","Shutdown Servidor");
    // S7.1 //
    if(shmId < 0){
        so_error("S7.1","");
    }else{
        so_success("S7.1","");
        // S7.2 //
        for(int i = 0; i < MAX_USERS; i++){
            if(db->listUsers[i].pidServidorDedicado != -1){
                kill(db->listUsers[i].pidServidorDedicado, SIGUSR1);
            }
        }
        so_success("S7.2","");
        // S7.3 //
        so_success("S7.3","");
        // S7.4 //
        FILE *file;
        file = fopen(FILE_DATABASE_PRODUCTS,"w");
        if(file == NULL){
            so_error("S7.4", "");
        }else{
            for(int i = 0; i < MAX_PRODUCTS; i++){
                if(db->listProducts[i].stock > 0){
                    if(fwrite(&db->listProducts[i], sizeof(Produto), 1, file) < 1){
                        so_error("S7.4", "");
                    }
                }
            }
            fclose(file);
            so_success("S7.4","");
        }
    }
    // S7.5 //
    if(semRemove(semId) < 0 || msgRemove(msgId) < 0 || shmRemove() < 0){
        so_error("S7.5","");
    }else{
        so_success("S7.5","");
    }

    // S7.6  Termina o processo Servidor.
    so_debug(">");
    exit(RET_SUCCESS);
}

/**
 * @brief S8    O sinal armado SIGINT serve para o dono da loja encerrar o Servidor, usando o
 *              atalho <CTRL+C>. Se receber esse sinal (do utilizador via Shell), o Servidor dá
 *              so_success, e vai para o passo terminal S7.
 *
 * @param sinalRecebido nº do Sinal Recebido (preenchido pelo SO)
 */
void handleSignalSIGINT_S8(int sinalRecebido) {
    so_debug("< [@param sinalRecebido:%d]", sinalRecebido);

    so_success("S8","");
    shutdownAndExit_S7();

    so_debug(">");
}

/**
 * @brief S9    O sinal armado SIGCHLD serve para que o Servidor seja alertado quando um dos seus
 *              filhos Servidor Dedicado terminar. Se o Servidor receber esse sinal, identifica o
 *              PID do Servidor Dedicado que terminou (usando wait), dá so_success "Terminou
 *              Servidor Dedicado <pidServidorDedicado>", retornando ao que estava a fazer
 *              anteriormente.
 *
 * @param sinalRecebido nº do Sinal Recebido (preenchido pelo SO)
 */
void handleSignalSIGCHLD_S9(int sinalRecebido) {
    so_debug("< [@param sinalRecebido:%d]", sinalRecebido);

    int pidSD =  wait (NULL);
    so_success("S9","Terminou Servidor Dedicado %d", pidSD);

    so_debug(">");
}

/**
 * @brief SD10      O novo processo Servidor Dedicado (filho) arma os sinais SIGUSR1 (ver SD18) e
 *                  SIGINT (programa-o para ignorar este sinal). Em caso de erro a armar os sinais,
 *                  dá so_error e retorna erro (vai para SD17). Caso contrário, dá so_success.
 *
 * @return int Sucesso (RET_SUCCESS | RET_ERROR)
 */
int triggerSignals_SD10() {
    int result = RET_ERROR;    // Por omissão retorna erro
    so_debug("<");

    if(signal(SIGUSR1, handleSignalSIGUSR1_SD18) == SIG_ERR || signal(SIGINT, SIG_IGN) == SIG_ERR) {
        so_error("SD10", "");
    }else{
        so_success("SD10", "");
        result = RET_SUCCESS;
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief SD11      O Servidor Dedicado deve validar, em primeiro lugar, no pedido Login recebido
 *                  do Cliente (herdado do processo Servidor pai), se o campo pidCliente > 0.
 *                  Se for, dá so_success e retorna sucesso.
 *                  Caso contrário, dá so_error e retorna erro (vai para SD17).
 *
 * @param request Pedido de Login que foi enviado pelo Cliente.
 * @return int Sucesso (RET_SUCCESS | RET_ERROR)
 */
int validateRequest_SD11(Login request) {
    int result = RET_ERROR;    // Por omissão retorna erro
    so_debug("< [@param request.nif:%d, request.senha:%s, request.pidCliente:%d]", request.nif,
                                                            request.senha, request.pidCliente);

    if(request.pidCliente > 0){
        so_success("SD11","");
        result = RET_SUCCESS;
    }else{
        so_error("SD11","");
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief SD12      Percorre a lista de utilizadores, atualizando a variável indexClient,
 *                  procurando pelo utilizador com o NIF recebido no pedido do Cliente.
 *        SD12.1    Se encontrou um utilizador com o NIF recebido,
 *                  e a Senha registada é igual à que foi recebida no pedido do Cliente,
 *                  então dá  so_success <indexClient>, e retorna indexClient (vai para SD13).
 *                  Caso contrário, dá so_error.
 *        SD12.2    Cria uma resposta indicando erro ao Cliente, preenchendo na estrutura Login o
 *                  campo pidServidorDedicado=-1. Envia essa mensagem para a fila de mensagens,
 *                  usando como msgType o PID do processo Cliente. Em caso de erro, dá so_error,
 *                  caso contrário dá  so_success.
 *                  Em ambos os casos, retorna erro (USER_NOT_FOUND, vai para SD17).
 *
 * @param request Pedido de Login que foi enviado pelo Cliente.
 * @return int Em caso de sucesso, retorna o índice do registo do cliente na listUsers (indexClient)
 *             Em caso de erro, retorna USER_NOT_FOUND
 */
int searchUserDB_SD12(Login request) {
    int indexClient = USER_NOT_FOUND;    // Por omissão retorna erro
    so_debug("< [@param request.nif:%d, request.senha:%s]", request.nif, request.senha);

    // SD12 //
    for(int i = 0; i < MAX_USERS /*|| db->listUsers[i].nif != USER_NOT_FOUND*/; i++){   
        // SD12.1 //         
        if(db->listUsers[i].nif == request.nif){
            if(!strcmp(db->listUsers[i].senha, request.senha)){
                so_success("SD12.1","%d",i);
                return i;
            }
        }
    }
    so_error("SD12.1","");
    // SD12.2 //
    msg.msgData.infoLogin.pidServidorDedicado = -1;
    if(msgSend(msgId, msg, msg.msgData.infoLogin.pidCliente) < 0){
        so_error("SD12.2","");
    }else{
        so_success("SD12.2","");
    }

    so_debug("> [@return:%d]", indexClient);
    return indexClient;
}

/**
 * @brief SD13      Reserva a entrada do utilizador na BD, atualizando na Lista de Utilizadores,
 *                  na posição indexClient, os campos pidServidorDedicado e pidCliente (com o
 *                  valor do pedido do Cliente), e dá so_success.
 *
 * @param indexClient O índice na Lista de Utilizadores do elemento correspondente ao cliente
 * @param pidCliente Process ID do cliente
 * @return int Sucesso (RET_SUCCESS | RET_ERROR)
 */
int reserveUserDB_SD13(int indexClient, int pidCliente) {
    int result = RET_ERROR;    // Por omissão retorna erro
    so_debug("< [@param indexClient:%d, pidCliente:%d]", indexClient, pidCliente);

    db->listUsers[indexClient].pidCliente = pidCliente;
    db->listUsers[indexClient].pidServidorDedicado = getpid();
    so_success("SD13","");
    result = RET_SUCCESS;

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief SD14      Cria a resposta indicando sucesso ao Cliente:
 *        SD14.1    Preenche na mensagem de resposta os campos nome e saldo da estrutura Login
 *                  com os valores da Lista de Utilizadores para indexClient. Preenche o campo
 *                  pidServidorDedicado com o PID do processo Servidor Dedicado, e dá so_success.
 *        SD14.2    Envia a lista de produtos ao Cliente: Percorre a Lista de Produtos, e por cada
 *                  produto com stock > 0, preenche a estrutura Produto da mensagem de resposta
 *                  com os dados do produto em questão, e envia, usando como msgType o PID do
 *                  processo Cliente, a mensagem de resposta ao Cliente.
 *                  Em caso de erro, dá so_error e retorna erro (vai para SD17).
 *                  No fim de enviar a lista, dá so_success.
 *        SD14.3    Depois de ter enviado todas as mensagens (uma por cada produto com stock > 0),
 *                  preenche uma nova mensagem final, preenchendo a estrutura Produto novamente,
 *                  colocando apenas o campo idProduto=FIM_LISTA_PRODUTOS, o que se convencionou
 *                  que significa que não há mais produtos a listar. Envia, usando como msgType o
 *                  PID do processo Cliente, a mensagem de resposta ao Cliente. Em caso de erro, dá
 *                  so_error e retorna erro (vai para SD17). Caso contrário, dá so_success.
 *
 * @param indexClient O índice na Lista de Utilizadores do elemento correspondente ao cliente
 * @param pidCliente Process ID do cliente
 * @return int Sucesso (RET_SUCCESS | RET_ERROR)
 */
int sendProductList_SD14(int indexClient, int pidCliente) {
    int result = RET_ERROR;    // Por omissão retorna erro
    so_debug("< [@param indexClient:%d, pidCliente:%d]", indexClient, pidCliente);

    // S14.1 //
    strcpy(msg.msgData.infoLogin.nome, db->listUsers[indexClient].nome);
    msg.msgData.infoLogin.saldo = db->listUsers[indexClient].saldo;
    msg.msgData.infoLogin.pidServidorDedicado = getpid();
    so_success("SD14.1","");
    // S14.2 //
    for(int i = 0; i < MAX_PRODUCTS; i++){
        if(db->listProducts[i].stock > 0){
            msg.msgData.infoProduto = db->listProducts[i];
            if(msgSend(msgId, msg, pidCliente) < 0){
                so_error("SD14.2","");
                return result;
            }
        }
    }
    so_success("SD14.2","");
    // SD14.3 //
    msg.msgData.infoProduto.idProduto = FIM_LISTA_PRODUTOS;
    if(msgSend(msgId, msg, pidCliente) < 0){
        so_error("SD14.3","");
    }else{
        so_success("SD14.3","");
        result = RET_SUCCESS;
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief SD15      Lê da fila de mensagens a resposta do Cliente: uma única mensagem com msgType
 *                  igual ao PID deste processo Servidor Dedicado, indicando no campo idProduto
 *                  qual foi o produto escolhido pelo Cliente. Em caso de erro, dá so_error, e
 *                  retorna erro (vai para SD17). Caso contrário, dá so_success.
 *
 * @return MsgContent Elemento com os dados preenchidos. Se nif==USER_NOT_FOUND, significa que o elemento é inválido
 */
MsgContent receiveClientOrder_SD15() {
    MsgContent msg;
    msg.msgData.infoLogin.nif = USER_NOT_FOUND;   // Por omissão retorna erro
    so_debug("<");

    if(msgReceive(msgId, &msg, getpid()) < 0){
        so_error("SD15","");
    }else{
        so_success("SD15","");
    }

    so_debug("> [@return nif:%d, idProduto:%d, pidCliente:%d]", msg.msgData.infoLogin.nif,
                        msg.msgData.infoProduto.idProduto, msg.msgData.infoLogin.pidCliente);
    return msg;
}

/**
 * @brief SD16      Produz a resposta final a dar ao Cliente:
 *        SD16.1    Se o idProduto enviado pelo Cliente for PRODUCT_NOT_FOUND, então preenche o
 *                  campo idProduto=PRODUTO_NAO_COMPRADO e dá so_error.
 *        SD16.2    Caso contrário, percorre a lista de produtos, procurando pelo produto com o
 *                  idProduto recebido no pedido do Cliente.
 *                  Se não encontrou nenhum produto com o idProduto recebido, ou se encontrou
 *                  esse produto, mas o mesmo já não tem stock (porque, entretanto, já esgotou),
 *                  preenche o campo idProduto=PRODUTO_NAO_COMPRADO e dá so_error.
 *                  Caso contrário, decrementa o stock do produto na Lista de Produtos, preenche
 *                  o campo idProduto=PRODUTO_COMPRADO, e dá  so_success.
 *                  Atenção: Deve cuidar para que o acesso ao stock do produto seja feito em exclusão!
 *        SD16.3    Envia, usando como msgType o PID do processo Cliente, a mensagem de resposta de
 *                  conclusão ao Cliente. Em caso de erro, dá so_error. Senão, dá so_success.
 *
 * @param idProduto O produto pretendido pelo Utilizador Cliente
 * @param pidCliente Process ID do cliente
 * @return int Sucesso (RET_SUCCESS | RET_ERROR)
 */
int sendPurchaseAck_SD16(int idProduto, int pidCliente) {
    int result = RET_ERROR;    // Por omissão retorna erro
    so_debug("< [@param idProduto:%d, pidCliente:%d]", idProduto, pidCliente);

    // SD16.1 //
    if(idProduto == PRODUCT_NOT_FOUND){
        msg.msgData.infoProduto.idProduto = PRODUTO_NAO_COMPRADO;
        so_error("SD16.1","");
    }else{
        // SD16.2 //
        semNrDown(semId, SEM_PRODUCTS);
        int i = 0;
        while(TRUE){
            if(i == MAX_PRODUCTS){
                msg.msgData.infoProduto.idProduto = PRODUTO_NAO_COMPRADO;
                so_error("SD16.2","");
                break;
            }
            if(db->listProducts[i].idProduto == idProduto){
                if(db->listProducts[i].stock > 0){
                    db->listProducts[i].stock--;
                    msg.msgData.infoProduto.idProduto = PRODUTO_COMPRADO;
                    so_success("SD16.2","");
                }else{
                    msg.msgData.infoProduto.idProduto = PRODUTO_NAO_COMPRADO;
                    so_error("SD16.2","");
                }
                break;
            }
            i++;
        }
        semNrUp(semId, SEM_PRODUCTS);
    }
    // S16.3 //
    if(msgSend(msgId, msg, pidCliente) < 0){
        so_error("SD16.3","");
    }else{
        so_success("SD16.3","");
        result = RET_SUCCESS;
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief SD17      Passo terminal para fechar o Servidor Dedicado:
 *                  dá so_success "Shutdown Servidor Dedicado", e faz as seguintes ações:
 *        SD17.1    Atualiza, na Lista de utilizadores para a posição indexClient, os campos
 *                  pidServidorDedicado=-1 e pidCliente=-1, e dá so_success.
 *        SD17.2    Termina o processo Servidor Dedicado.
 */
void shutdownAndExit_SD17() {
    so_debug("<");

    // SD17 //
    so_success("SD17","Shutdown Servidor Dedicado");
    // SD17.1 //
    db->listUsers[indexClient].pidCliente = -1;
    db->listUsers[indexClient].pidServidorDedicado = -1;
    so_success("SD17.1","");

    // SD17.2: Termina o processo Servidor Dedicado.
    so_debug(">");
    exit(RET_SUCCESS);
}

/**
 * @brief SD18  O sinal armado SIGUSR1 serve para que o Servidor Dedicado seja alertado quando o
 *              Servidor principal quer terminar. Se o Servidor Dedicado receber esse sinal,
 *              envia um sinal SIGUSR2 ao Cliente (para avisá-lo do Shutdown), dá so_success, e
 *              vai para o passo terminal SD17.
 *
 * @param sinalRecebido nº do Sinal Recebido (preenchido pelo SO)
 */
void handleSignalSIGUSR1_SD18(int sinalRecebido) {
    so_debug("< [@param sinalRecebido:%d]", sinalRecebido);

    kill(msg.msgData.infoLogin.pidCliente, SIGUSR2);
    so_success("SD18","");
    shutdownAndExit_SD17();

    so_debug(">");
}