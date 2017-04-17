#include "pthread.h"
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#define RTP_HEADER_LEN_MAX (1 + 4)
#define RTP_KEY_FRAME (2)
#define RTP_START_FRAME (1)
#define RTP_CONT_FRAME (0)
#define RTP_PACKET_SIZE (1140)

#define LOGD(...) printf(__VA_ARGS__)
#define LOGV(...) printf(__VA_ARGS__)
#define LOGE(...) printf(__VA_ARGS__)


#define LITTLE_TO_BIG_END_INT(nInt) ( (nInt & 0xFF000000) >> 24      \
    | (nInt & 0xFF0000) >> 8 | (nInt & 0xFF00) << 8                  \
    | (nInt & 0xFF) << 24)

//#define DUMP_TO_FILE
#define OVER_UDP

#define SELF_IP "192.168.43.125" //"10.0.0.5"

typedef struct
{
    pthread_t hUdpReceiver;
    pthread_t hMedia;
    int nRecvSo;
    int FD[2];
    struct sockaddr_in selfSoAddr;
    #ifdef DUMP_TO_FILE
    FILE* fpOut;
    #endif /**< DUMP_TO_FILE */
    GstPipeline* mpPipeline;
    GMainLoop* mpMainLoop;
    GstAppSrc* mpStreamSrc;
}tReceiver;

void* udp_receiver(void* p)
{
    tReceiver* pR = (tReceiver*)p;
    int nBytesRecvd = 0;
    char* pcDataRead;
    int nOptVal = 1;
    int nRet;
    struct sockaddr_in srcSoAddr = {0};
    unsigned int nSrcSoAddrLen = sizeof(srcSoAddr);

    if(!pR)
        goto cleanup;
    pR->nRecvSo = socket(AF_INET, 
    #ifdef OVER_UDP
    SOCK_DGRAM, IPPROTO_UDP
    #else
    SOCK_STREAM, IPPROTO_TCP
    #endif
    );
    setsockopt(pR->nRecvSo, SOL_SOCKET, SO_REUSEADDR, &nOptVal, sizeof(nOptVal));
    pR->selfSoAddr.sin_family = AF_INET;
    pR->selfSoAddr.sin_port = htons(5600);
    pR->selfSoAddr.sin_addr.s_addr = inet_addr(SELF_IP);
    nRet = bind(pR->nRecvSo, (const struct sockaddr *)(&pR->selfSoAddr), sizeof(pR->selfSoAddr));
    LOGV("bind %s\n", nRet == -1 ? "failed" : "success");
    #ifndef OVER_UDP
    listen(pR->nRecvSo, 0);
    pR->nRecvSo = accept(pR->nRecvSo, (struct sockaddr *)&srcSoAddr, &nSrcSoAddrLen);
    LOGV("accept returned=%d\n", pR->nRecvSo);
    #endif
    while(1)
    {
        /** Port 5600
         * listen for incoming packets
         * packet format:
         * 1 Byte | 4 Byte | DATA -- together 1140B
         * 
         * */
        pcDataRead = malloc(RTP_PACKET_SIZE);
        LOGD("wait on socket\n");
        nBytesRecvd = recv(pR->nRecvSo, pcDataRead, RTP_PACKET_SIZE, MSG_WAITALL);
        if(nBytesRecvd != 1140)
        {
            LOGE("nBytesRecvd=%d\n", nBytesRecvd);
            goto cleanup;
        }
        nBytesRecvd = write(pR->FD[1], &pcDataRead, sizeof(pcDataRead));
        if(nBytesRecvd <= 0)
        {
            LOGE("write failed wn=%d\n", nBytesRecvd);
            goto cleanup;
        }
    }

    cleanup:

    return NULL;
}

