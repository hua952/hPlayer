#include "cppCom.h"
#include "cppClock.h"

#include "tSingleton.h"

extern "C"
{
int cpp_queue_picture(VideoState *is, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
    cppFrame *vp = nullptr;
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rPictQ = rGlobal.m_pictQ;
#if defined(DEBUG_SYNC)
    printf("frame_type=%c pts=%0.3f\n",
           av_get_picture_type_char(src_frame->pict_type), pts);
#endif

    if (!(vp = rPictQ.nextWrite()))
        return -1;

    vp->sar = src_frame->sample_aspect_ratio;
    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    vp->serial = serial;

    set_default_window_size(vp->width, vp->height, vp->sar);

    av_frame_move_ref(vp->frame, src_frame);
    // frame_queue_push(&is->pictq);
    rPictQ.push();
    return 0;
}

double cpp_get_master_clock(VideoState *is)
{
    double val;

    auto& rGlobal = tSingleton<globalData>::single();
    auto& rVidClk = rGlobal.vidClk;
    auto& rAudClk = rGlobal.m_audclk;
    auto& rExtclk = rGlobal.m_extclk;

    switch (cpp_get_master_sync_type(is)) {
        case AV_SYNC_VIDEO_MASTER:
            val = rVidClk.getClock();
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = rAudClk.getClock();
            break;
        default:
            val = rExtclk.getClock();
            break;
    }
    return val;
}


}

