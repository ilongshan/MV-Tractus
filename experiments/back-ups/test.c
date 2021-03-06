/*
 * Copyright (c) 2012 Stefano Sabatini
 * Copyright (c) 2014 Clément Bœsch
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.

 *Modified by: Jishnu Jaykumar Padalunkal
 *Modified on: 26 Apr 2018

*/

#include "include/libavutil/motion_vector.h"
#include "include/libavformat/avformat.h"
#include "include/libswscale/swscale.h"

// #include <libswscale/swscale.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <json/json.h>
#include <stdbool.h>

struct frame_and_MV{
  char * frame_data;
  json_object *mv_json_array;
} fmv;

static AVFormatContext *pFormatCtx = NULL;
// static AVCodecContext *video_dec_ctx = NULL;
static AVStream *video_stream = NULL;
static const char *src_filename = NULL;
AVCodecContext    *pCodecCtxOrig = NULL;
AVCodecContext    *pCodecCtx = NULL;
AVCodec           *pCodec = NULL;

static int video_stream_idx = -1;
static AVFrame *pFrame = NULL;
static AVFrame *pFrameRGB = NULL;
static int video_frame_count = 0;
int               frameFinished;
int               numBytes;
uint8_t           *buffer = NULL;
struct SwsContext *sws_ctx = NULL;

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;

  // Open file
  sprintf(szFilename, "./v2/output/frames/frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  for(y=0; y<height; y++)
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);

  // Close file
  fclose(pFile);
}

char* temp;

char* SaveFrameInDS(AVFrame *pFrame, int width, int height, int iFrame) {
  // char temp[width*height*5];
  // char szFilename[width*height*4];


  temp= (char*) malloc( width *height*5 * sizeof(char));
  char* szFilename= (char*) malloc( width *height*4 * sizeof(char));
  int  y;
  // Open file
  //sprintf(szFilename, "./v2/output/frames/frame%d.ppm", iFrame);
  //pFile=fopen(szFilename, "wb");
  //if(pFile==NULL)
  //  return;

  // Write header
  sprintf(szFilename, "P6\n# frame%d.ppm\n%d %d\n255\n", width, height);
  strcat(temp,szFilename);
  // Write pixel data
  long long a;
  for(y=0; y<height; y++){
    a=(pFrame->data[0])+y*(pFrame->linesize[0]);
    // printf("%s\n", a);
    sprintf(szFilename, "%d", a);
    strcat(temp,szFilename);
  }
  // free(szFilename);
  return temp;
    // printf("%d\n", pFrame->data[0]+y*pFrame->linesize[0]);
    // printf("%s\n", szFilename);
  // frame_data=temp;
  // printf("%s\n",szFilename );
 //
 //  // Close file
 //  // fclose(pFile);
 // FILE *pFile;
 // char szFilename[32];
 // int  y;
 //
 // // Open file
 // sprintf(szFilename, "./v2/output/frames/frame%d.ppm", iFrame);
 // pFile=fopen(szFilename, "wb");
 // if(pFile==NULL)
 //   return;
 //
 // // Write header
 // fprintf(pFile, "P6\n%d %d\n255\n", width, height);
 //
 // // Write pixel data
 // for(y=0; y<height; y++)
 //   fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
 //
 // // Close file
 // fclose(pFile);
 // return pFile;

}


static int print_motion_vectors_data(AVMotionVector *mv, int video_frame_count){
  printf("| #:%d | p/f:%2d | %2d x %2d | src:(%4d,%4d) | dst:(%4d,%4d) | dx:%4d | dy:%4d | motion_x:%4d | motion_y:%4d | motion_scale:%4d | 0x%"PRIx64" |\n",
      video_frame_count,
      mv->source,
      mv->w,
      mv->h,
      mv->src_x,
      mv->src_y,
      mv->dst_x,
      mv->dst_y,
      mv->dst_x - mv->src_x,
      mv->dst_y - mv->src_y,
      mv->motion_x,
      mv->motion_y,
      mv->motion_scale,
      mv->flags);
  printf("---------------------------------------------------------------------------------------------------------------------------------------------\n");
  return 0;
}

