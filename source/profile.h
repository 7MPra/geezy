#ifndef GEEZY_PROFILE_H
#define GEEZY_PROFILE_H
#ifdef GEEZY_PROFILE
static RenderStats profile_stats;
static u64 profile_cursor;
static unsigned profile_us[TIME_COUNT],profile_interval,profile_copies;
static void profile_begin(u64 now,unsigned interval) {
    profile_cursor=now; profile_interval=interval; profile_copies=0;
}
static void profile_mark(int stage) {
    u64 now=gettime(); profile_us[stage]=ticks_to_microsecs(now-profile_cursor); profile_cursor=now;
}
static void profile_end(void) {
    profile_mark(TIME_WAIT);
    render_stats_add(&profile_stats,profile_us,profile_interval,profile_copies);
}
#define profile_copy() (profile_copies++)
#else
#define profile_begin(now,interval) ((void)0)
#define profile_mark(stage) ((void)0)
#define profile_end() ((void)0)
#define profile_copy() ((void)0)
#endif
#endif
