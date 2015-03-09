// tutorial01.c
// Code based on a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1
// With updates from https://github.com/chelyaev/ffmpeg-tutorial
// Updates tested on:
// LAVC 54.59.100, LAVF 54.29.104, LSWS 2.1.101 
// on GCC 4.7.2 in Debian February 2015

// A small sample program that shows how to use libavformat and libavcodec to
// read video from a file.
//
// Use
//
// gcc -o tutorial01 tutorial01.c -lavformat -lavcodec -lswscale -lavutil -lz
//
// to build (assuming libavformat and libavcodec are correctly installed
// your system).
//
// Run using
//
// tutorial01 myvideofile.mpg
//
// to write the first five frames from "myvideofile.mpg" to disk in PPM
// format.

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;

  //open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile = fopen(szFilename, "wb");
  if(pFile==NULL)
    return;

  //write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  //write pixel data
  for(y=0; y<height; y++) {
    fwrite(pFrame->data[0]+y*pFrame->linesize[0],1,width*3, pFile);
  }
  fclose(pFile);
}

int main(int argc, char *argv[]) {
   //registers all available codecs and formats with the library
  av_register_all();

  AVFormatContext   *pFormatCtx = NULL;
  AVCodecContext    *pCodecCtxOrig = NULL;
  AVCodecContext    *pCodecCtx = NULL;
  int               i, videoStream;
  AVCodec           *pCodec = NULL;
  AVFrame           *pFrame = NULL;
  AVFrame           *pFrameRGB = NULL;
  uint8_t           *buffer=NULL;
  int               numBytes;
  struct SwsContext *swsCtx = NULL;
  int               frameFinished;
  AVPacket          packet;
   
   //open the file now
  //read the file header and store info about the file format in 
  //pFormatCtx which is an AVFormatCOntext struct.
  if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {
    return -1; //couldn't open file
  }

  //Retrieve stream information. populates pFormatCtx->streams
  if(avformat_find_stream_info(pFormatCtx, NULL) < 0) {
    return -1;
  }
  //Dump information about file onto std error
  av_dump_format(pFormatCtx, 0, argv[1], 0);

  //pFormatCtx->streams is an array of pointers of size pFormatCtx->nb_streams
  //so loop through each stream and find a video stream
  videoStream = -1;
  for(i=0; i<pFormatCtx->nb_streams; i++) {
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
      videoStream=i;
      break;
    }
  }
  if(videoStream == -1) {
    return -1; //didn't find a video stream
  }

  //get a pointer to the codec context for the video stream
  pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
  //codec context is the stream information about the codec. Its a pointer to the 
  //stream information. Now find the actual codec and open it

  pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
  if(pCodec == NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;
  }

  //don't use the AVCodecContext from the video stream directly. create a copy 
  //of the context

  pCodecCtx = avcodec_alloc_context3(pCodec);
  if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
    fprintf(stderr, "Couldn't copy context");
    return -1; //error copying codec
  }
  //Open the codec now
  if(avcodec_open2(pCodecCtx, pCodec, NULL) > 0) {
    return -1; //couldn't open codec
  }

  //Allocate video frame
  pFrame = av_frame_alloc();

  //convert frame from native format to 24 bit RGB(PPM format)
  //also allocate a frame for the converted frame
  pFrameRGB = av_frame_alloc();
  if(pFrameRGB==NULL)
    return -1;
  //now need to store the raw data whose size can be gotten from 
  //avpicture_get_size
  numBytes = avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width,
                                pCodecCtx->height);
  buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

  //use avpicture_fill to associate frame with newly allocated buffer

  avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24,
                  pCodecCtx->width, pCodecCtx->height);

  //read from stream now
  //initialize sws context for software scaling
  swsCtx = sws_getContext(pCodecCtx->width,
    pCodecCtx->height,
    pCodecCtx->pix_fmt,
    pCodecCtx->width,
    pCodecCtx->height,
    PIX_FMT_RGB24,
    SWS_BILINEAR,
    NULL,
    NULL,
    NULL);
  i=0;
  //
  while(av_read_frame(pFormatCtx, &packet)>=0) {
    //is this a packet from the video stream?
    if(packet.stream_index==videoStream) {
      //decode video frame
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
      if(frameFinished) {
        //Convert image to RGB
        sws_scale(swsCtx, (uint8_t const * const *)pFrame->data,
          pFrame->linesize, 0, pCodecCtx->height,
          pFrameRGB->data, pFrameRGB->linesize);

        if(++i<=5) {
          SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
        }
      }
    }
    //Free the packet allocated by av_read_frame
    av_free_packet(&packet);
  }

  //free the RGB image 
  av_free(buffer);
  av_frame_free(&pFrameRGB);

  //free the YUV frame
  av_free(pFrame);

  //close the codecs
  avcodec_close(pCodecCtx);
  avcodec_close(pCodecCtxOrig);

  //close the video file
  avformat_close_input(&pFormatCtx);
  return 0;
}
