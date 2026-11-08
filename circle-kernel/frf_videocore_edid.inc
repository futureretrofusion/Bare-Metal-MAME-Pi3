/* VideoCore HDMI EDID discovery for Pi 3 firmware (read-only).
   EDID VDBs describe sink-supported CEA VICs; do not claim firmware
   can dynamically switch via mailbox property tags. */
struct FRFVideoMode { unsigned width,height,rate_millihz,vic,flags; };
static FRFVideoMode gVideoModes[96];
static unsigned gVideoModeCount=0,gEDIDBlocks=0,gEDIDValid=0;
struct FRFVC4Mode { unsigned width,height,hz,group,code,native,interlaced; };
static FRFVC4Mode gVC4Modes[256];
static unsigned gVC4ModeCount=0;
static bool gVC4ModesReady=false;
struct FRFVC4State {unsigned valid,width,height,hz,group,code;};
static FRFVC4State gVC4State={0,0,0,0,0,0};
static int gVideoModePage=0;
static const char *gEDIDStatus="not queried";
static unsigned gVSyncMeasuredHz100=0;
static unsigned gVSyncMinGapUs=0,gVSyncMaxGapUs=0;
static void frf_measure_hdmi_vsync(void)
{
    gVSyncMeasuredHz100=gVSyncMinGapUs=gVSyncMaxGapUs=0;
    if(!gVideoFrameBuffer)return;
    /* Measure the firmware's own VSync pacing, not the source game's timer. */
    if(!gVideoFrameBuffer->WaitForVerticalSync())return;
    uint64_t t0=bm_cycles(),last=t0;
    uint64_t min=~0ull,max=0;
    for(unsigned n=0;n<32;n++){
        if(!gVideoFrameBuffer->WaitForVerticalSync())return;
        uint64_t t=bm_cycles(),dt=t-last;
        if(dt<min)min=dt;
        if(dt>max)max=dt;
        last=t;
    }
    uint64_t elapsed=last-t0;
    if(elapsed<100000u || elapsed>5000000u)return;
    gVSyncMeasuredHz100=(unsigned)((3200000000ull+elapsed/2u)/elapsed);
    gVSyncMinGapUs=(unsigned)min;
    gVSyncMaxGapUs=(unsigned)max;
}


