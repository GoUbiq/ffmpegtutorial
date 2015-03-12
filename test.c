#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <x264.h>
#include <stdio.h>

// void show_av_device() {

//    ifmt->get_device_list(pFormatCtx, device_list);
//    printf("Device Info=============\n");
//    //avformat_open_input(&pFormatCtx,"video=Capture screen 0",ifmt,&options);
//    printf("===============================\n");
// }

int main(int argc, char *argv[]) {
  AVFormatContext   *pFormatCtx = NULL;
  int               i, videoStream;
  AVCodecContext    *pCodecCtx = NULL;
  AVCodec           *pCodec = NULL;
  AVDictionary      *options = NULL;
  // AVDeviceInfoList *device_list = NULL;
  AVInputFormat     *ifmt;
  const char        *name;
  AVFrame           *pFrame = NULL;
  AVFrame           *pFrameYUV = NULL;
  int               numBytes;
  // AVFrame *pFrameRGB = NULL;
  // uint8_t *buffer = NULL;
  // int numBytes;
  // struct SwsContext *swsCtx = NULL;
  // int frameFinished;
  // AVPacket packet;

  name = "avfoundation";
  av_register_all();
  avdevice_register_all();
  pFormatCtx = avformat_alloc_context();

  // av_dict_set(&options, "list_devices", "true", 0);
  av_dict_set(&options, "pixel_format", "uyvy422", 0);
  av_dict_set(&options, "probesize", "7000000", 0);

  ifmt = av_find_input_format(name);
  if (avformat_open_input(&pFormatCtx, "Capture screen 0", ifmt, &options) != 0) {
    printf("Could not load the context for the input device\n");
    return -1;
  }
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
    printf("Could not find stream info for screen\n");
    return -1;
  }
  av_dump_format(pFormatCtx, 0, "Capture screen 0", 0);
  // pFormatCtx->streams is an array of pointers of size pFormatCtx->nb_streams
  // so loop through each stream and find a video stream
  videoStream = -1;
  for (i = 0; i < pFormatCtx->nb_streams; i++) {
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStream = i;
      break;
    }
  }
  if (videoStream == -1) {
    printf("no video stream found\n");
    return -1;
  }

  pCodecCtx = pFormatCtx->streams[videoStream]->codec;
  pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
  if (pCodec == NULL) {
    fprintf(stderr, "Unsupported codec\n");
    return -1;
  }
  // open codec
  if (avcodec_open2(pCodecCtx, pCodec, NULL) > 0) {
    return -1;  // couldn't open codec
  }
  // Allocate video frame
  pFrame = av_frame_alloc();
  numBytes = avpicture_get_size(PIX_FMT_UYVY422, pCodecCtx->width, pCodecCtx->height);
  uint8_t *buffer = (uint8_t *)av_malloc(
    numBytes*sizeof(uint8_t));

  int ret = avpicture_fill((AVPicture *)pFrame, buffer, PIX_FMT_UYVY422,
                pCodecCtx->width, pCodecCtx->height);

  printf("%d is return val for fill\n", ret);

  // if (avpicture_fill((AVPicture *)pFrameYUV, buffer, PIX_FMT_UYVY422,
  //               pCodecCtx->width, pCodecCtx->height) != 0) {

  // }



  return 1;

  // convert frame from native format to 24 bit RGB(PPM format)
  // also allocate a frame for the converted frame
}
