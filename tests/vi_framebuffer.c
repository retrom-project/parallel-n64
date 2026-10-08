/* Native framebuffer reads are real; GPU operations only record submission. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
   uint32_t addr, size, width, height, ul_x, ul_y, lr_x, lr_y, opaque;
} FB_TO_SCREEN_INFO;
typedef struct {
   int smallLodLog2, largeLodLog2, aspectRatioLog2, format;
   void *data;
} GrTexInfo;
enum {GR_LOD_LOG2_256, GR_LOD_LOG2_512, GR_ASPECT_LOG2_1x1,
      GR_ASPECT_LOG2_2x1, GR_TEXFMT_ARGB_1555, GR_TEXFMT_ARGB_8888,
      GR_MIPMAPLEVELMASK_BOTH, hack_RE2 = 1, hack_Lego = 2};
static struct {uint8_t *RDRAM; uint32_t *VI_WIDTH_REG, *VI_ORIGIN_REG, *VI_STATUS_REG;} gfx_info;
static struct {float vi_height, scale_x, scale_y, offset_x, offset_y; uint32_t last_bg; int updatescreen;} rdp;
static struct {unsigned hacks;} settings;
static struct {unsigned tmem_ptr[2];} voodoo;
static uint32_t BMASK = 0x7fffff;
static uint32_t texture_buffer[512 * 512];
static int submitted;
static int SetupFBtoScreenCombiner(unsigned a, unsigned b) {(void)a; (void)b; return 0;}
static unsigned grTexCalcMemRequired(int a, int b, int c) {(void)a; (void)b; (void)c; return 0;}
static void grTexSource(int a, unsigned b, int c, GrTexInfo *d, bool e)
{(void)a; (void)b; (void)c; (void)d; (void)e; submitted++;}
static void DrawFrameBufferToScreen256(FB_TO_SCREEN_INFO *f) {(void)f; abort();}
static void DrawRE2Video(FB_TO_SCREEN_INFO *f, float s) {(void)f; (void)s; abort();}
static void glide64_draw_fb(float a,float b,float c,float d,float e,float f,float g)
{(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;}
static void newSwapBuffers(void) {abort();}

/* PRODUCTION_FUNCTIONS */

int main(int argc, char **argv)
{
   uint32_t width = 320, origin = 0x272e0, status = 2;
   int expect_draw = 1;
   uint16_t expected16 = (0x1235 >> 1) | 0x8000;
   assert(argc == 2);
   rdp.vi_height = 2;
   if (!strcmp(argv[1], "physical32") || !strcmp(argv[1], "kseg32") ||
       !strcmp(argv[1], "last32") || !strcmp(argv[1], "cross-end32")) status = 3;
   if (!strncmp(argv[1], "kseg", 4)) origin |= 0xa0000000;
   if (!strncmp(argv[1], "last", 4)) origin = BMASK + 1 - 640 * (1u << (status - 1));
   if (!strcmp(argv[1], "past-end")) {origin = 0x800000; expect_draw = 0;}
   if (!strncmp(argv[1], "cross-end", 9)) {origin = BMASK + 1 - 320 * (1u << (status - 1)); expect_draw = 0;}
   if (!strcmp(argv[1], "zero-width")) {width = 0; expect_draw = 0;}
   if (!strcmp(argv[1], "width-bits")) width |= 0xf0000000;
   if (!strcmp(argv[1], "zero-height")) {rdp.vi_height = 0; expect_draw = 0;}
   if (!strcmp(argv[1], "negative-height")) {rdp.vi_height = -1; expect_draw = 0;}
   if (!strcmp(argv[1], "nan-height")) {rdp.vi_height = NAN; expect_draw = 0;}
   if (!strcmp(argv[1], "infinite-height")) {rdp.vi_height = INFINITY; expect_draw = 0;}
   if (!strcmp(argv[1], "huge-height")) {rdp.vi_height = 1e20f; expect_draw = 0;}
   if (!strcmp(argv[1], "odd16-end")) {width = 201; rdp.vi_height = 1; origin = BMASK + 1 - 402; expect_draw = 0;}
   if (!strcmp(argv[1], "four-mib")) {BMASK = 0x3fffff; origin = 0x400000; expect_draw = 0;}
   gfx_info.RDRAM = malloc(BMASK + 1u);
   assert(gfx_info.RDRAM);
   for (uint32_t i = 0; i < (BMASK + 1u) / 2; ++i) ((uint16_t*)gfx_info.RDRAM)[i] = 0x1235;
   gfx_info.VI_WIDTH_REG = &width;
   gfx_info.VI_ORIGIN_REG = &origin;
   gfx_info.VI_STATUS_REG = &status;
   drawViRegBG();
   assert(submitted == expect_draw);
   if (expect_draw) {
      if (status == 2) assert(((uint16_t*)texture_buffer)[0] == expected16);
      else assert(texture_buffer[0] == 0xff123512);
      assert(rdp.last_bg == (origin & 0xffffff));
   }
   /* Renderer interpretation must not change the CPU-visible register. */
   if (!strncmp(argv[1], "kseg", 4)) assert(origin == 0xa00272e0);
   free(gfx_info.RDRAM);
   return 0;
}