int media_player_launch(tReceiver* pR)
{
    int nRet = 0;
    GError* pError;
    pR->mpPipeline = (GstPipeline*)gst_parse_launch("appsrc name=stream_src ! video/x-h264 ! h264parse ! avdec_h264 ! queue ! videorate ! video/x-raw, framerate=25/1 ! autovideosink sync=false ", &pError);
    pR->mpStreamSrc = (GstAppSrc*)gst_bin_get_by_name((GstBin*)pR->mpPipeline, "stream_src");
    gst_element_set_state((GstElement*)pR->mpPipeline, GST_STATE_PAUSED);
    gst_element_set_state((GstElement*)pR->mpPipeline, GST_STATE_PLAYING);
    pR->mpMainLoop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(pR->mpMainLoop);
    return nRet;
}

void release_buffer_notify(gpointer p)
{
    if(p)
    {
        free(p);
    }
}

void* media_player(void* p)
{
    tReceiver* pR = (tReceiver*)p;
    char* pcDataRead = NULL;
    int nBytesRecvd;
    int bGotRtpStart = 0;
    int nCurrentFrameLen = 0;
    void* pMediaData = NULL;
    int j = 0;

    GstBuffer* buffer;

    if(!pR)
        goto cleanup;
    
    while(1)
    {
        int i = 0;
        int nDataRecvNow = 0;
        pcDataRead = NULL;
        LOGV("wait on pipe\n");
        nBytesRecvd = read(pR->FD[0], &pcDataRead, sizeof(pcDataRead));
        if(nBytesRecvd <= 0 || !pcDataRead)
        {
            LOGE("did not receive proper data pointer %d\n", nBytesRecvd);
        }
        /** ignore until pcDataRead[0] = RTP_START_FRAME */
        if(bGotRtpStart == 0 && pcDataRead[0] != RTP_START_FRAME)
        {
            free(pcDataRead);
            pcDataRead = NULL;
            continue;
        }
        bGotRtpStart = 1;
        i++;
        if(pcDataRead[0] == RTP_START_FRAME)
        {
            j = 0;
            #ifdef DUMP_TO_FILE
            if(pMediaData)
            {
                fwrite(pMediaData, nCurrentFrameLen, 1, pR->fpOut);
                free(pMediaData);
            }
            #else
            buffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, pMediaData, nCurrentFrameLen, 0, nCurrentFrameLen, pMediaData, release_buffer_notify);
            gst_app_src_push_buffer(pR->mpStreamSrc, buffer);
            #endif /**< DUMP_TO_FILE */
            nCurrentFrameLen =  LITTLE_TO_BIG_END_INT(*((int*)(&pcDataRead[1])));
            i += 4;
            LOGD("frame length=%d %x\n", nCurrentFrameLen, nCurrentFrameLen);
            pMediaData = malloc(nCurrentFrameLen);
        }
        LOGD("read from %d to end;length=%d\n", i, nCurrentFrameLen);
        memcpy(pMediaData + j, (void*)&pcDataRead[i], nDataRecvNow = (nCurrentFrameLen - j > (RTP_PACKET_SIZE - (i))) ? (RTP_PACKET_SIZE - (i)) : nCurrentFrameLen - j);
        j += nDataRecvNow;
        free(pcDataRead);
        pcDataRead = NULL;
    }

    cleanup:
    return NULL;
}

int main(int argc, char* argv[])
{
    tReceiver* pR = (tReceiver*)calloc(1, sizeof(tReceiver));
    int nRet = 0;
    if(!pR)
    {
        nRet = -1;
        goto cleanup;
    }
    #ifdef DUMP_TO_FILE
    pR->fpOut = fopen("sample.h264", "wb");
    #endif /**< DUMP_TO_FILE */
    gst_init(NULL, NULL);
    pipe(pR->FD);
    pthread_create(&pR->hUdpReceiver, NULL, udp_receiver, pR);
    pthread_create(&pR->hMedia, NULL, media_player, pR);
    media_player_launch(pR);
    getchar();
    cleanup:
    if(pR)
    {
        #ifdef DUMP_TO_FILE
        if(pR->fpOut)
            fclose(pR->fpOut);
        #endif /**< DUMP_TO_FILE */
        free(pR);
    }
    return nRet;
}
