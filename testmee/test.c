#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

/*
 compile using gcc -g -o stream test.c -lavformat -lavutil -lavcodec -lavdevice -lswscale
 */
// void show_av_device() {

//    inFmt->get_device_list(inFmtCtx, device_list);
//    printf("Device Info=============\n");
//    //avformat_open_input(&inFmtCtx,"video=Capture screen 0",inFmt,&inOptions);
//    printf("===============================\n");
// }

void AVFAIL (int code, const char *what) {
    char msg[500];
    av_strerror(code, msg, sizeof(msg));
    fprintf(stderr, "failed: %s\nerror: %s\n", what, msg);
    exit(2);
}

#define AVCHECK(f) do { int e = (f); if (e < 0) AVFAIL(e, #f); } while (0)
#define AVCHECKPTR(p,f) do { p = (f); if (!p) AVFAIL(AVERROR_UNKNOWN, #f); } while (0)

void registerLibs() {
    av_register_all();
    avdevice_register_all();
    avformat_network_init();
    avcodec_register_all();
}

int main(int argc, char *argv[]) {
    
    //conversion variables
    struct SwsContext *swsCtx = NULL;
    //input stream variables
    AVFormatContext   *inFmtCtx = NULL;
    AVCodecContext    *inCodecCtx = NULL;
    AVCodec           *inCodec = NULL;
    AVInputFormat     *inFmt = NULL;
    AVFrame           *inFrame = NULL;
    AVDictionary      *inOptions = NULL;
    const char *streamURL = "test.flv";
    const char *name = "avfoundation";
    
//    AVFrame           *inFrameYUV = NULL;
    AVPacket          inPacket;
    
    
    //output stream variables
    AVCodecContext    *outCodecCtx = NULL;
    AVCodec           *outCodec;
    AVFormatContext   *outFmtCtx = NULL;
    AVOutputFormat    *outFmt = NULL;
    AVFrame           *outFrameYUV = NULL;
    AVStream          *stream = NULL;
    
    int               i, videostream, ret;
    int               numBytes, frameFinished;
    
    registerLibs();
    inFmtCtx = avformat_alloc_context(); //alloc input context
    av_dict_set(&inOptions, "pixel_format", "uyvy422", 0);
    av_dict_set(&inOptions, "probesize", "7000000", 0);
    
    inFmt = av_find_input_format(name);
    ret = avformat_open_input(&inFmtCtx, "Capture screen 0:", inFmt, &inOptions);
    if (ret < 0) {
        printf("Could not load the context for the input device\n");
        return -1;
    }
    if (avformat_find_stream_info(inFmtCtx, NULL) < 0) {
        printf("Could not find stream info for screen\n");
        return -1;
    }
    av_dump_format(inFmtCtx, 0, "Capture screen 0", 0);
    // inFmtCtx->streams is an array of pointers of size inFmtCtx->nb_stream
    
    videostream = av_find_best_stream(inFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &inCodec, 0);
    if (videostream == -1) {
        printf("no video stream found\n");
        return -1;
    } else {
        printf("%s is inCodec\n", inCodec->long_name);
    }
    inCodecCtx = inFmtCtx->streams[videostream]->codec;
    // open codec
    if (avcodec_open2(inCodecCtx, inCodec, NULL) > 0) {
        printf("Couldn't open codec");
        return -1;  // couldn't open codec
    }

    
        //setup output params
    outFmt = av_guess_format(NULL, streamURL, NULL);
    if(outFmt == NULL) {
        printf("output format was not guessed properly");
        return -1;
    }
    
    if((outFmtCtx = avformat_alloc_context()) < 0) {
        printf("output context not allocated. ERROR");
        return -1;
    }
    
    printf("%s", outFmt->long_name);
    
    outFmtCtx->oformat = outFmt;
    
    snprintf(outFmtCtx->filename, sizeof(outFmtCtx->filename), streamURL);
    printf("%s\n", outFmtCtx->filename);

    outCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if(!outCodec) {
        printf("could not find encoder for H264 \n" );
        return -1;
    }
    
    stream = avformat_new_stream(outFmtCtx, outCodec);
    outCodecCtx = stream->codec;
    avcodec_get_context_defaults3(outCodecCtx, outCodec);
    
    outCodecCtx->codec_id = AV_CODEC_ID_H264;
    outCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    outCodecCtx->flags = CODEC_FLAG_GLOBAL_HEADER;
    outCodecCtx->width = inCodecCtx->width;
    outCodecCtx->height = inCodecCtx->height;
    outCodecCtx->time_base.den = 25;
    outCodecCtx->time_base.num = 1;
    outCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    outCodecCtx->gop_size = 50;
    outCodecCtx->bit_rate = 400000;

    //setup output encoders etc
    if(stream) {
        ret = avcodec_open2(outCodecCtx, outCodec, NULL);
        if (ret < 0) {
            printf("Could not open output encoder");
            return -1;
        }
    }
    
    if (avio_open(&outFmtCtx->pb, outFmtCtx->filename, AVIO_FLAG_WRITE ) < 0) {
        perror("url_fopen failed");
    }
    //avio_a
    avio_open_dyn_buf(&outFmtCtx->pb);
    ret = avformat_write_header(outFmtCtx, NULL);
    if (ret != 0) {
        printf("was not able to write header to output format");
        return -1;
    }
    unsigned char *pb_buffer;
    int len = avio_close_dyn_buf(outFmtCtx->pb, (unsigned char **)(&pb_buffer));
    avio_write(outFmtCtx->pb, (unsigned char *)pb_buffer, len);
    
    
    numBytes = avpicture_get_size(PIX_FMT_UYVY422, inCodecCtx->width, inCodecCtx->height);
    // Allocate video frame
    inFrame = av_frame_alloc();

    swsCtx = sws_getContext(inCodecCtx->width, inCodecCtx->height, inCodecCtx->pix_fmt, inCodecCtx->width,
                            inCodecCtx->height, PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    int frame_count = 0;
    while(av_read_frame(inFmtCtx, &inPacket) >= 0) {
        if(inPacket.stream_index == videostream) {
            avcodec_decode_video2(inCodecCtx, inFrame, &frameFinished, &inPacket);
            // 1 Frame might need more than 1 packet to be filled
            if(frameFinished) {
                outFrameYUV = av_frame_alloc();
                
                uint8_t *buffer = (uint8_t *)av_malloc(numBytes);
                
                int ret = avpicture_fill((AVPicture *)outFrameYUV, buffer, PIX_FMT_YUV420P,
                                         inCodecCtx->width, inCodecCtx->height);
                if(ret < 0){
                    printf("%d is return val for fill\n", ret);
                    return -1;
                }
                //convert image to YUV
                sws_scale(swsCtx, (uint8_t const * const* )inFrame->data,
                          inFrame->linesize, 0, inCodecCtx->height,
                          outFrameYUV->data, outFrameYUV->linesize);
                //outFrameYUV now holds the YUV scaled frame/picture
                outFrameYUV->format = outCodecCtx->pix_fmt;
                outFrameYUV->width = outCodecCtx->width;
                outFrameYUV->height = outCodecCtx->height;
                
                
                AVPacket pkt;
                int got_output;
                av_init_packet(&pkt);
                pkt.data = NULL;
                pkt.size = 0;

                outFrameYUV->pts = frame_count;
                
                ret = avcodec_encode_video2(outCodecCtx, &pkt, outFrameYUV, &got_output);
                if (ret < 0) {
                    fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
                    return -1;
                }
                
                if(got_output) {
                    if(stream->codec->coded_frame->key_frame) {
                        pkt.flags |= AV_PKT_FLAG_KEY;
                    }
                    pkt.stream_index = stream->index;
                    if(pkt.pts != AV_NOPTS_VALUE)
                        pkt.pts = av_rescale_q(pkt.pts, stream->codec->time_base, stream->time_base);
                    if(pkt.dts != AV_NOPTS_VALUE)
                        pkt.dts = av_rescale_q(pkt.dts, stream->codec->time_base, stream->time_base);
                    
                    if(avio_open_dyn_buf(&outFmtCtx->pb)!= 0) {
                        printf("ERROR: Unable to open dynamic buffer\n");
                    }
                    ret = av_interleaved_write_frame(outFmtCtx, &pkt);
                    unsigned char *pb_buffer;
                    int len = avio_close_dyn_buf(outFmtCtx->pb, (unsigned char **)&pb_buffer);
                    avio_write(outFmtCtx->pb, (unsigned char *)pb_buffer, len);

                } else {
                    ret = 0;
                }
                if(ret != 0) {
                    fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
                    exit(1);
                }
                
                fprintf(stderr, "encoded frame #%d\n", frame_count);
                frame_count++;
                
                av_free_packet(&pkt);
                av_frame_unref(outFrameYUV);
                av_frame_unref(inFrame);
                av_free(buffer);
                
            }
        }
        av_free_packet(&inPacket);
    }
    av_write_trailer(outFmtCtx);
    
    //close video stream
    if(stream) {
        avcodec_close(outCodecCtx);
    }
    for (i = 0; i < outFmtCtx->nb_streams; i++) {
        av_freep(&outFmtCtx->streams[i]->codec);
        av_freep(&outFmtCtx->streams[i]);
    }
    if (!(outFmt->flags & AVFMT_NOFILE))
    /* Close the output file. */
        avio_close(outFmtCtx->pb);
    /* free the output format context */
    avformat_free_context(outFmtCtx);
    
    // Free the YUV frame populated by the decoder
    av_frame_free(&inFrame);
    
    // Close the video codec (decoder)
    avcodec_close(inCodecCtx);
    
    // Close the input video file
    avformat_close_input(&inFmtCtx);

    return 1;

}