static int cpp_packet_queue_get(packQue& rQ, AVPacket *pkt, int block, int *serial)
{
    int ret = 0;
    do {
        auto pF = rQ.front();
        if (!pF) {
            if (!block) {
                ret = -1;
                break;
            }
            while(!(pF = rQ.front()));
        }

        rQ.allPackSizeAdd (-pF->m_pkt->size);
        rQ.allPackDurationAdd (-pF->m_pkt->duration);
        av_packet_move_ref(pkt, pF->m_pkt);
        if (serial) {
            *serial = pF->m_serial;
        }
        rQ.pop();
    } while (0);
    return ret;
}
int cpp_decoder_decode_frame(cppDecoder& rD, AVFrame *frame, AVSubtitle *sub)
{
    AVRational tb;
    int ret = AVERROR(EAGAIN);

    for (;;) {
        if (rD.queue->serial() == rD.pkt_serial) {
            do {
                
                switch (rD.avctx->codec_type) {
                    case AVMEDIA_TYPE_VIDEO:
                        ret = avcodec_receive_frame(rD.avctx, frame);
                        if (ret >= 0) {
                            if (decoder_reorder_pts == -1) {
                                frame->pts = frame->best_effort_timestamp;
                            } else if (!decoder_reorder_pts) {
                                frame->pts = frame->pkt_dts;
                            }
                        }
                        break;
                    case AVMEDIA_TYPE_AUDIO:
                        ret = avcodec_receive_frame(rD.avctx, frame);
                        if (ret >= 0) {
                            tb.num = 1; tb.den = frame->sample_rate; //tb = (AVRational){1, frame->sample_rate};
                            if (frame->pts != AV_NOPTS_VALUE)
                                frame->pts = av_rescale_q(frame->pts, rD.avctx->pkt_timebase, tb);
                            else if (rD.next_pts != AV_NOPTS_VALUE)
                                frame->pts = av_rescale_q(rD.next_pts, rD.next_pts_tb, tb);
                            if (frame->pts != AV_NOPTS_VALUE) {
                                rD.next_pts = frame->pts + frame->nb_samples;
                                rD.next_pts_tb = tb;
                            }
                        }
                        break;
                }
                if (ret == AVERROR_EOF) {
                    rD.finished = rD.pkt_serial;
                    avcodec_flush_buffers(rD.avctx);
                    return 0;
                }
                if (ret >= 0)
                    return 1;
            } while (ret != AVERROR(EAGAIN));
        }

        do {
            
            if (rD.packet_pending) {
                rD.packet_pending = 0;
            } else {
                int old_serial = rD.pkt_serial;
                if (cpp_packet_queue_get(*rD.queue, rD.pkt, 0, &rD.pkt_serial) < 0)
                    return -1;
                if (old_serial != rD.pkt_serial) {
                    avcodec_flush_buffers(rD.avctx);
                    rD.finished = 0;
                    rD.next_pts = rD.start_pts;
                    rD.next_pts_tb = rD.start_pts_tb;
                }
            }
            if (rD.queue->serial() == rD.pkt_serial)
                break;
            av_packet_unref(rD.pkt);
        } while (1);

        if (rD.avctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            int got_frame = 0;
            ret = avcodec_decode_subtitle2(rD.avctx, sub, &got_frame, rD.pkt);
            if (ret < 0) {
                ret = AVERROR(EAGAIN);
            } else {
                if (got_frame && !rD.pkt->data) {
                    rD.packet_pending = 1;
                }
                ret = got_frame ? 0 : (rD.pkt->data ? AVERROR(EAGAIN) : AVERROR_EOF);
            }
            av_packet_unref(rD.pkt);
        } else {
            if (rD.pkt->buf && !rD.pkt->opaque_ref) {
                FrameData *fd;

                rD.pkt->opaque_ref = av_buffer_allocz(sizeof(*fd));
                if (!rD.pkt->opaque_ref)
                    return AVERROR(ENOMEM);
                fd = (FrameData*)rD.pkt->opaque_ref->data;
                fd->pkt_pos = rD.pkt->pos;
            }

            if (avcodec_send_packet(rD.avctx, rD.pkt) == AVERROR(EAGAIN)) {
                av_log(rD.avctx, AV_LOG_ERROR, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                rD.packet_pending = 1;
            } else {
                av_packet_unref(rD.pkt);
            }
        }
    }
    return ret;
}



void cpp_sync_clock_to_slave(cppClock& c, cppClock &rSlave)
{
    double clock = c.getClock ();
    double slave_clock = rSlave.getClock();
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        // set_clock(c, slave_clock, rSlave.serial());
        c.setClock(slave_clock, rSlave.serial());
}

int cpp_get_master_sync_type(VideoState *is)
{
    auto& rGlobal = tSingleton<globalData>::single();
    if (rGlobal.av_sync_type() == AV_SYNC_VIDEO_MASTER) {
        if (rGlobal.videoSt())
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    } else if (rGlobal.av_sync_type() == AV_SYNC_AUDIO_MASTER) {
        if (rGlobal.audioSt())
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    } else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}
int create_hwaccel(AVBufferRef **device_ctx)
{
    enum AVHWDeviceType type;
    int ret;
    AVBufferRef *vk_dev;

    *device_ctx = NULL;

    if (!hwaccel)
        return 0;

    type = av_hwdevice_find_type_by_name(hwaccel);
    if (type == AV_HWDEVICE_TYPE_NONE)
        return AVERROR(ENOTSUP);

    if (!vk_renderer) {
        av_log(NULL, AV_LOG_ERROR, "Vulkan renderer is not available\n");
        return AVERROR(ENOTSUP);
    }

    ret = vk_renderer_get_hw_dev(vk_renderer, &vk_dev);
    if (ret < 0)
        return ret;

    ret = av_hwdevice_ctx_create_derived(device_ctx, type, vk_dev, 0);
    if (!ret)
        return 0;

    if (ret != AVERROR(ENOSYS))
        return ret;

    av_log(NULL, AV_LOG_WARNING, "Derive %s from vulkan not supported.\n", hwaccel);
    ret = av_hwdevice_ctx_create(device_ctx, type, NULL, NULL, 0);
    return ret;
}

int cpp_decoder_init(cppDecoder& rD, AVCodecContext *avctx, packQue* queue) {
    memset(&rD, 0, sizeof(rD));
    rD.pkt = av_packet_alloc();
    if (!rD.pkt)
        return AVERROR(ENOMEM);
    rD.avctx = avctx;
    rD.queue = queue;
    rD.start_pts = AV_NOPTS_VALUE;
    rD.pkt_serial = -1;
    return 0;
}