static int print_frame_data(AVFrame * frame){
  printf("%s\n", frame->data[0]);
  return 0;
}

static int decode_packet(const AVPacket *pkt)
{
  int ret = avcodec_send_packet(pCodecCtx, pkt);

  /*Creating a json array*/
  json_object *jarray;

  /*Creating a json object*/
  json_object * jobj;

  if (ret < 0) {
      fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(ret));
      return ret;
  }

  jarray = json_object_new_array();
  jarray=json_object_new_array();
  // struct frame_and_MV fmv;


  while (ret >= 0)  {
      ret = avcodec_receive_frame(pCodecCtx, pFrame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
      } else if (ret < 0) {
          fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(ret));
          return ret;
      }

      if (ret >= 0) {
          int i;
          AVFrameSideData *sd;
          video_frame_count++;
          sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
              pFrame->linesize, 0, pCodecCtx->height,
              pFrameRGB->data, pFrameRGB->linesize);

          // Save the frame to disk
          //SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height,video_frame_count);


          fmv.frame_data=SaveFrameInDS(pFrameRGB, pCodecCtx->width, pCodecCtx->height,video_frame_count);
          // printf("%s",frame_data);
          // printf("****************************************************************\n");
          // printf("%d\n",video_frame_count);
          sd = av_frame_get_side_data(pFrame, AV_FRAME_DATA_MOTION_VECTORS);
          // printf("%d\n", sd);

          if (sd) {
              const AVMotionVector *mvs = (const AVMotionVector *)sd->data;
              for (i = 0; i < sd->size / sizeof(*mvs); i++) {
                  const AVMotionVector *mv = &mvs[i];
                  // print_motion_vectors_data(mv, video_frame_count);
                    /*Form the json object*/
                    jobj = json_object_new_object();
                    json_object_object_add(jobj,"source", json_object_new_int (mv->source));
                    json_object_object_add(jobj,"width", json_object_new_int (mv->w));
                    json_object_object_add(jobj,"height", json_object_new_int (mv->h));

                    if(mv->source<0){
                      json_object_object_add(jobj,"src_x", json_object_new_int ((mv->src_x)/abs(mv->source)));
                      json_object_object_add(jobj,"src_y", json_object_new_int ((mv->src_y)/abs(mv->source)));
                      json_object_object_add(jobj,"dst_x", json_object_new_int ((mv->dst_x/abs(mv->source))));
                      json_object_object_add(jobj,"dst_y", json_object_new_int ((mv->dst_y/abs(mv->source))));
                      json_object_object_add(jobj,"dx", json_object_new_int (((mv->dst_x - mv->src_x)/abs(mv->source))));
                      json_object_object_add(jobj,"dy", json_object_new_int (((mv->dst_y - mv->src_y)/abs(mv->source))));
                    }else{
                      json_object_object_add(jobj,"src_x", json_object_new_int ((mv->dst_x/abs(mv->source))));
                      json_object_object_add(jobj,"src_y", json_object_new_int ((mv->dst_y/abs(mv->source))));
                      json_object_object_add(jobj,"dst_x", json_object_new_int ((mv->src_x/abs(mv->source))));
                      json_object_object_add(jobj,"dst_y", json_object_new_int ((mv->src_y/abs(mv->source))));
                      json_object_object_add(jobj,"dx", json_object_new_int (((mv->src_x - mv->dst_x)/abs(mv->source))));
                      json_object_object_add(jobj,"dy", json_object_new_int (((mv->src_y - mv->dst_y)/abs(mv->source))));
                    }
                    /*Adding the above created json object to the array*/
                    json_object_array_add(jarray,jobj);
                    /*Now printing the json object*/
                    // printf ("\n\nThe json object created: %s\n",json_object_to_json_string(jarray));
                    // jobj = json_object_new_object();
              // printf("%s\n", strcat( "./output/", strcat(video_frame_count, ".json")));
          }

          fmv.mv_json_array=jarray;
}
    printf("\rTotal Processed Frames:%d", video_frame_count);
    fflush(stdout);
          //Print frame data
          // print_frame_data(frame);
          av_frame_unref(pFrame);
  }
}

  return video_frame_count;
}

