//
//  objcffmpeg.m
//  testmee
//
//  Created by Prajna Kandarpa on 2015-03-30.
//  Copyright (c) 2015 Prajna Kandarpa. All rights reserved.
//



//Hi,
//I succesfully got FFMPEG and LIBX264 compiling together.
//I am trying to encode some RAW images into an .MP4 file (with H264
//                                                         video and AAC audio). I have managed to use libavcodec to write to a
//non - H264 file and it all seems to work well. I get warnings in my
//log [non-strictly-monotonic pts] repeatedly throughout the encode
//process and when done, the output file is unreadable.
//I have tried setting a CFR to the AVFormat with no luck.
//This is how i setup my frame writer

-(void)setupWriter:(int)width height:(int)height
{
    outputWidth = width;
    outputHeight = height;
    av_register_all();
    //Future proof for h264
    pOutputFmt = av_guess_format("h264",NULL, NULL);
    if(!pOutputFmt)
    {
        pOutputFmt = av_guess_format(NULL,[videoPath UTF8String], NULL);
    }
    
    if(!pOutputFmt)
    {
        NSLog(@"Count not deduce output format from file extention");
        pOutputFmt = av_guess_format("mpeg", NULL, NULL);
    }
    
    if(!pOutputFmt)
    {
        NSLog(@"Could not find suitable output format");
        return;
    }
    
    pFmtCtx = avformat_alloc_context();
    
    if(!pFmtCtx)
    {
        NSLog(@"Memory Error");
        return;
    }
    
    pFmtCtx->oformat = pOutputFmt;
    snprintf(pFmtCtx->filename, sizeof(pFmtCtx->filename), "%s",
             [videoPath UTF8String]);
    /* add the audio and video streams using the default format codecs and
     initialize the codecs */
    video_st = NULL;
    audio_st = NULL;
    if(pOutputFmt->video_codec != CODEC_ID_NONE)
        video_st = [self addVideoStream:pOutputFmt->video_codec];
        
        if(pOutputFmt->audio_codec == CODEC_ID_NONE)
        {
            pOutputFmt->audio_codec = CODEC_ID_AAC;
        }
    
    audio_st = [self addAudioStream:pOutputFmt->audio_codec];
    if(av_set_parameters(pFmtCtx, NULL) < 0 )
    {
        NSLog(@"Invald output format parameters");
        return;
    }
    
    dump_format(pFmtCtx, 0, [videoPath UTF8String], 1);
    
    if(video_st)
        [self openVideo:video_st];
    if(audio_st)
        [self openAudio:audio_st];
    float fMuxPreload  = 0.5f;
    float fMuxMaxDelay = 0.7f;
    pFmtCtx->preload   = (int)( fMuxPreload  * AV_TIME_BASE );
    pFmtCtx->max_delay = (int)( fMuxMaxDelay * AV_TIME_BASE );
    if(!(pOutputFmt->flags & AVFMT_NOFILE))
    {
        if(url_fopen(&pFmtCtx->pb, [videoPath UTF8String], URL_WRONLY) < 0)
        {
            NSLog(@"Cound not open file: %@",videoPath);
        }
    }
    
    pFmtCtx->packet_size = 2048;
    pFmtCtx->mux_rate = 10080000;
    av_write_header(pFmtCtx);
}


-(AVStream*) addVideoStream:(enum CodecID)codec_id
{
    AVCodecContext *c;
    AVStream *st;
    st = av_new_stream(pFmtCtx, 0);
    if (!st)
    {
        NSLog(@"Could not alloc stream");
        return nil;
    }
    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    /* put sample parameters */
    c->bit_rate = 4000000;
    /* resolution must be a multiple of two */
    c->width = outputWidth;
    c->height = outputHeight;
    c->time_base.den = streamFrameRate;
    c->time_base.num = 1;
    c->gop_size = 12; /* emit one intra frame every twelve frames at most */
    c->pix_fmt = STREAM_PIX_FMT;
    
    // some formats want stream headers to be separate
    if(pFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
        return st;
}

-(void) openVideo:(AVStream*)st
{
    AVCodec *codec;
    AVCodecContext *c;
    c = st->codec;
    /* find the video encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec)
    {
        NSLog(@"codec not found");
        return;
    }
    
    /* open the codec */
    if (avcodec_open(c, codec) < 0)
    {
        NSLog(@"could not open codec");
        return;
    }
    
    video_outbuf = NULL;
    if (!(pFmtCtx->oformat->flags & AVFMT_RAWPICTURE))
    {
        video_outbuf_size = 200000;
        video_outbuf = av_malloc(video_outbuf_size);
    }
    
    /* allocate the encoded raw picture */
    picture = [self allocPicture:c->pix_fmt];
    if (!picture)
    {
        NSLog(@"Could not allocate picture\n");
        return;
    }
    
    tmp_picture = NULL;
    if (c->pix_fmt != PIX_FMT_YUV420P)
    {
        tmp_picture = [self allocPicture:PIX_FMT_YUV420P];
        if (!tmp_picture)
        {
            NSLog(@"Could not allocate temporary picture");
            return;
        }
    }
}

and this is how i actually write the frame in question

-(void) writeVideoFrame:(AVStream*)st
{
    int out_size, ret;
    AVCodecContext *c;
    static struct SwsContext *img_convert_ctx;
    c = st->codec;
    if (c->pix_fmt != PIX_FMT_YUV420P)
    {
        
        /* as we only generate a YUV420P picture, we must convert it
         to the codec pixel format if needed */
        if (img_convert_ctx == NULL)
        {
            img_convert_ctx = sws_getContext(c->width, c->height,
                                             PIX_FMT_YUV420P,
                                             c->width, c->height,
                                             c->pix_fmt,
                                             SWS_BICUBIC, NULL, NULL, NULL);
            if (img_convert_ctx == NULL)
            {
                NSLog(@"Cannot initialize the conversion context\n");
                return;
            }
        }
        [self fillYUVImage:tmp_picture frame_index:frame_count];
        sws_scale(img_convert_ctx, tmp_picture->data,
                  tmp_picture->linesize,0, c->height, picture->data, picture->linesize);
    }
    else
    {
        [self fillYUVImage:picture frame_index:frame_count];
    }
    if (pFmtCtx->oformat->flags & AVFMT_RAWPICTURE)
    {
        /* raw video case. The API will change slightly in the near
         futur for that */
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.flags |= AV_PKT_FLAG_KEY;
        pkt.stream_index= st->index;
        pkt.data= (uint8_t *)picture;
        pkt.size= sizeof(AVPicture);
        ret = av_interleaved_write_frame(pFmtCtx, &pkt);
    }
    else
    {
        /* encode the image */
        out_size = avcodec_encode_video(c, video_outbuf,
                                        video_outbuf_size, picture);
        /* if zero size, it means the image was buffered */ 
        if (out_size > 0) { 
            AVPacket pkt; 
            av_init_packet(&pkt); 
            if (c->coded_frame->pts != AV_NOPTS_VALUE) 
                pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base); 
            if(c->coded_frame->key_frame) 
                pkt.flags |= AV_PKT_FLAG_KEY; 
            pkt.stream_index= st->index; 
            pkt.data= video_outbuf; 
            pkt.size= out_size; 
            /* write the compressed frame in the media file */ 
            ret = av_interleaved_write_frame(pFmtCtx, &pkt); 
        } 
        else 
        { 
            ret = 0; 
        } 
    } 
    if (ret != 0) 
    { 
        NSLog(@"Error while writing video frame"); 
        return; 
    } 
    frame_count++; 
} 
