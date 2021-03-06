/*
 * RACK - Robotics Application Construction Kit
 * Copyright (C) 2005-2006 University of Hannover
 *                         Institute for Systems Engineering - RTS
 *                         Professor Bernardo Wagner
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Authors
 *      Joerg Langenberg <joerg.langenberg@gmx.net>
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <semaphore.h>

#define NAME "TimsMsgClient"

#include <main/tims/tims.h>
#include <main/tims/msgtypes/tims_msg_types.h>
#include <main/tims/router/tims_msg_router.h>

#define TIMS_LEVEL_PRINT          0
#define TIMS_LEVEL_DBG            1
#define TIMS_LEVEL_DBG_DETAIL     2

#define tims_print(fmt, ... )       \
                                do { \
                                  if (loglevel >= TIMS_LEVEL_PRINT) \
                                    printf(NAME": "fmt, ##__VA_ARGS__); \
                                } while(0)

#define tims_dbg(fmt, ... )       \
                                do { \
                                  if (loglevel >= TIMS_LEVEL_DBG) \
                                    printf(NAME": "fmt, ##__VA_ARGS__); \
                                } while(0)

#define tims_dbgdetail(fmt, ... )       \
                                do { \
                                  if (loglevel >= TIMS_LEVEL_DBG_DETAIL) \
                                    printf(NAME": "fmt, ##__VA_ARGS__); \
                                } while(0)

// init flags
#define TIMS_CLIENT_CONFIG_MSG          0x0001
#define TIMS_CLIENT_SEM_TCP_SEND        0x0002
#define TIMS_CLIENT_SEM_PIPE_SEND       0x0004
#define TIMS_CLIENT_SEM_WATCHDOG        0x0008
#define TIMS_CLIENT_PIPE_TIMS_CLIENT    0x0010
#define TIMS_CLIENT_PIPE_CLIENT_TIMS    0x0020
#define TIMS_CLIENT_THREAD_TCPRECV      0x0040
#define TIMS_CLIENT_THREAD_WATCHDOG     0x0080

#define TIMS_CLIENT_TCP_CONNECT         0x0100
#define TIMS_CLIENT_TCP_RECV_MSG        0x0200
#define TIMS_CLIENT_PIPE_RECV_MSG       0x0800

//
// parameter, constants and variables
//


#define                   DEFAULT_IP     "127.0.0.1"
#define                   DEFAULT_PORT   2000
#define                   DEFAULT_CFG    ""
#define                   DEFAULT_MAX    256

static unsigned int       maxMsgSize     = 0;
static unsigned int       init_flags     = 0;
static int                terminate      = 0;

static pthread_t          tcpRecvThread;
static int                tcpSocket      = -1;
static struct sockaddr_in tcpAddr;
static sem_t              tcpSendSem;

static int                pipeTimsToClientFd  = -1;
static int                pipeClientToTimsFd  = -1;
static sem_t              pipeSendSem;

#define                   MAX_CONFIG_NUM 512
static timsMsgRouter_ConfigMsg* configMsg = NULL;

static pthread_t          watchdogThread;
static int                watchdog = 0;
static sem_t              watchdogSem;

static timsMsgHead*       tcpRecvMsg;
static timsMsgHead*       pipeRecvMsg;

static int                loglevel = 0;


void cleanup(void)
{
    terminate = 1;

    tims_dbg("[CLEANUP] Terminate\n");

    if (init_flags & TIMS_CLIENT_TCP_CONNECT)
    {
        close(tcpSocket);
        tcpSocket = -1;
        tims_dbgdetail("[CLEANUP] socket closed\n");
        init_flags &= ~TIMS_CLIENT_TCP_CONNECT;
    }

    if (init_flags & TIMS_CLIENT_PIPE_RECV_MSG)
    {
        free(pipeRecvMsg);
        pipeRecvMsg = NULL;
        tims_dbgdetail("[CLEANUP] free pipe receive buffer\n");
        init_flags &= ~TIMS_CLIENT_PIPE_RECV_MSG;
    }

    if (init_flags & TIMS_CLIENT_THREAD_TCPRECV)
    {
        pthread_join(tcpRecvThread, NULL);
        tims_dbgdetail("[CLEANUP] join tcpRecvThread\n");
        init_flags &= ~TIMS_CLIENT_THREAD_TCPRECV;
    }

    if (init_flags & TIMS_CLIENT_THREAD_WATCHDOG)
    {
        pthread_join(watchdogThread, NULL);
        tims_dbgdetail("[CLEANUP] join watchdogThread\n");
        init_flags &= ~TIMS_CLIENT_THREAD_WATCHDOG;
    }

    if (init_flags & TIMS_CLIENT_PIPE_CLIENT_TIMS)
    {
        close(pipeClientToTimsFd);
        pipeClientToTimsFd = -1;
        tims_dbgdetail("[CLEANUP] pipe to tims closed\n");
        init_flags &= ~TIMS_CLIENT_PIPE_CLIENT_TIMS;
    }

    if (init_flags & TIMS_CLIENT_PIPE_TIMS_CLIENT)
    {
        close(pipeTimsToClientFd);
        pipeTimsToClientFd = -1;
        tims_dbgdetail("[CLEANUP] pipe from tims closed\n");
        init_flags &= ~TIMS_CLIENT_PIPE_TIMS_CLIENT;
    }

    if (init_flags & TIMS_CLIENT_SEM_WATCHDOG)
    {
        sem_destroy(&watchdogSem);
        tims_dbgdetail("[CLEANUP] destroy watchdogSem\n");
        init_flags &= ~TIMS_CLIENT_SEM_WATCHDOG;
    }

    if (init_flags & TIMS_CLIENT_SEM_PIPE_SEND)
    {
        sem_destroy(&pipeSendSem);
        tims_dbgdetail("[CLEANUP] destroy pipeSendSem\n");
        init_flags &= ~TIMS_CLIENT_SEM_PIPE_SEND;
    }

    if (init_flags & TIMS_CLIENT_SEM_TCP_SEND)
    {
        sem_destroy(&tcpSendSem);
        tims_dbgdetail("[CLEANUP] destroy tcpSendSem\n");
        init_flags &= ~TIMS_CLIENT_SEM_TCP_SEND;
    }

    if (init_flags & TIMS_CLIENT_CONFIG_MSG)
    {
        free(configMsg);
        configMsg = NULL;
        tims_dbgdetail("[CLEANUP] free config message buffer\n");
        init_flags &= ~TIMS_CLIENT_CONFIG_MSG;
    }

    if (init_flags & TIMS_CLIENT_TCP_RECV_MSG)
    {
        if (tcpRecvMsg)
        {
            free(tcpRecvMsg);
            tcpRecvMsg = NULL;
            tims_dbgdetail("[CLEANUP] free tcp receive buffer (cleanup)\n");
            init_flags &= ~TIMS_CLIENT_TCP_RECV_MSG;
        }
    }
    tims_dbgdetail("[CLEANUP] exit\n");
}

void signal_handler(int arg)
{
  cleanup();
}

void *watchdogTask(void *arg)
{
    signal(SIGHUP,  signal_handler);
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, signal_handler);

    tims_dbg("[WATCHDOG] init\n");

    while (!terminate)
    {
        sem_wait(&watchdogSem);
        watchdog++;

        if (watchdog > 10)
        {
            sem_post(&watchdogSem);
            tims_print("[WATCHDOG] call signal handler\n");
            signal_handler(0);
        }
        else
        {
            sem_post(&watchdogSem);
        }
        sleep(1);
    }
    return NULL;
}

int sndTcpTimsMsg(timsMsgHead* sndMsg)
{
    sem_wait(&tcpSendSem);
    int ret;

    tims_dbgdetail("[TCP]  %8x --(%04d)--> %8x, sending %u bytes\n",
                   sndMsg->src, sndMsg->type, sndMsg->dest, sndMsg->msglen);

    ret = send(tcpSocket, sndMsg, sndMsg->msglen, 0);
    if (ret != sndMsg->msglen)
    {
        tims_print("[TCP]  %8x --(%04d)--> %8x, (%u bytes), send ERROR, "
                   "code = %d\n", sndMsg->src, sndMsg->type, sndMsg->dest,
                   sndMsg->msglen, ret);
        sem_post(&tcpSendSem);
        return -1;
    }
    sem_post(&tcpSendSem);
    return 0;
}

int recvTcpTimsMsg(timsMsgHead* recvMsg)
{
  int          ret;
  unsigned int len = 0;

  // receive head
  while (len < TIMS_HEADLEN) {
    ret = recv(tcpSocket, (void*)recvMsg + len, TIMS_HEADLEN - len, 0);
    if (ret < 0) {
      tims_print("[TCP] recv head ERROR, code = %d \n", ret);
      return ret;
    }
    if (ret == 0) {
      tims_print("[TCP] recv head ERROR, socket closed\n");
      return -1;
    }
    len += ret;

    if (terminate) {
      return -EINTR;
    }

  }
  tims_parse_head_byteorder(recvMsg);

  if (recvMsg->msglen < TIMS_HEADLEN) {

    tims_print("[TCP] recv ERROR, invalid message length: %u is smaller than TIMS_HEADLEN\n",
               recvMsg->msglen);
    return -1;

  } else if (recvMsg->msglen > maxMsgSize) {

    tims_print("[TCP]  %8x --(%04d)--> %8x, recv ERROR, message (%u bytes) is too big for buffer (%u bytes)\n",
               recvMsg->src, recvMsg->type, recvMsg->dest, recvMsg->msglen, maxMsgSize);
    return -1;

  }

  if (recvMsg->msglen > TIMS_HEADLEN) {

    // read body
    while (len < recvMsg->msglen) {

      ret = recv(tcpSocket, (void*)recvMsg + len, recvMsg->msglen - len, 0);
      if (ret < 0) {
        tims_print("[TCP]  %8x --(%04d)--> %8x, recv body ERROR, code = %d \n",
                   recvMsg->src, recvMsg->type, recvMsg->dest, ret);
        return ret;
      }
      if (ret == 0) {
        tims_print("[TCP]  %8x --(%04d)--> %8x, recv body ERROR, socket closed, code = %d\n",
                   recvMsg->src, recvMsg->type, recvMsg->dest, ret);
        return -1;
      }
      len += ret;

      if (terminate) {
        return -EINTR;
      }

    }

    tims_dbgdetail("[TCP]  %8x --(%04d)--> %8x, recv head (%u bytes), data (%u bytes)\n",
                   recvMsg->src, recvMsg->type, recvMsg->dest, TIMS_HEADLEN,
                   recvMsg->msglen - TIMS_HEADLEN);


  } else {

    tims_dbgdetail("[TCP]  %8x --(%04d)--> %8x, recv head (%u bytes)\n",
                   recvMsg->src, recvMsg->type, recvMsg->dest, recvMsg->msglen);

  }
  return 0;
}

int sndPipeTimsMsg(timsMsgHead* sndMsg)
{
    int ret;
    sem_wait(&pipeSendSem);

    tims_dbgdetail("[PIPE] %8x --(%04d)--> %8x, sending %u bytes\n",
                   sndMsg->src, sndMsg->type, sndMsg->dest, sndMsg->msglen);

    ret = write(pipeClientToTimsFd, sndMsg, sndMsg->msglen);
    if (ret != sndMsg->msglen)
    {
        tims_print("[PIPE] %8x --(%04d)--> %8x,, (%d bytes), send ERROR, "
                   "code = %d\n", sndMsg->src, sndMsg->type, sndMsg->dest,
                   sndMsg->msglen, ret);
        sem_post(&pipeSendSem);
        return -1;
    }
    sem_post(&pipeSendSem);
    return 0;
}

int recvPipeTimsMsg(timsMsgHead* recvMsg)
{
    int msglen;
    int cpybytes = maxMsgSize;

    // read head
    msglen = read(pipeTimsToClientFd, (void*)recvMsg, cpybytes);
    if (msglen < TIMS_HEADLEN)  // error code or too small for message head
    {
        if ((int)msglen == -1) // special error code
        {
            msglen = errno;
        }
        tims_print("[PIPE] recv ERROR, code = %d \n", (int)msglen);
        return -1;
    }
    tims_parse_head_byteorder(recvMsg);

    if (recvMsg->msglen != msglen) // invalid size
    {
        tims_print("[PIPE] %8x --(%04d)--> %8x, recv ERROR, %d bytes msglen in "
                   "head, %d bytes received\n", recvMsg->src, recvMsg->type,
                   recvMsg->dest, recvMsg->msglen, msglen);
        return -1;
    }

    tims_dbgdetail("[PIPE] %8x --(%04d)--> %8x, recv message (%u bytes)\n",
                   recvMsg->src, recvMsg->type, recvMsg->dest, recvMsg->msglen);

    return 0;
}

void tcpRecvTask(void *arg)
{
  int ret;

  timsMsgHead  connMsg;
  timsMsgHead  replyMsg;

  signal(SIGHUP,  signal_handler);
  signal(SIGINT,  signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGPIPE, signal_handler);

  tims_dbg("[TCP_TASK] init\n");

  tcpRecvMsg = malloc(maxMsgSize);
  if (!tcpRecvMsg) {
    tims_print("[TCP_TASK] ERROR: can't allocate memory for tcpRecvMsg\n");
    terminate = 1;
    return;
  }
  init_flags |= TIMS_CLIENT_TCP_RECV_MSG;

  while (!terminate) {

    if (tcpSocket == -1) {

      // try to connect to TCP Tims Message Router
      tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
      if (tcpSocket == -1) {

        tims_print("[TCP_TASK] ERROR: can't create TCP/IP socket\n");
        sleep(5);

      } else if (connect(tcpSocket, (struct sockaddr *)&tcpAddr, sizeof(tcpAddr)) == -1) {

        tims_print("[TCP_TASK]: Can't connect to TcpTimsMsgRouter, (%s)\n", strerror(errno));
        close(tcpSocket);
        tcpSocket = -1;
        sleep(5);

      } else {  // connected to TCPTimsMsgRouter

        init_flags |= TIMS_CLIENT_TCP_CONNECT;

        tims_fillhead(&connMsg, TIMS_MSG_ROUTER_CONNECTED, 0, 0, 0, 0, 0, TIMS_HEADLEN);

        if (sndPipeTimsMsg(&connMsg) != 0) {
          tims_dbgdetail("[TCP_TASK] ERROR: Can't send connect message to TIMS\n");
          close(tcpSocket);
          tcpSocket = -1;
          init_flags &= ~TIMS_CLIENT_TCP_CONNECT;
          sleep(5);

        } else {

          tims_print("[TCP_TASK] connected to TcpTimsMessageRouter\n");

        }
      }

    } else { // tcpSocket != -1

      // handle incoming Tims Message from TcpTimsMsgRouter

      ret = recvTcpTimsMsg(tcpRecvMsg);
      if (ret) {

        tims_print("[TCP_TASK] connection to TcpTimsMsgRouter closed\n");
        close(tcpSocket);
        tcpSocket = -1;
        init_flags &= ~TIMS_CLIENT_TCP_CONNECT;
        sleep(5);

      } else {

        if ((tcpRecvMsg->dest == 0) &
            (tcpRecvMsg->src == 0) &
            (tcpRecvMsg->type == TIMS_MSG_ROUTER_GET_STATUS)) {

          // reply to lifesign
          tims_fillhead(&replyMsg, TIMS_MSG_OK, 0, 0, tcpRecvMsg->priority,
                         0, 0, TIMS_HEADLEN);

          sndTcpTimsMsg(&replyMsg);

        } else if(sndPipeTimsMsg(tcpRecvMsg) != 0) {

          if (tcpRecvMsg->type > 0) { // send reply if connection is not available

            tims_fillhead(&replyMsg, TIMS_MSG_NOT_AVAILABLE, tcpRecvMsg->src,
                           tcpRecvMsg->dest, tcpRecvMsg->priority, tcpRecvMsg->seq_nr,
                           0, TIMS_HEADLEN);

            sndTcpTimsMsg(&replyMsg);
          }
        }
      }
    }
    sem_wait(&watchdogSem);
    watchdog = 0;
    sem_post(&watchdogSem);
  }

  tims_dbgdetail("[TCP_TASK] terminate \n");

  if (init_flags & TIMS_CLIENT_TCP_RECV_MSG) {
    if (tcpRecvMsg) {
      free(tcpRecvMsg);
      tims_dbgdetail("free tcp receive buffer (task)\n");
      init_flags &= ~TIMS_CLIENT_TCP_RECV_MSG;
    }
  }
  return;

}

void pipeRecvTask(void)
{
    timsMsgHead  replyMsg;

    tims_dbg("[PIPE_TASK] init\n");
    tims_dbg("[PIPE_TASK] MaxMsgSize %u kByte\n", maxMsgSize/1024);

    pipeRecvMsg = (timsMsgHead *)malloc(maxMsgSize);
    if (!pipeRecvMsg)
    {
        tims_print("[PIPE_TASK] ERROR: can't allocate memory for pipeRecvMsg\n");
        return;
    }
    init_flags |= TIMS_CLIENT_TCP_RECV_MSG;

    while (!terminate) // handle incoming messages from TIMS
    {
        if (recvPipeTimsMsg(pipeRecvMsg) == 0)
        {
            if ((pipeRecvMsg->type == TIMS_MSG_ROUTER_GET_CONFIG) &&
                (pipeRecvMsg->dest == 0) &&
                (pipeRecvMsg->src  == 0))
            {
                if (init_flags & TIMS_CLIENT_CONFIG_MSG)
                {
                    sndPipeTimsMsg((timsMsgHead*)configMsg);
                }
                else
                {
                    tims_fillhead(&replyMsg, TIMS_MSG_ROUTER_CONFIG, 0, 0,
                                  pipeRecvMsg->priority, 0, 0, TIMS_HEADLEN);
                    sndPipeTimsMsg(&replyMsg);
                }
            }
            else if (sndTcpTimsMsg(pipeRecvMsg) != 0)
            {
                if (pipeRecvMsg->type > 0) // send reply if connection is not available
                {
                    tims_fillhead(&replyMsg, TIMS_MSG_NOT_AVAILABLE,
                                  pipeRecvMsg->src, pipeRecvMsg->dest,
                                  pipeRecvMsg->priority, pipeRecvMsg->seq_nr,
                                  0, TIMS_HEADLEN);
                    sndPipeTimsMsg(&replyMsg);
                }
            }
        }
        else
        {
           if (!terminate)
               sleep(5);
           else
               break;
        }
    }

    if (init_flags & TIMS_CLIENT_PIPE_RECV_MSG)
    {
        free(pipeRecvMsg);
        pipeRecvMsg = NULL;
        init_flags &= ~TIMS_CLIENT_PIPE_RECV_MSG;
    }

    tims_dbgdetail("[PIPE_TASK] terminate\n");
}

int readConfigFile(char* filename)
{
    FILE* fp;
    int   line = 0;
    char  buffer[80];
    int   mbx;
    char  ip[80];
    unsigned int msglen;

    tims_dbg(NAME ": Read cfg file \"%s\"\n", filename);

    if ((fp = fopen(filename, "r")) == NULL)
    {
        tims_print("error: Can't open cfg file \"%s\"\n", filename);
        return -1;
    }

    configMsg->num = 0;

    while (fgets(buffer, 80, fp) != NULL)
    {
        line++;
        if (configMsg->num >= MAX_CONFIG_NUM)
        {
            tims_print("ERROR: Config file has got more than MAX_CONFIG_NUM(%i) "
                       "entries\n", MAX_CONFIG_NUM);
            fclose(fp);
            return -1;
        }

        if (sscanf(buffer, "%i %s", &mbx, ip) != 2)
        {
            continue;
        }

        if (mbx <= 0)
        {
            tims_print("ERROR: MBX not valid (cfg file line %i)\n", line);
            fclose(fp);
            return -1;
        }

        configMsg->mbx_route[configMsg->num].mbx = mbx;

        if ((configMsg->mbx_route[configMsg->num].ip = inet_addr(ip)) ==
             INADDR_NONE)
        {
            tims_print("error: IP not valid (cfg file line %i)\n", line);
            fclose(fp);
            return -1;
        }

        tims_dbgdetail("Route MBX %X to IP %s\n", mbx, ip);
        configMsg->num++;
    }

    fclose(fp);

    // init head
    msglen = sizeof(timsMsgRouter_ConfigMsg) +
                    configMsg->num * sizeof(timsMsgRouter_MbxRoute);

    tims_fillhead((timsMsgHead*)configMsg, TIMS_MSG_ROUTER_CONFIG,
                  0, 0, 0, 0, 0, msglen);

    return 0;
}

int init(char* filename)
{
    int  ret;
    char pipePathname[20];

    terminate = 0;

    //
    // init semaphore
    //

    ret = sem_init(&tcpSendSem, 0, 1);
    if (ret)
    {
        tims_print("[INIT] ERROR: can't create tcpSendSem\n");
        goto init_error;
    }
    tims_dbgdetail("[INIT] tcpSendSem created\n");
    init_flags |= TIMS_CLIENT_SEM_TCP_SEND;

    if (terminate)
        goto init_error; // break, if signal handler was called

    ret = sem_init(&pipeSendSem, 0, 1);
    if (ret)
    {
        tims_print("[INIT] ERROR: can't create pipeSendSem\n");
        goto init_error;
    }
    tims_dbgdetail("[INIT] pipeSendSem created\n");
    init_flags |= TIMS_CLIENT_SEM_PIPE_SEND;

    if (terminate)
        goto init_error; // break, if signal handler was called

    if (sem_init(&watchdogSem, 0, 1) < 0)
    {
        tims_print("[INIT] ERROR: can't create watchdogSem\n");
        goto init_error;
    }
    tims_dbgdetail("[INIT] watchdogSem created\n");
    init_flags |= TIMS_CLIENT_SEM_WATCHDOG;

    if (terminate)
        goto init_error; // break, if signal handler was called

    tims_dbgdetail("[INIT] now the Pipes to TIMS will be opened.\n"
                   "                      If the TIMS kernel module isn't loaded into the kernel, \n"
                   "                      this INIT process will blocking until the module is loaded ...\n");

    //
    // open PIPESs (blocking)
    //

    snprintf(pipePathname, 20, "/dev/rtp%i", PIPE_TIMS_TO_CLIENT);
    pipeTimsToClientFd = open(pipePathname, O_RDONLY);
    if (pipeTimsToClientFd < 0)
    {
        tims_print("[INIT] ERROR: can't open pipe from TIMS %s\n", pipePathname);
        ret = pipeTimsToClientFd;
        goto init_error;
    }
    tims_dbgdetail("[INIT] pipe from TIMS (/dev/rtp%i) opened\n", PIPE_TIMS_TO_CLIENT);
    init_flags |= TIMS_CLIENT_PIPE_TIMS_CLIENT;

    if (terminate)
        goto init_error; // break, if signal handler was called

    snprintf(pipePathname, 20, "/dev/rtp%i", PIPE_CLIENT_TO_TIMS);
    pipeClientToTimsFd = open(pipePathname, O_WRONLY);
    if (pipeClientToTimsFd < 0)
    {
        tims_print("[INIT] ERROR: can't open pipe to TIMS %s\n", pipePathname);
        ret = pipeClientToTimsFd;
        goto init_error;
    }
    tims_dbgdetail("[INIT] pipe to TIMS (/dev/rtp%i) opened\n",
                   PIPE_CLIENT_TO_TIMS);
    init_flags |= TIMS_CLIENT_PIPE_CLIENT_TIMS;

    if (terminate)
        goto init_error; // break, if signal handler was called

    //
    // create tasks
    //

    ret = pthread_create(&tcpRecvThread, NULL, (void *)tcpRecvTask, NULL);
    if (ret)
    {
        tims_print("[INIT] ERROR: can't create TCP/IP receive thread\n");
        goto init_error;
    }
    tims_dbgdetail("[INIT] TCP/IP receive thread created\n");
    init_flags |= TIMS_CLIENT_THREAD_TCPRECV;

    if (terminate)
        goto init_error; // break, if signal handler was called

    ret = pthread_create(&watchdogThread, NULL, watchdogTask, NULL);
    if (ret)
    {
        tims_print("[INIT] ERROR: can't create watchdog thread\n");
        goto init_error;
    }
    tims_dbgdetail("[INIT] watchdogThread created\n");
    init_flags |= TIMS_CLIENT_THREAD_WATCHDOG;

    //
    // read rtnet config file
    //

    if (strlen(filename) > 0)
    {
        // read router configuration file
        if ((configMsg = malloc(sizeof(timsMsgRouter_ConfigMsg) +
                         MAX_CONFIG_NUM * sizeof(timsMsgRouter_MbxRoute)))
                         == NULL) {
            tims_print("[INIT] ERROR: can't allocate memory for config message\n");
            ret = -ENOMEM;
            goto init_error;
        }
        tims_dbgdetail("[INIT] buffer for config message allocated\n");
        init_flags |= TIMS_CLIENT_CONFIG_MSG;

        if (terminate)
            goto init_error; // break, if signal handler was called

        if (readConfigFile(filename) != 0)
        {
            ret = -1;
            tims_print("[INIT] ERROR: can't read config file\n");
            goto init_error;
        }
        tims_dbgdetail("[INIT] config file has been read successfully\n");
    }
    tims_dbgdetail("[INIT] exit\n");
    return 0;

init_error:
    cleanup();
    return ret;
}

int main(int argc, char* argv[])
{
    char filename[80];
    char ip[16];
    int  port;
    int  opt;
    int  ret;

    init_flags = 0;

    // read parameter
    strncpy(filename, DEFAULT_CFG, 80);
    strncpy(ip, DEFAULT_IP, 16);
    port       = DEFAULT_PORT;
    maxMsgSize = DEFAULT_MAX * 1024;

    while ((opt = getopt(argc, argv, "i:p:m:c:h:l:")) != -1)
    {
        switch (opt)
        {
            case 'l':
                sscanf(optarg, "%i", &loglevel);
                printf(NAME": opt -l loglevel: %i\n", loglevel);
                break;

            case 'i':
                strncpy(ip, optarg, 16);
                tims_dbgdetail("opt -i ip: %s\n", ip);
                break;

            case 'p':
                sscanf(optarg, "%i", &port);
                tims_dbgdetail("opt -p port: %i\n", port);
                break;

            case 'm':
                sscanf(optarg, "%i", &maxMsgSize);
                tims_dbgdetail("opt -m maxMessageSize: %i kByte\n", maxMsgSize);
                if (!maxMsgSize || maxMsgSize < 0)
                {
                    printf(NAME": opt -m invalid: %i\n", maxMsgSize);
                    return -1;
                }
                maxMsgSize *= 1024;
                break;

            case 'c':
                strncpy(filename, optarg, 80);
                tims_dbgdetail("opt -c config filename: %s\n", filename);
                break;

            case 'h':

            default:
                printf("\n"
                "The TimsMsg is a Linux usermode programm that transferes Tims Messages\n"
                "between the TIMS kernel module tims.o and the TcpTimsMsgRouter (former TimsMsgGateway)\n"
                "\n"
                "-i IP address of the TcpTimsMsgRouter\n"
                "   (default 127.0.0.1 localhost)\n"
                "-p port of the TcpTimsMsgRouter\n"
                "   (default 2000)\n"
                "-m maxMessageSize in kBytes\n"
                "   (default 256 kByte)\n"
                "-c config filename\n"
                "   only needed for distributed real-time communication via RTnet\n"
                "-l log level\n"
                "   debug log level, 0 = silent, 1 = some important messages, 2 = verbose\n");

                return -1;
        }
    }

    // parse TCPTimsMsgRouter ip address and port
    if ((tcpAddr.sin_addr.s_addr = inet_addr(ip)) == INADDR_NONE)
    {
        tims_print("error: Ip %s not valid\n", ip);
        return -1;
    }

    if ((port < 0x400) | ( port > 0xffff))
    {
        tims_print("error: Port %i not valid\n", port);
        return -1;
    }

    tcpAddr.sin_port   = htons((unsigned short)port);
    tcpAddr.sin_family = AF_INET;
    bzero(&(tcpAddr.sin_zero), 8);

    signal(SIGHUP,  signal_handler);
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, signal_handler);

    ret = init(filename);
    if (ret)
        return ret;

    tims_dbg("TcpTimsMsgRouter IP %s port %i\n", ip, port);
    pipeRecvTask();
    cleanup();
    return 0;
}