static int open_codec_context(AVFormatContext *pFormatCtx, enum AVMediaType type)
{
    int ret;
    AVStream *st;
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    ret = av_find_best_stream(pFormatCtx, type, -1, -1, &dec, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        int stream_idx = ret;
        st = pFormatCtx->streams[stream_idx];

        dec_ctx = avcodec_alloc_context3(dec);
        if (!dec_ctx) {
            fprintf(stderr, "Failed to allocate codec\n");
            return AVERROR(EINVAL);
        }

        ret = avcodec_parameters_to_context(dec_ctx, st->codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters to codec context\n");
            return ret;
        }

        /* Init the video decoder */
        av_dict_set(&opts, "flags2", "+export_mvs", 0);
        if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }

        video_stream_idx = stream_idx;
        video_stream = pFormatCtx->streams[video_stream_idx];
        pCodecCtx = dec_ctx;
    }

    return 0;
}

void createConnection(char* videopath){

  src_filename = videopath;

  printf("\nEstablishing Connection to %s\n", src_filename);

  if (avformat_open_input(&pFormatCtx, src_filename, NULL, NULL) < 0) {
      fprintf(stderr, "Could not open source file %s\n", src_filename);
      exit(1);
  }

  if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
      fprintf(stderr, "Could not find stream information\n");
      exit(1);
  }

  open_codec_context(pFormatCtx, AVMEDIA_TYPE_VIDEO);
  av_dump_format(pFormatCtx, 0, src_filename, 0);
  printf("\nConnection established to %s\n", src_filename);
}

void extract_motion_vectors(char *videopath){
    int ret = 0;
    AVPacket pkt = { 0 };
    struct stat sb;

    if (!video_stream) {
        fprintf(stderr, "Could not find video stream in the input, aborting\n");
        ret = 1;
        goto end;
    }

    pFrame = av_frame_alloc();
    if (!pFrame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    // printf("\n");
    // printf("**************************************************************************************\n");
    // printf("*       Tool : MV-Tractus                                                            *\n");
    // printf("*     Author : Jishnu Jaykumar Padalunkal (https://jishnujayakumar.github.io)        *\n");
    // printf("*  Used Libs : FFmpeg                                                                *\n");
    // printf("*Description : A simple tool to extract motion vectors from MPEG videos              *\n");
    // printf("**************************************************************************************\n");
    // printf("\n");
    // printf("--------------------------------------------------------------------------------------\n");
    // printf("framenum,source,blockw,blockh,srcx,srcy,dstx,dsty,motion_x,motion_y,motion_scale,flags\n");
    // printf("--------------------------------------------------------------------------------------\n");

    // Allocate an AVFrame structure
    pFrameRGB=av_frame_alloc();
    if(pFrameRGB==NULL)
      return -1;

    // Determine required buffer size and allocate buffer
    numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
  			      pCodecCtx->height);
    buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
  		 pCodecCtx->width, pCodecCtx->height);
    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(
           pCodecCtx->width,
  			   pCodecCtx->height,
  			   pCodecCtx->pix_fmt,
  			   pCodecCtx->width,
  			   pCodecCtx->height,
  			   AV_PIX_FMT_RGB24,
  			   SWS_BILINEAR,
  			   NULL,
  			   NULL,
  			   NULL
  			   );



    /* read frames from the file */
    while (av_read_frame(pFormatCtx, &pkt) >= 0) {
        if (pkt.stream_index == video_stream_idx){
              // printf("\nDecoding Packets\n");
              ret=decode_packet(&pkt);
              // ret = decode_packet(&pkt);
              // avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &p);

        }
        av_packet_unref(&pkt);
        if (ret < 0)
            break;
    }

    /* flush cached frames */
    decode_packet(NULL);
    printf("\n--------------------------------------------------------------------------------------\n");

end:
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
    av_frame_free(&pFrame);
    return ret < 0;
}

int main(int argc, char **argv)
{
  createConnection(argv[1]);
	extract_motion_vectors(argv[1]);
}
