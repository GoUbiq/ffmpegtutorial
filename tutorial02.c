// tutorial02.c
// A pedagogical video player that will stream through every video frame as fast as it can.
//
// Code based on FFplay, Copyright (c) 2003 Fabrice Bellard, 
// and a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1
// With updates from https://github.com/chelyaev/ffmpeg-tutorial
// Updates tested on:
// LAVC 54.59.100, LAVF 54.29.104, LSWS 2.1.101, SDL 1.2.15
// on GCC 4.7.2 in Debian February 2015
//
// Use
// 
// gcc -o tutorial02 tutorial02.c -lavformat -lavcodec -lswscale -lavutil -lz -lm `sdl-config --cflags --libs`

// to use gdb
// gcc -g -o tutorial02 tutorial02.c -lavformat -lavcodec -lswscale -lavutil -lz -lm `sdl-config --cflags --libs`

//gdb toutorial02
// inside gdb, run videoview.mp4
// to build (assuming libavformat and libavcodec are correctly installed, 
// and assuming you have sdl-config. Please refer to SDL docs for your installation.)
//
// Run using
// tutorial02 myvideofile.mpg
//
// to play the video stream on your screen.

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

int main(int argc, char *argv[]) {
  AVFormatContext *pFormatCtx = NULL;
  int             i, videoStream;
  AVCodecContext  *pCodecCtxOrig = NULL;
  AVCodecContext  *pCodecCtx = NULL;
  AVCodec         *pCodec = NULL;
  AVFrame         *pFrame = NULL;
  AVPacket        packet;
  int             frameFinished;
  struct SwsContext *sws_ctx = NULL;

  SDL_Surface     *screen;
  SDL_Overlay     *bmp = NULL;
  SDL_Rect        rect;
  SDL_Event       event;



  if(argc < 2) {
    fprintf(stderr, "Usage: test <file>\n");
    exit(1);
  }
  // Register all formats and codecs
  av_register_all();

  
  //include SDL libraries and initialize SDL
  //SDL_INIT() tells the library what features we will use
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }

  //Open video file
  if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
    return -1;

  //rettrieve stream information
  if (avformat_find_stream_info(pFormatCtx, NULL) > 0)
    return -1;
  //dump info about file into stderr

  av_dump_format(pFormatCtx, 0, argv[1], 0);

  //find first video stream

  videoStream = -1;

  for(i=0; i<pFormatCtx->nb_streams; i++)
    if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStream = i;
      break;
    }
  if(videoStream == -1)
    return -1;

  //get a pointer to the codec context
  pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;

  pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);

  if(pCodec == NULL) {
    fprintf(stderr, "Unsupported codec\n");
    return -1;
  }

  //copy context
  pCodecCtx = avcodec_alloc_context3(pCodec);
  if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
    fprintf(stderr, "Couldn't copy codec context\n");
    return -1;
  }

  //open codec
  if(avcodec_open2(pCodecCtx, pCodec, NULL) > 0) {
    return -1; //couoldn't open codec
  }

  //allocate a video frame
  pFrame = av_frame_alloc();

  //Make a screen for video
#ifndef __DARWIN__
        screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
#else
        screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
#endif
  if(!screen) {
    fprintf(stderr, "SDL: Could not set video mode - exiting\n");
    exit(1);
  }

  //Now create a YUV overlay on that screen to input video to it
  //setup SWSContext to convert image data to YUV420
  bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,
                            SDL_YV12_OVERLAY, screen);
  //we use YUV12 to display the image and get YUV420 data from ffmpeg

  //initi sws context for software scaling
  sws_ctx = sws_getContext(pCodecCtx->width,
    pCodecCtx->height,
    pCodecCtx->pix_fmt,
    pCodecCtx->width,
    pCodecCtx->height,
    PIX_FMT_YUV420P,
    SWS_BILINEAR,
    NULL,
    NULL,
    NULL);

  //Read frames and save first five frames to disk

  while(av_read_frame(pFormatCtx, &packet) >= 0) {
    //check if its a packet from the video stream
    if(packet.stream_index == videoStream) {
      ///decode the video frame
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
      if(frameFinished) {
        SDL_LockYUVOverlay(bmp);

        //make an AVPicture struct and set its data pointers and linesize to our YUV overlay "bmp"

        AVPicture pict;
        pict.data[0] = bmp->pixels[0];
        pict.data[1] = bmp->pixels[1];
        pict.data[2] = bmp->pixels[2];

        pict.linesize[0] = bmp->pitches[0];
        pict.linesize[1] = bmp->pitches[1];
        pict.linesize[2] = bmp->pitches[2];

        //now convert the image into a format that YUV o
        sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, 
          pFrame->linesize, 0, pCodecCtx->height,
          pict.data, pict.linesize);

        SDL_UnlockYUVOverlay(bmp);

        //Draw the image
        rect.x = 0;
        rect.y = 0;
        rect.w = pCodecCtx->width;
        rect.h = pCodecCtx->height;
        SDL_DisplayYUVOverlay(bmp, &rect);
      }
    }
    // Free the packet allocated by av_read_frame
    av_free_packet(&packet);
    SDL_PollEvent(&event);
    switch(event.type) {
    case SDL_QUIT:
      exit(0);
      break;
    default:
      break;
    }
  }

  //free the YUV frame
  av_frame_free(&pFrame);
  avcodec_close(pCodecCtx);
  avcodec_close(pCodecCtxOrig);

  //close the video input
  avformat_close_input(&pFormatCtx);
  return(1);
}






