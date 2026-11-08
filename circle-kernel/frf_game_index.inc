/* Persistent ROM ZIP index; accessed by CPU0 launcher only.
   No SD directory scan after returning from a game. */
static const unsigned FRF_MAX_INDEX=8192;
static const char *FRF_INDEX_PATH="SD:/MAMEIDX.DAT";
static const char *FRF_INDEX_TEMP="SD:/MAMEIDX.TMP";
static int frf_game_indices[8192];
static int frf_game_count=0;
static bool frf_index_from_cache=false;
struct FRFIndexHeader { uint32_t magic, version, drivers_hash, drivers_count, games_count, list_hash; };
static uint32_t frf_hash_byte(uint32_t h,unsigned char b) { return (h^(uint32_t)b)*16777619u; }
static uint32_t frf_hash_word(uint32_t h,uint32_t n) {
    for(unsigned k=0;k<4;k++){ h=frf_hash_byte(h,(unsigned char)n);n>>=8; }
    return h;
}
static uint32_t frf_driver_fingerprint(unsigned *total) {
    unsigned i=0;uint32_t h=2166136261u;
    for(;drivers[i] && i<FRF_MAX_INDEX;i++) {
        for(const char *p=drivers[i]->name;*p;p++)h=frf_hash_byte(h,(unsigned char)*p);
        h=frf_hash_byte(h,0);
    }
    *total=i;
    return frf_hash_word(h,i);
}
static uint32_t frf_indices_hash(const int *ids,unsigned count) {
    uint32_t h=2166136261u;
    for(unsigned i=0;i<count;i++)h=frf_hash_word(h,(uint32_t)ids[i]);
    return h;
}
static int frf_game_title_cmp(const void *pa,const void *pb) {
    int x=*(const int*)pa,y=*(const int*)pb;
    int n=strcmp(drivers[x]->description,drivers[y]->description);
    return n?n:strcmp(drivers[x]->name,drivers[y]->name);
}
static bool frf_load_game_index(void) {
    unsigned nd=0;uint32_t fp=frf_driver_fingerprint(&nd);
    FIL f;if(f_open(&f,FRF_INDEX_PATH,FA_READ)!=FR_OK)return false;
    FRFIndexHeader h={0};UINT got=0;
    FRESULT rc=f_read(&f,&h,sizeof(h),&got);
    bool valid=rc==FR_OK && got==sizeof(h) && h.magic==0x5844494du
        && h.version==2 && h.drivers_hash==fp && h.drivers_count==nd
        && h.games_count<=FRF_MAX_INDEX && h.games_count<=nd;
    int last=0;
    for(unsigned i=0;i<h.games_count && valid;i++){
        uint32_t idx=0;got=0;rc=f_read(&f,&idx,sizeof(idx),&got);
        valid=(rc==FR_OK && got==sizeof(idx) && idx<nd);
        if(valid && i){ int current=(int)idx; if(frf_game_title_cmp(&last,&current)>=0)valid=false; }
        last=(int)idx;
        if(valid)frf_game_indices[i]=(int)idx;
    }
    if(valid)valid=frf_indices_hash(frf_game_indices,h.games_count)==h.list_hash;
    f_close(&f);
    if(!valid)return false;
    frf_game_count=(int)h.games_count;
    frf_index_from_cache=true;
    return true;
}
static bool frf_save_game_index(void){
    unsigned nd=0;uint32_t fp=frf_driver_fingerprint(&nd);
    FRFIndexHeader h={0x5844494du,2u,fp,nd,(uint32_t)frf_game_count,
                      frf_indices_hash(frf_game_indices,(unsigned)frf_game_count)};
    FIL f;UINT wrote=0;
    if(f_open(&f,FRF_INDEX_TEMP,FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK)return false;
    FRESULT rc=f_write(&f,&h,sizeof(h),&wrote);
    bool ok=(rc==FR_OK && wrote==sizeof(h));
    for(int i=0;i<frf_game_count && ok;i++){
        uint32_t idx=(uint32_t)frf_game_indices[i];
        wrote=0;rc=f_write(&f,&idx,sizeof(idx),&wrote);
        ok=(rc==FR_OK && wrote==sizeof(idx));
    }
    if(ok)ok=f_sync(&f)==FR_OK;
    if(f_close(&f)!=FR_OK)ok=false;
    if(!ok){f_unlink(FRF_INDEX_TEMP);return false;}
    f_unlink(FRF_INDEX_PATH);
    return f_rename(FRF_INDEX_TEMP,FRF_INDEX_PATH)==FR_OK;
}
/* A single FAT directory walk instead of thousands of per-driver stat() calls.
   Driver name lookup is binary searched; this is only done when rebuilding. */
static int frf_driver_by_name[8192];
static int frf_name_cmp(const void *pa,const void *pb){
    int x=*(const int*)pa,y=*(const int*)pb;
    return strcmp(drivers[x]->name,drivers[y]->name);
}
static bool frf_fast_scan_roms(void){
    FATFS_DIR dir;
    if(f_opendir(&dir,"SD:/roms")!=FR_OK)return false;
    unsigned nd=0;
    for(;drivers[nd] && nd<FRF_MAX_INDEX;nd++)frf_driver_by_name[nd]=(int)nd;
    qsort(frf_driver_by_name,nd,sizeof(frf_driver_by_name[0]),frf_name_cmp);
    FILINFO info;
    bool ok=true;
    for(;;){
        FRESULT rc=f_readdir(&dir,&info);
        if(rc!=FR_OK){ok=false;break;}
        if(info.fname[0]==0)break;
        if(info.fattrib & AM_DIR)continue;
        size_t len=strlen(info.fname);
        if(len<=4 || len>=256u)continue;
        const char *ext=info.fname+len-4u;
        if(ext[0]!='.' || (ext[1]!='z'&&ext[1]!='Z') ||
           (ext[2]!='i'&&ext[2]!='I') || (ext[3]!='p'&&ext[3]!='P'))continue;
        char name[256];
        for(size_t j=0;j<len-4u;j++){
            char c=info.fname[j];
            name[j]=(c>='A'&&c<='Z')?(char)(c+32):c;
        }
        name[len-4u]=0;
        if(strcmp(name,"neogeo")==0)continue; /* BIOS dependency, not a game */
        unsigned lo=0,hi=nd;
        while(lo<hi){
            unsigned mid=lo+(hi-lo)/2u;
            int rel=strcmp(drivers[frf_driver_by_name[mid]]->name,name);
            if(rel<0)lo=mid+1u;else hi=mid;
        }
        if(lo<nd && strcmp(drivers[frf_driver_by_name[lo]]->name,name)==0 &&
           frf_game_count<(int)FRF_MAX_INDEX){
            frf_game_indices[frf_game_count++]=frf_driver_by_name[lo];
        }
    }
    f_closedir(&dir);
    return ok;
}
static void frf_refresh_games(void) {
    frf_game_count=0;frf_index_from_cache=false;
    if(!frf_fast_scan_roms()){
        /* Safe fallback for unsupported FAT directory enumeration. */
        frf_game_count=0;
        for(unsigned i=0;drivers[i] && frf_game_count<(int)FRF_MAX_INDEX;i++){
            if(frf_rom_present(drivers[i]->name))frf_game_indices[frf_game_count++]=(int)i;
        }
    }
    qsort(frf_game_indices,(size_t)frf_game_count,sizeof(frf_game_indices[0]),frf_game_title_cmp);
    frf_save_game_index();
}
static void frf_init_games(void) { if(!frf_load_game_index())frf_refresh_games(); }
