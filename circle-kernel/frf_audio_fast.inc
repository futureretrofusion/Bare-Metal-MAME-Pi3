/* Fast audio experiment: stream 32 kHz MAME PCM into 48 kHz HDMI
   with a continuous, 3/2 linear interpolator. No heap or floating-point.
   Maintains fractional phase across game frames to prevent pitch drift. */
static bool frf_audio_has_prev=false;
static int16_t frf_audio_prev_l=0,frf_audio_prev_r=0;
static uint64_t frf_audio_source_index=0,frf_audio_next_thirds=0;
static int16_t frf_audio_resample_buffer[4096u*2u];
extern "C" void bm_audio_reset(void)
{
    frf_audio_has_prev=false;
    frf_audio_source_index=0;
    frf_audio_next_thirds=0;
    if(gAudioDevice) gAudioDevice->Flush();
}
static int frf_audio_resample_write(const short *src,unsigned frames)
{
    unsigned out=0;
    for(unsigned i=0;i<frames;i++) {
        int16_t l=src[i*2u],r=src[i*2u+1u];
        if(!frf_audio_has_prev) {
            frf_audio_has_prev=true;
            frf_audio_prev_l=l;frf_audio_prev_r=r;
            frf_audio_resample_buffer[0]=l;
            frf_audio_resample_buffer[1]=r;
            out=1;
            frf_audio_next_thirds=2;
            continue;
        }
        ++frf_audio_source_index;
        uint64_t end=frf_audio_source_index*3ull;
        while(frf_audio_next_thirds<=end) {
            unsigned b=(unsigned)(frf_audio_next_thirds-(end-3ull));
            unsigned a=3u-b;
            int32_t ol=((int32_t)frf_audio_prev_l*(int)a+(int32_t)l*(int)b)/3;
            int32_t orr=((int32_t)frf_audio_prev_r*(int)a+(int32_t)r*(int)b)/3;
            frf_audio_resample_buffer[out*2u]=(int16_t)ol;
            frf_audio_resample_buffer[out*2u+1u]=(int16_t)orr;
            out++;
            frf_audio_next_thirds+=2ull;
            if(out==4096u) {
                frf_audio_queue_write(frf_audio_resample_buffer,out);
                out=0;
            }
        }
        frf_audio_prev_l=l;frf_audio_prev_r=r;
    }
    return out ? frf_audio_queue_write(frf_audio_resample_buffer,out) : 0;
}