static const struct { unsigned vic,w,h,hz; } frfVicTable[]={
 {1,640,480,60},{2,720,480,60},{3,720,480,60},{4,1280,720,60},
 {5,1920,1080,60},{6,1440,480,60},{7,1440,480,60},
 {16,1920,1080,60},{17,720,576,50},{18,720,576,50},
 {19,1280,720,50},{20,1920,1080,50},{21,1440,576,50},
 {22,1440,576,50},{31,1920,1080,50},{32,1920,1080,24},
 {33,1920,1080,25},{34,1920,1080,30},{39,1920,1080,50},
 {60,1280,720,24},{61,1280,720,25},{62,1280,720,30},
 {93,3840,2160,24},{94,3840,2160,25},{95,3840,2160,30},
 {96,3840,2160,50},{97,3840,2160,60}
};
static void frf_mode_add(unsigned w,unsigned h,unsigned mhz,
                         unsigned vic,unsigned flags)
{
    if(!w||!h||mhz<10000u||mhz>240000u)return;
    for(unsigned i=0;i<gVideoModeCount;i++)
        if(gVideoModes[i].width==w && gVideoModes[i].height==h &&
           gVideoModes[i].rate_millihz==mhz){
            gVideoModes[i].flags|=flags;
            if(vic)gVideoModes[i].vic=vic;
            return;
        }
    if(gVideoModeCount<96u)
        gVideoModes[gVideoModeCount++]={w,h,mhz,vic,flags};
}
static void frf_parse_dtd(const unsigned char *p)
{
    unsigned pix10k=(unsigned)p[0]|((unsigned)p[1]<<8);
    if(!pix10k)return;
    unsigned w=(unsigned)p[2]|((unsigned)(p[4]&0xf0)<<4);
    unsigned h=(unsigned)p[5]|((unsigned)(p[7]&0xf0)<<4);
    unsigned hblank=(unsigned)p[3]|((unsigned)(p[4]&0x0f)<<8);
    unsigned vblank=(unsigned)p[6]|((unsigned)(p[7]&0x0f)<<8);
    unsigned ht=w+hblank, vt=h+vblank;
    if(!ht||!vt)return;
    unsigned mhz=(unsigned)(((uint64_t)pix10k*10000000u+
                             ((uint64_t)ht*vt)/2u)/((uint64_t)ht*vt));
    unsigned interlaced=(p[17]&0x80)?2u:0u;
    frf_mode_add(w,h,mhz,0,1u|interlaced);
}
static void frf_parse_edid_block(const unsigned char *e,unsigned block)
{
    unsigned checksum=0;
    for(unsigned i=0;i<128;i++)checksum+=e[i];
    if(checksum&255u)return;
    if(!block) {
        static const unsigned char magic[]={0,255,255,255,255,255,255,0};
        if(memcmp(e,magic,8))return;
        gEDIDValid++;
        /* Established DMT timings from EDID base block. */
        static const struct {unsigned byte,bit,w,h,hz;} established[]={
            {35,7,720,400,70},{35,6,720,400,88},
            {35,5,640,480,60},{35,4,640,480,67},
            {35,3,640,480,72},{35,2,640,480,75},
            {35,1,800,600,56},{35,0,800,600,60},
            {36,7,800,600,72},{36,6,800,600,75},
            {36,5,832,624,75},{36,4,1024,768,87},
            {36,3,1024,768,60},{36,2,1024,768,70},
            {36,1,1024,768,75},{36,0,1280,1024,75},
            {37,7,1152,870,75}
        };
        for(unsigned i=0;i<sizeof(established)/sizeof(established[0]);i++)
            if(e[established[i].byte]&(1u<<established[i].bit))
                frf_mode_add(established[i].w,established[i].h,
                             established[i].hz*1000u,0,8u);
        /* Eight EDID standard timings (DMT-like timings). */
        for(unsigned i=38;i<54;i+=2){
            if((e[i]==1u && e[i+1]==1u) || e[i]==0u)continue;
            unsigned w=((unsigned)e[i]+31u)*8u;
            unsigned aspect=e[i+1]>>6;
            unsigned h=aspect==0u ? (w*10u)/16u:
                       aspect==1u ? (w*3u)/4u:
                       aspect==2u ? (w*4u)/5u:(w*9u)/16u;
            unsigned hz=(e[i+1]&63u)+60u;
            frf_mode_add(w,h,hz*1000u,0,8u);
        }
        for(unsigned i=54;i+18<=126;i+=18)frf_parse_dtd(e+i);
    } else if(e[0]==0x02 && e[1]>=3) {
        gEDIDValid++;
        unsigned end=e[2];
        if(end<4 || end>127)end=127;
        unsigned pos=4;
        while(pos<end){
            unsigned v=e[pos++];
            unsigned tag=v>>5,n=v&31u;
            if(pos+n>end)break;
            if(tag==2) {
                for(unsigned j=0;j<n;j++){
                    unsigned vic=e[pos+j]&0x7fu;
                    bool known=false;
                    for(unsigned k=0;k<sizeof(frfVicTable)/sizeof(frfVicTable[0]);k++)
                        if(frfVicTable[k].vic==vic) {
                            frf_mode_add(frfVicTable[k].w,frfVicTable[k].h,
                                         frfVicTable[k].hz*1000u,vic,4u);
                            known=true;break;
                        }
                    if(!known && gVideoModeCount<96u)
                        gVideoModes[gVideoModeCount++]={0,0,0,vic,16u};
                }
            }
            pos+=n;
        }
        for(unsigned i=end;i+18<=126;i+=18)frf_parse_dtd(e+i);
    } else gEDIDValid++;
}
static void frf_query_hdmi_modes(void)
{
    gVideoModeCount=gEDIDBlocks=gEDIDValid=0;
    gEDIDStatus="EDID unavailable (display/firmware)";
    CBcmPropertyTags tags;
    unsigned maxBlocks=1;
    for(unsigned n=0;n<maxBlocks && n<5;n++) {
        TPropertyTagEDIDBlock edid={};
        edid.nBlockNumber=n;
        if(!tags.GetTag(PROPTAG_GET_EDID_BLOCK,&edid,sizeof(edid),8u) ||
           edid.nStatus!=EDID_STATUS_SUCCESS)break;
        ++gEDIDBlocks;
        frf_parse_edid_block(edid.Block,n);
        if(n==0) {
            maxBlocks=1u+edid.Block[126];
            if(maxBlocks>5u)maxBlocks=5u;
        }
    }
    if(gEDIDValid)gEDIDStatus="EDID supported display modes";
}
/* On-demand export; never write SD during normal gameplay. */
static const char *gVideoExportStatus="Press E to export all modes.";
static bool frf_video_export_line(FIL *file,const char *line)
{
    UINT n=0;
    unsigned len=(unsigned)strlen(line);
    return f_write(file,line,len,&n)==FR_OK && n==len;
}
static bool frf_export_video_modes(void)
{
    FIL f;
    if(f_open(&f,"SD:/VIDMODES.TXT",FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK)
        return false;
    bool ok=true;
    char row[192];
    snprintf(row,sizeof(row),
             "MAME106RGB VideoCore modes\nEDID blocks=%u valid=%u modes=%u\n"
             "Firmware TV service modes=%u\n",
             gEDIDBlocks,gEDIDValid,gVideoModeCount,gVC4ModeCount);
    ok=frf_video_export_line(&f,row);
    if(ok && gVSyncMeasuredHz100){
        snprintf(row,sizeof(row),
                 "Measured VSync=%u.%02u Hz, interval min=%uus max=%uus\n",
                 gVSyncMeasuredHz100/100u,gVSyncMeasuredHz100%100u,
                 gVSyncMinGapUs,gVSyncMaxGapUs);
        ok=frf_video_export_line(&f,row);
    }
    if(ok && gVC4State.valid){
        snprintf(row,sizeof(row),"ACTIVE: %ux%u @%uHz group=%u mode=%u\n",
                 gVC4State.width,gVC4State.height,gVC4State.hz,
                 gVC4State.group,gVC4State.code);
        ok=frf_video_export_line(&f,row);
    }
    for(unsigned i=0;i<gVC4ModeCount && ok;i++){
        const FRFVC4Mode &m=gVC4Modes[i];
        snprintf(row,sizeof(row),"VC group=%u code=%u %ux%u @%uHz %s native=%u\n",
                 m.group,m.code,m.width,m.height,m.hz,m.interlaced?"i":"p",m.native);
        ok=frf_video_export_line(&f,row);
    }
    for(unsigned i=0;i<gVideoModeCount && ok;i++){
        const FRFVideoMode &m=gVideoModes[i];
        snprintf(row,sizeof(row),"EDID VIC=%u %ux%u @%u.%03uHz flags=%u\n",
                 m.vic,m.width,m.height,m.rate_millihz/1000u,
                 m.rate_millihz%1000u,m.flags);
        ok=frf_video_export_line(&f,row);
    }
    if(ok)ok=f_sync(&f)==FR_OK;
    if(f_close(&f)!=FR_OK)ok=false;
    return ok;
}
static void frf_format_suggested_hdmi_mode(char *out,size_t capacity)
{
    if(!out || !capacity)return;
    out[0]=0;
    if(!gSelectedGameHzMillihz || !gVideoFrameBuffer)return;
    unsigned w=gVideoFrameBuffer->GetWidth(),h=gVideoFrameBuffer->GetHeight();
    unsigned bestDiff=~0u,bestGroup=0,bestCode=0,bestHz=0;
    if(gVC4ModeCount){
        for(unsigned i=0;i<gVC4ModeCount;i++){
            const FRFVC4Mode &m=gVC4Modes[i];
            if(m.width!=w || m.height!=h || m.interlaced)continue;
            unsigned hz=m.hz*1000u;
            unsigned diff=hz>gSelectedGameHzMillihz?
                hz-gSelectedGameHzMillihz:gSelectedGameHzMillihz-hz;
            if(diff<bestDiff){
                bestDiff=diff;bestGroup=m.group;bestCode=m.code;bestHz=hz;
            }
        }
    } else {
        for(unsigned i=0;i<gVideoModeCount;i++){
            const FRFVideoMode &m=gVideoModes[i];
            if(m.width!=w || m.height!=h || (m.flags&2u))continue;
            unsigned diff=m.rate_millihz>gSelectedGameHzMillihz?
                m.rate_millihz-gSelectedGameHzMillihz:
                gSelectedGameHzMillihz-m.rate_millihz;
            if(diff<bestDiff){
                bestDiff=diff;bestGroup=m.vic?1u:0u;
                bestCode=m.vic;bestHz=m.rate_millihz;
            }
        }
    }
    if(bestDiff!=~0u){
        snprintf(out,capacity,"%ux%u @%u.%03uHz  %s code %u  (suggestion only)",
                 w,h,bestHz/1000u,bestHz%1000u,
                 bestGroup==1u?"CEA":bestGroup==2u?"DMT":"EDID",bestCode);
    } else {
        snprintf(out,capacity,"No progressive %ux%u mode in detected list",w,h);
    }
}
static void frf_show_hdmi_modes(void)
{
    printf("\033[2J\033[H");
    printf("  VideoCore HDMI display information\n");
    printf("  TV service modes: %u %s\n",
           gVC4ModeCount,gVC4ModesReady?"(firmware enumerated)":"(EDID fallback)");
    if(gVSyncMeasuredHz100)
        printf("  Measured firmware VSync: %u.%02uHz  interval %u-%uus\n",
               gVSyncMeasuredHz100/100u,gVSyncMeasuredHz100%100u,
               gVSyncMinGapUs,gVSyncMaxGapUs);
    if(gVC4State.valid)
        printf("  ACTIVE HDMI: %ux%u @%uHz %s mode %u\n",gVC4State.width,
               gVC4State.height,gVC4State.hz,
               gVC4State.group==1u?"CEA":"DMT",gVC4State.code);
    printf("  EDID blocks: %u  valid: %u  modes: %u\n\n",
           gEDIDBlocks,gEDIDValid,gVideoModeCount);
    printf("  %s\n\n",gEDIDStatus);
    if(gSelectedGameHzMillihz){
        char recommend[160];
        frf_format_suggested_hdmi_mode(recommend,sizeof(recommend));
        printf("  Selected game: %ux%u @%u.%03uHz\n",
               gSelectedGameW,gSelectedGameH,
               gSelectedGameHzMillihz/1000u,
               gSelectedGameHzMillihz%1000u);
        printf("  Closest refresh at current size: %s\n",recommend);
    }
    printf("  Firmware boot output: framebuffer %ux%u @ depth 16\n\n",
           (unsigned)(gVideoFrameBuffer?gVideoFrameBuffer->GetWidth():0u),
           (unsigned)(gVideoFrameBuffer?gVideoFrameBuffer->GetHeight():0u));
    unsigned begin=(unsigned)gVideoModePage*14u;
    if(gVC4ModeCount){
        for(unsigned i=begin;i<gVC4ModeCount && i<begin+14u;i++){
            const FRFVC4Mode &m=gVC4Modes[i];
            printf("  %2u: %-3s mode %-3u %4ux%-4u @%3uHz  %s%s\n",
                   i+1u,m.group==1u?"CEA":"DMT",m.code,
                   m.width,m.height,m.hz,m.interlaced?"i":"p",
                   m.native?" native":"");
        }
    } else for(unsigned i=begin;i<gVideoModeCount && i<begin+14u;i++){
        const FRFVideoMode &m=gVideoModes[i];
        printf("  %2u: %4ux%-4u %3u.%03u Hz  VIC:%-3u %s\n",i+1u,
               m.width,m.height,m.rate_millihz/1000u,
               m.rate_millihz%1000u,m.vic,(m.flags&16u)?"CEA unknown":((m.flags&4u)?"CEA":((m.flags&8u)?"DMT":"DTD")));
    }
    printf("\n  PGUP/PGDN more  V or ESC return  E export\n");
    printf("  %s\n",gVideoExportStatus);
    printf("  %s\n",
           gVC4ModeCount?"Modes returned by VideoCore TV firmware.":
                         "EDID modes; firmware support not verified.");
    printf("  Current HDMI timing is selected by config.txt at boot.\n");
    fflush(stdout);
}
