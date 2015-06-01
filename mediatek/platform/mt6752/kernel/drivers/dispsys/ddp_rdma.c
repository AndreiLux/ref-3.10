#define LOG_TAG "RDMA" 
#include "ddp_log.h"

#include <mach/mt_clkmgr.h>
#include <linux/delay.h>
#include "ddp_info.h"
#include "ddp_reg.h"
#include "ddp_matrix_para.h"
#include "ddp_rdma.h"
#include "ddp_dump.h"
#include "lcm_drv.h"  // to check if UFO enabled.

enum RDMA_INPUT_FORMAT {
    RDMA_INPUT_FORMAT_BGR565    = 0,
    RDMA_INPUT_FORMAT_RGB888    = 1,
    RDMA_INPUT_FORMAT_RGBA8888  = 2,
    RDMA_INPUT_FORMAT_ARGB8888  = 3,
    RDMA_INPUT_FORMAT_VYUY      = 4,
    RDMA_INPUT_FORMAT_YVYU      = 5,

    RDMA_INPUT_FORMAT_RGB565    = 6,
    RDMA_INPUT_FORMAT_BGR888    = 7,
    RDMA_INPUT_FORMAT_BGRA8888  = 8,
    RDMA_INPUT_FORMAT_ABGR8888  = 9,
    RDMA_INPUT_FORMAT_UYVY      = 10,
    RDMA_INPUT_FORMAT_YUYV      = 11,
    
    RDMA_INPUT_FORMAT_UNKNOW    = 32,
};

static unsigned int rdma_fps[RDMA_INSTANCES]={60,60};

static enum RDMA_INPUT_FORMAT rdma_input_format_convert(DpColorFormat fmt)
{
   enum RDMA_INPUT_FORMAT rdma_fmt = RDMA_INPUT_FORMAT_RGB565;
   switch(fmt)
   {    
       case eBGR565           :
           rdma_fmt = RDMA_INPUT_FORMAT_BGR565    ;  break;
       case eRGB888           :
           rdma_fmt = RDMA_INPUT_FORMAT_RGB888    ;  break;
       case eRGBA8888         :
           rdma_fmt = RDMA_INPUT_FORMAT_RGBA8888  ;  break;
       case eARGB8888         :
           rdma_fmt = RDMA_INPUT_FORMAT_ARGB8888  ;  break;
       case eVYUY             :
           rdma_fmt = RDMA_INPUT_FORMAT_VYUY      ;  break;
       case eYVYU             :
           rdma_fmt = RDMA_INPUT_FORMAT_YVYU      ;  break;
       case eRGB565           : 
           rdma_fmt = RDMA_INPUT_FORMAT_RGB565    ;  break;    
       case eBGR888           :
           rdma_fmt = RDMA_INPUT_FORMAT_BGR888    ;  break;
       case eBGRA8888         :
           rdma_fmt = RDMA_INPUT_FORMAT_BGRA8888  ;  break;
       case eABGR8888         :
           rdma_fmt = RDMA_INPUT_FORMAT_ABGR8888  ;  break; 
       case eUYVY             :
           rdma_fmt = RDMA_INPUT_FORMAT_UYVY      ;  break;
       case eYUY2             :
           rdma_fmt = RDMA_INPUT_FORMAT_YUYV      ;  break;
       default:
           DDPERR("rdma_fmt_convert fmt=%d, rdma_fmt=%d \n", fmt, rdma_fmt);
   }
   return rdma_fmt;
}

static unsigned int rdma_input_format_byte_swap(enum RDMA_INPUT_FORMAT inputFormat)
{
    int input_swap = 0;
    switch(inputFormat) 
    {
        case RDMA_INPUT_FORMAT_BGR565:
        case RDMA_INPUT_FORMAT_RGB888:
        case RDMA_INPUT_FORMAT_RGBA8888:
        case RDMA_INPUT_FORMAT_ARGB8888:
        case RDMA_INPUT_FORMAT_VYUY:
        case RDMA_INPUT_FORMAT_YVYU:
        	input_swap = 1;
        	break;
        case RDMA_INPUT_FORMAT_RGB565:
        case RDMA_INPUT_FORMAT_BGR888:
        case RDMA_INPUT_FORMAT_BGRA8888:
        case RDMA_INPUT_FORMAT_ABGR8888:
        case RDMA_INPUT_FORMAT_UYVY:
        case RDMA_INPUT_FORMAT_YUYV:
        	input_swap = 0;
        	break;
        default:
            DDPERR("unknow RDMA input format is %d\n", inputFormat);
            ASSERT(0);
    }
    return input_swap;
}

static unsigned int rdma_input_format_bpp(enum RDMA_INPUT_FORMAT inputFormat)
{
    int bpp = 0;
    switch(inputFormat) 
    {
        case RDMA_INPUT_FORMAT_BGR565:
        case RDMA_INPUT_FORMAT_RGB565: 
        case RDMA_INPUT_FORMAT_VYUY:
        case RDMA_INPUT_FORMAT_UYVY:
        case RDMA_INPUT_FORMAT_YVYU:
        case RDMA_INPUT_FORMAT_YUYV:
            bpp = 2;
            break;
        case RDMA_INPUT_FORMAT_RGB888:
        case RDMA_INPUT_FORMAT_BGR888:
            bpp = 3;
            break;
        case RDMA_INPUT_FORMAT_ARGB8888:
        case RDMA_INPUT_FORMAT_ABGR8888:
        case RDMA_INPUT_FORMAT_RGBA8888:
        case RDMA_INPUT_FORMAT_BGRA8888:
            bpp = 4;
            break;
        default:
            DDPERR("unknown RDMA input format = %d\n", inputFormat);
            ASSERT(0);
    }
    return  bpp;
}

static unsigned int rdma_input_format_color_space(enum RDMA_INPUT_FORMAT inputFormat)
{
    int space = 0;
    switch(inputFormat) 
    {
        case RDMA_INPUT_FORMAT_BGR565:
        case RDMA_INPUT_FORMAT_RGB565:
        case RDMA_INPUT_FORMAT_RGB888:
        case RDMA_INPUT_FORMAT_BGR888:
        case RDMA_INPUT_FORMAT_RGBA8888:
        case RDMA_INPUT_FORMAT_BGRA8888:
        case RDMA_INPUT_FORMAT_ARGB8888:
        case RDMA_INPUT_FORMAT_ABGR8888:
            space = 0;
            break;
        case RDMA_INPUT_FORMAT_VYUY:
        case RDMA_INPUT_FORMAT_UYVY:
        case RDMA_INPUT_FORMAT_YVYU:            
        case RDMA_INPUT_FORMAT_YUYV:
            space = 1;
            break;
        default:
            DDPERR("unknown RDMA input format = %d\n", inputFormat);
            ASSERT(0);
    }
    return space;
}

static unsigned int rdma_input_format_reg_value(enum RDMA_INPUT_FORMAT inputFormat)
{
    int reg_value = 0;
    switch(inputFormat) 
    {
        case RDMA_INPUT_FORMAT_BGR565:
        case RDMA_INPUT_FORMAT_RGB565:
            reg_value = 0x0;
        	break;
        case RDMA_INPUT_FORMAT_RGB888:
        case RDMA_INPUT_FORMAT_BGR888:
            reg_value = 0x1;
        	break;
        case RDMA_INPUT_FORMAT_RGBA8888:
        case RDMA_INPUT_FORMAT_BGRA8888:
            reg_value = 0x2;
        	break;
        case RDMA_INPUT_FORMAT_ARGB8888:
        case RDMA_INPUT_FORMAT_ABGR8888:
            reg_value = 0x3;
        	break;
        case RDMA_INPUT_FORMAT_VYUY:
        case RDMA_INPUT_FORMAT_UYVY:  
          	reg_value = 0x4;
        	break;          
        case RDMA_INPUT_FORMAT_YVYU:
        case RDMA_INPUT_FORMAT_YUYV:  
        	reg_value = 0x5;
        	break;
        default:
            DDPERR("unknow RDMA input format is %d\n", inputFormat);
            ASSERT(0);
    }
    return reg_value;
}

static char*  rdma_intput_format_name(enum RDMA_INPUT_FORMAT fmt, int swap)
{
    switch(fmt)
    {    
        case RDMA_INPUT_FORMAT_BGR565   :
	    {
			return swap ? "eBGR565" : "eRGB565";
    	}
        case RDMA_INPUT_FORMAT_RGB565   :
	    {
			return "eRGB565";
    	}        
        case RDMA_INPUT_FORMAT_RGB888   :
		{
            return swap ? "eRGB888" : "eBGR888";
		}
        case RDMA_INPUT_FORMAT_BGR888   :
		{
            return "eBGR888";
		}        
        case RDMA_INPUT_FORMAT_RGBA8888 :
		{
			return swap ? "eRGBA888" : "eBGRA888";
		}
        case RDMA_INPUT_FORMAT_BGRA8888 :
		{
			return "eBGRA888";
		}        
        case RDMA_INPUT_FORMAT_ARGB8888 :
		{
			return swap ? "eARGB8888" : "eABGR8888";
		}
        case RDMA_INPUT_FORMAT_ABGR8888 :
		{
			return "eABGR8888";
		}	        
        case RDMA_INPUT_FORMAT_VYUY :
		{
			return swap ? "eVYUY" : "eUYVY";
		}
        case RDMA_INPUT_FORMAT_UYVY :
		{
			return "eUYVY";
		}	        
        case RDMA_INPUT_FORMAT_YVYU :
		{
			return swap ? "eYVYU" : "eYUY2";
		}
        case RDMA_INPUT_FORMAT_YUYV :
		{
			return "eYUY2";
		}	        
        default:
		{
			DDPERR("rdma_intput_format_name unknow fmt=%d, swap=%d \n", fmt, swap);
			break;
    	}   
    }
    return "unknow";
}

static unsigned int rdma_index(DISP_MODULE_ENUM module)
{
    int idx = 0;
    switch(module) {
        case DISP_MODULE_RDMA0:
            idx = 0;
            break;
        case DISP_MODULE_RDMA1:
            idx = 1; 
            break;
        case DISP_MODULE_RDMA2:
            idx = 2; 
            break;
        default:
            DDPERR("invalid rdma module=%d \n", module);// invalid module
            ASSERT(0);
    }
    return idx;
}

int rdma_start(DISP_MODULE_ENUM module, void * handle) {
    unsigned int idx = rdma_index(module);
    ASSERT(idx <= 2);

    DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_INT_ENABLE, 0x3E);
    DISP_REG_SET_FIELD(handle,GLOBAL_CON_FLD_ENGINE_EN, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_GLOBAL_CON, 1);

    return 0;
}

int rdma_stop(DISP_MODULE_ENUM module,void * handle) {
    unsigned int idx = rdma_index(module);
    ASSERT(idx <= 2);

    DISP_REG_SET_FIELD(handle,GLOBAL_CON_FLD_ENGINE_EN, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_GLOBAL_CON, 0);
    DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_INT_ENABLE, 0);
    DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_INT_STATUS, 0);
    return 0;
}

int rdma_reset(DISP_MODULE_ENUM module,void * handle) {
    unsigned int delay_cnt=0;
    int ret =0;
    unsigned int idx = rdma_index(module);

    ASSERT(idx <= 2);

    DISP_REG_SET_FIELD(handle,GLOBAL_CON_FLD_SOFT_RESET, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_GLOBAL_CON, 1);
    while((DISP_REG_GET(idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_GLOBAL_CON)&0x700)==0x100)
    {
        delay_cnt++;
        udelay(10);
        if(delay_cnt>10000)
        {
            ret = -1;
            DDPERR("rdma%d_reset timeout, stage 1! DISP_REG_RDMA_GLOBAL_CON=0x%x \n", idx, DISP_REG_GET(idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_GLOBAL_CON));
            break;
        }
    }      
    DISP_REG_SET_FIELD(handle,GLOBAL_CON_FLD_SOFT_RESET, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_GLOBAL_CON, 0);
    delay_cnt =0;
    while((DISP_REG_GET(idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_GLOBAL_CON)&0x700)!=0x100)
    {
        delay_cnt++;
        udelay(10);
        if(delay_cnt>10000)
        {
            ret = -1;
            DDPERR("rdma%d_reset timeout, stage 2! DISP_REG_RDMA_GLOBAL_CON=0x%x \n", idx, DISP_REG_GET(idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_GLOBAL_CON));
            break;
        }
    }   
    return ret;
}

// set ultra registers
extern unsigned int gUltraEnable;
extern unsigned int rdma_ultra;
extern unsigned int rdma_ultra_high;
extern unsigned int rdma_ultra_low;
void rdma_set_ultra(unsigned int idx, unsigned int width, unsigned int height, unsigned int bpp, unsigned int frame_rate,void * handle)
{
  // constant
  static const unsigned int blank_overhead = 115; //it is 1.15, need to divide 100 later
  static const unsigned int rdma_fifo_width = 16; //in unit of byte
  static const unsigned int ultra_low_time      = 6; //in unit of us
  static const unsigned int pre_ultra_low_time  = 8; //in unit of us
  static const unsigned int pre_ultra_high_time = 9; //in unit of us
  static const unsigned int fifo_size = 512;
  static const unsigned int fifo_valid_line_ratio = 125; //valid size 1/8 line;
  static const unsigned int fifo_min_size = 32;
  // working variables
  unsigned int consume_levels_per_sec;
  unsigned int ultra_low_level;
  unsigned int pre_ultra_low_level;
  unsigned int pre_ultra_high_level;
  unsigned int ultra_high_ofs;
  unsigned int pre_ultra_low_ofs;
  unsigned int pre_ultra_high_ofs;
  unsigned int fifo_valid_size = 16;

  /* compute fifo valid size */

  fifo_valid_size = (width * bpp * fifo_valid_line_ratio) / (rdma_fifo_width * 1000);
  fifo_valid_size = fifo_valid_size > fifo_min_size ? fifo_valid_size : fifo_min_size;
  //consume_levels_per_sec = ((long long unsigned int)width*height*frame_rate*blank_overhead*bpp)/rdma_fifo_width/100;
  //change calculation order to prevent overflow of unsigned int
  consume_levels_per_sec = (width*height*frame_rate*bpp/rdma_fifo_width/100)*blank_overhead;
  
  // /1000000 for ultra_low_time in unit of us
  ultra_low_level      = (unsigned int)(ultra_low_time      * consume_levels_per_sec / 1000000 ); 
  pre_ultra_low_level  = (unsigned int)(pre_ultra_low_time  * consume_levels_per_sec / 1000000 ); 
  pre_ultra_high_level = (unsigned int)(pre_ultra_high_time * consume_levels_per_sec / 1000000 ); 
  
  pre_ultra_low_ofs = pre_ultra_low_level - ultra_low_level;
  ultra_high_ofs = 1; 
  pre_ultra_high_ofs = pre_ultra_high_level - pre_ultra_low_level ;

  if(gUltraEnable==1)
  {
	  ultra_low_level = 0x6b;
	  pre_ultra_low_ofs = 0xa0;
	  ultra_high_ofs = 1;
	  pre_ultra_high_ofs = 1;
  }
  
  //write ultra_low_level, ultra_high_ofs, pre_ultra_low_ofs, pre_ultra_high_ofs into register DISP_RDMA_MEM_GMC_SETTING_0
  DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_GMC_SETTING_0, 
    ultra_low_level|(pre_ultra_low_ofs<<8)|(ultra_high_ofs<<16)|(pre_ultra_high_ofs<<24));

  // set rdma ultra/pre-ultra according to resolution
#if 0  
  if(idx==0)
  {
      if(width>=1080)
      {
          DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_GMC_SETTING_0, 0x1b01356b);
      }
      else if(width>=720)
      {
          DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_GMC_SETTING_0, 0x0b011730);
      }
      else
      {
          DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_GMC_SETTING_0, 0x08010f20);
      }
  }
  else
  {
      if(width>=1080)
      {
          DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_GMC_SETTING_0, 0x0e011b38);
      }
      else if(width>=720)
      {
          DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_GMC_SETTING_0, 0x0e011b38);
      }
  }
#else
  DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_GMC_SETTING_0, 0x1b01356b);
#endif

  
  DISP_REG_SET_FIELD(handle,FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_FIFO_CON, fifo_valid_size);

  DDPDBG("FIFO_VALID_Size      = 0x%03x = %d\n", fifo_valid_size     , fifo_valid_size     );
  DDPDBG("ultra_low_level      = 0x%03x = %d\n", ultra_low_level     , ultra_low_level     ); 
  DDPDBG("pre_ultra_low_level  = 0x%03x = %d\n", pre_ultra_low_level , pre_ultra_low_level );
  DDPDBG("pre_ultra_high_level = 0x%03x = %d\n", pre_ultra_high_level, pre_ultra_high_level);
  DDPDBG("ultra_high_ofs       = 0x%03x = %d\n", ultra_high_ofs      , ultra_high_ofs      );
  DDPDBG("pre_ultra_low_ofs    = 0x%03x = %d\n", pre_ultra_low_ofs   , pre_ultra_low_ofs   );
  DDPDBG("pre_ultra_high_ofs   = 0x%03x = %d\n", pre_ultra_high_ofs  , pre_ultra_high_ofs  );
}

// fixme: spec has no RDMA format, fix enum definition here
int rdma_config(DISP_MODULE_ENUM module,
                    enum RDMA_MODE mode,
                    unsigned long address,
                    DpColorFormat inFormat, 
                    unsigned pitch,
                    unsigned width, 
                    unsigned height, 
                    unsigned ufoe_enable,
                    void * handle)  // ourput setting
{

    unsigned int output_is_yuv = 0; 
    enum RDMA_INPUT_FORMAT inputFormat = rdma_input_format_convert(inFormat);
    unsigned int  bpp                  = rdma_input_format_bpp(inputFormat);
    unsigned int input_is_yuv          = rdma_input_format_color_space(inputFormat);
    unsigned int input_swap            = rdma_input_format_byte_swap(inputFormat);  
    unsigned int input_format_reg      = rdma_input_format_reg_value(inputFormat);
    unsigned int  color_matrix         = 0x4; //0100 MTX_JPEG_TO_RGB (YUV FUll TO RGB)
    unsigned int idx = rdma_index(module);
    
    DDPDBG("RDMAConfig idx %d, mode %d, address 0x%x, inputformat %s, pitch %u, width %u, height %u\n",
                         idx, mode, address, rdma_intput_format_name(inputFormat,input_swap), pitch,width, height);
    ASSERT(idx <= 2);
    if((width > RDMA_MAX_WIDTH) || (height > RDMA_MAX_HEIGHT))
    {
    	  DDPERR("RDMA input overflow, w=%d, h=%d, max_w=%d, max_h=%d\n", width, height, RDMA_MAX_WIDTH, RDMA_MAX_HEIGHT);
    }
    if(input_is_yuv==1 && output_is_yuv==0)
    {
	    DISP_REG_SET_FIELD(handle,SIZE_CON_0_FLD_MATRIX_ENABLE, idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0, 1);
	    DISP_REG_SET_FIELD(handle,SIZE_CON_0_FLD_MATRIX_INT_MTX_SEL, idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0, color_matrix);
    }
    else if(input_is_yuv==0 && output_is_yuv==1)
    {
        color_matrix = 0x2; //0x0010, RGB_TO_BT601
	    DISP_REG_SET_FIELD(handle,SIZE_CON_0_FLD_MATRIX_ENABLE, idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0, 1);
	    DISP_REG_SET_FIELD(handle,SIZE_CON_0_FLD_MATRIX_INT_MTX_SEL, idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0, color_matrix);
    }
    else 
    {
        DISP_REG_SET_FIELD(handle,SIZE_CON_0_FLD_MATRIX_ENABLE, idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0, 0);
        DISP_REG_SET_FIELD(handle,SIZE_CON_0_FLD_MATRIX_INT_MTX_SEL, idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_SIZE_CON_0, 0);
    }

    DISP_REG_SET_FIELD(handle,GLOBAL_CON_FLD_MODE_SEL, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_GLOBAL_CON, mode);
    // FORMAT & SWAP only works when RDMA memory mode, set both to 0 when RDMA direct link mode.
    DISP_REG_SET_FIELD(handle,MEM_CON_FLD_MEM_MODE_INPUT_FORMAT, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_CON, ((mode == RDMA_MODE_DIRECT_LINK) ? 0 : input_format_reg&0xf));
    DISP_REG_SET_FIELD(handle,MEM_CON_FLD_MEM_MODE_INPUT_SWAP, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_CON, ((mode == RDMA_MODE_DIRECT_LINK) ? 0 : input_swap));
    DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_START_ADDR, address);
    DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_MEM_SRC_PITCH, pitch);
    DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_INT_ENABLE, 0x3F);
    DISP_REG_SET_FIELD(handle,SIZE_CON_0_FLD_OUTPUT_FRAME_WIDTH, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_SIZE_CON_0, width);
    DISP_REG_SET_FIELD(handle,SIZE_CON_1_FLD_OUTPUT_FRAME_HEIGHT, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_SIZE_CON_1, height);

    rdma_set_ultra(idx, width, height, bpp, rdma_fps[idx], handle);
#if 1
    if(ufoe_enable==0)  //UFOE bypassed, enable RDMA underflow intr, else disable RDMA underflow intr
    {
	    DISP_REG_SET_FIELD(handle,FIFO_CON_FLD_FIFO_UNDERFLOW_EN, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_FIFO_CON, 1);	    
        DISP_REG_SET_FIELD(handle,FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_FIFO_CON, 16);
    }
    else
    {
	    DISP_REG_SET_FIELD(handle,FIFO_CON_FLD_FIFO_UNDERFLOW_EN, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_FIFO_CON, 1);	    
        DISP_REG_SET_FIELD(handle,FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_FIFO_CON, width*3*3/16/2); //FHD:304, HD:203, QHD:151
    }
#endif
    return 0;
}

void rdma_set_target_line(DISP_MODULE_ENUM module, unsigned int line,void * handle)
{
    unsigned int idx = rdma_index(module);

    DISP_REG_SET(handle,idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_TARGET_LINE, line);
}

static int rdma_clock_on(DISP_MODULE_ENUM module,void * handle)
{
    unsigned int idx = rdma_index(module);
#ifdef ENABLE_CLK_MGR
    enable_clock(MT_CG_DISP0_DISP_RDMA0+idx , "DDP");
#endif
    DDPMSG("rdma_%d_clock_on CG 0x%x \n",idx, DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0)); 
    return 0;
}
static int rdma_clock_off(DISP_MODULE_ENUM module,void * handle)
{
    unsigned int idx = rdma_index(module);
#ifdef ENABLE_CLK_MGR
    disable_clock(MT_CG_DISP0_DISP_RDMA0+idx , "DDP");
#endif
    DDPMSG("rdma_%d_clock_off CG 0x%x \n",idx, DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));     
    return 0;
}

static int rdma_init(DISP_MODULE_ENUM module,void * handle)
{
    unsigned int idx = rdma_index(module);
#ifdef ENABLE_CLK_MGR
    enable_clock(MT_CG_DISP0_DISP_RDMA0+idx , "DDP");
#endif
    DDPMSG("rdma%d_init CG 0x%x \n",idx, DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0)); 
    return 0;
}
static int rdma_deinit(DISP_MODULE_ENUM module,void * handle)
{
    unsigned int idx = rdma_index(module);
#ifdef ENABLE_CLK_MGR
    disable_clock(MT_CG_DISP0_DISP_RDMA0+idx , "DDP");
#endif
    DDPMSG("rdma%d_deinit CG 0x%x \n",idx, DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));    
    return 0;
}

void rdma_dump_reg(DISP_MODULE_ENUM module)
{
    unsigned int idx = rdma_index(module);
    DDPDUMP("== DISP RDMA%d REGS ==\n", idx); 
    DDPDUMP("(0x000)R_INTEN       =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_INT_ENABLE+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x004)R_INTS        =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_INT_STATUS+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x010)R_CON         =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_GLOBAL_CON+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x014)R_SIZE0       =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x018)R_SIZE1       =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_1+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x01c)R_TAR_LINE    =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_TARGET_LINE+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x024)R_M_CON       =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_MEM_CON+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0xf00)R_M_S_ADDR    =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x02c)R_M_SRC_PITCH =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_MEM_SRC_PITCH+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x030)R_M_GMC_SET0  =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_0+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x034)R_M_SLOW_CON  =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_MEM_SLOW_CON+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x038)R_M_GMC_SET1  =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_1+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x040)R_FIFO_CON    =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_FIFO_CON+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x044)R_FIFO_LOG    =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_FIFO_LOG+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x078)R_PRE_ADD0    =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_PRE_ADD_0+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x07c)R_PRE_ADD1    =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_PRE_ADD_1+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x080)R_PRE_ADD2    =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_PRE_ADD_2+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x084)R_POST_ADD0   =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_POST_ADD_0+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x088)R_POST_ADD1   =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_POST_ADD_1+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x08c)R_POST_ADD2   =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_POST_ADD_2+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x090)R_DUMMY       =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_DUMMY+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x094)R_OUT_SEL     =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_DEBUG_OUT_SEL+DISP_RDMA_INDEX_OFFSET*idx));
    DDPDUMP("(0x094)R_M_START     =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR+DISP_RDMA_INDEX_OFFSET*idx));
    DDPDUMP("(0x0f0)R_IN_PXL_CNT  =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_IN_P_CNT+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x0f4)R_IN_LINE_CNT =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_IN_LINE_CNT+DISP_RDMA_INDEX_OFFSET*idx));             
    DDPDUMP("(0x0f8)R_OUT_PXL_CNT =0x%x\n", DISP_REG_GET(DISP_REG_RDMA_OUT_P_CNT+DISP_RDMA_INDEX_OFFSET*idx));
    DDPDUMP("(0x0fc)R_OUT_LINE_CNT=0x%x\n", DISP_REG_GET(DISP_REG_RDMA_OUT_LINE_CNT+DISP_RDMA_INDEX_OFFSET*idx));

    return;
}
extern unsigned long long rdma_start_time[2];
extern unsigned long long rdma_end_time[2];

extern unsigned int rdma_start_irq_cnt[2];
extern unsigned int rdma_done_irq_cnt[2];
extern unsigned int rdma_underflow_irq_cnt[2];
extern unsigned int rdma_targetline_irq_cnt[2];
void rdma_dump_analysis(DISP_MODULE_ENUM module)
{
    unsigned int idx = rdma_index(module);
    DDPDUMP("==DISP RDMA%d ANALYSIS==\n", idx); 
    DDPDUMP("rdma%d: en=%d, memory mode=%d, w=%d, h=%d, pitch=%d, addr=0x%x, fmt=%s, fifo_min=%d, \
    in_p_cnt=%d, in_l_cnt=%d, out_p_cnt=%d, out_l_cnt=%d, rdma_start_time=%lld ns,rdma_end_time=%lld ns\n",

        idx,
        DISP_REG_GET(DISP_REG_RDMA_GLOBAL_CON+DISP_RDMA_INDEX_OFFSET*idx)&0x1,
        (DISP_REG_GET(DISP_REG_RDMA_GLOBAL_CON+DISP_RDMA_INDEX_OFFSET*idx)&0x2)?1:0,
        DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0+DISP_RDMA_INDEX_OFFSET*idx)&0xfff,
        DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_1+DISP_RDMA_INDEX_OFFSET*idx)&0xfffff,
        DISP_REG_GET(DISP_REG_RDMA_MEM_SRC_PITCH+DISP_RDMA_INDEX_OFFSET*idx), 
        DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR+DISP_RDMA_INDEX_OFFSET*idx),
        rdma_intput_format_name(
            (DISP_REG_GET(DISP_REG_RDMA_MEM_CON+DISP_RDMA_INDEX_OFFSET*idx)>>4)&0xf,
            (DISP_REG_GET(DISP_REG_RDMA_MEM_CON+DISP_RDMA_INDEX_OFFSET*idx)>>8)&0x1
            ),
        DISP_REG_GET(DISP_REG_RDMA_FIFO_LOG+DISP_RDMA_INDEX_OFFSET*idx),
        DISP_REG_GET(DISP_REG_RDMA_IN_P_CNT+DISP_RDMA_INDEX_OFFSET*idx),
        DISP_REG_GET(DISP_REG_RDMA_IN_LINE_CNT+DISP_RDMA_INDEX_OFFSET*idx),
        DISP_REG_GET(DISP_REG_RDMA_OUT_P_CNT+DISP_RDMA_INDEX_OFFSET*idx),
        DISP_REG_GET(DISP_REG_RDMA_OUT_LINE_CNT+DISP_RDMA_INDEX_OFFSET*idx),
        rdma_start_time[idx],
        rdma_end_time[idx]
    );
    DDPDUMP("irq cnt: start=%d, end=%d, underflow=%d, targetline=%d \n", 
      rdma_start_irq_cnt[idx], rdma_done_irq_cnt[idx], rdma_underflow_irq_cnt[idx], rdma_targetline_irq_cnt[idx]);
      
    return ;
}

 static int rdma_dump(DISP_MODULE_ENUM module,int level)
 {
     rdma_dump_analysis(module);
     rdma_dump_reg(module);
     
     return 0;
 }

 void rdma_get_address(DISP_MODULE_ENUM module, unsigned long * addr)
 {
     unsigned int idx = rdma_index(module);
     *addr  =  DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR+DISP_RDMA_INDEX_OFFSET*idx);
     return ;
 }

 void rdma_get_info(int idx, RDMA_BASIC_STRUCT * info)
{
    RDMA_BASIC_STRUCT *p = info;
	p->addr  =     DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR+DISP_RDMA_INDEX_OFFSET*idx);
	p->src_w =     DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0+DISP_RDMA_INDEX_OFFSET*idx)&0xfff;
	p->src_h =     DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_1+DISP_RDMA_INDEX_OFFSET*idx)&0xfffff,	   
    p->bpp   =     rdma_input_format_bpp((DISP_REG_GET(DISP_REG_RDMA_MEM_CON+DISP_RDMA_INDEX_OFFSET*idx)>>4)&0xf);
    return ;
}

static int rdma_config_l(DISP_MODULE_ENUM module, disp_ddp_path_config* pConfig, void* handle)
{
	RDMA_CONFIG_STRUCT * r_config = & pConfig->rdma_config;
	enum RDMA_MODE mode = r_config->address ? RDMA_MODE_MEMORY : RDMA_MODE_DIRECT_LINK;
	LCM_PARAMS *lcm_param = &(pConfig->dispif_config);
        
	if(pConfig->dst_dirty)
	{
		if(pConfig->fps)
		{
			rdma_fps[rdma_index(module)] = pConfig->fps/100;
		}
	    	//config to direct link mode
        	rdma_config(module, 
			mode, //RDMA_MODE_DIRECT_LINK, //  link mode
			(mode == RDMA_MODE_DIRECT_LINK) ? 0 : r_config->address,			// address
			(mode == RDMA_MODE_DIRECT_LINK) ? eRGB888 : r_config->inputFormat,  // inputFormat
			(mode == RDMA_MODE_DIRECT_LINK) ? 0 : r_config->pitch,				// pitch
			pConfig->dst_w,        // width 
			pConfig->dst_h,        // height
			lcm_param->dsi.ufoe_enable,
			handle);
	}	
	else if(pConfig->rdma_dirty)
	{
		if(pConfig->fps)
		{
			rdma_fps[rdma_index(module)] = pConfig->fps/100;
		}
	
    		// decouple mode may use
    		rdma_config(module, 
			mode,                                                                  //  link mode
			(mode == RDMA_MODE_DIRECT_LINK) ? 0 : r_config->address,            // address
			(mode == RDMA_MODE_DIRECT_LINK) ? eRGB888 : r_config->inputFormat,  // inputFormat
			(mode == RDMA_MODE_DIRECT_LINK) ? 0 : r_config->pitch,              // pitch
			r_config->width,                                                    // width 
			r_config->height,                                                   // height
			lcm_param->dsi.ufoe_enable,
			handle);
	}

    	return 0;
}

void disp_rdma_enable_preultra(unsigned int enable)
{
    if(enable==1)
        rdma_ultra = rdma_ultra_high;
    else
        rdma_ultra = rdma_ultra_low;
        
    DDPMSG("rdma_ultra set to 0x%x. \n", rdma_ultra);
}

DDP_MODULE_DRIVER ddp_driver_rdma =
{
    .init            = rdma_init,
    .deinit          = rdma_deinit,
	.config          = rdma_config_l,
	.start 	         = rdma_start,
	.trigger         = NULL,
	.stop	         = rdma_stop,
	.reset           = rdma_reset,
	.power_on        = rdma_clock_on,
    .power_off       = rdma_clock_off,
    .is_idle         = NULL,
    .is_busy         = NULL,
    .dump_info       = rdma_dump,
    .bypass          = NULL,
    .build_cmdq      = NULL,
    .set_lcm_utils   = NULL,
}; 
#ifdef CONFIG_MTK_SEGMENT_TEST
int rdma_resolution_test(DISP_MODULE_ENUM module,unsigned int fw,unsigned fh)
{
	int ret=0;
	unsigned int idx = rdma_index(module);
	
	int pw = DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0+DISP_RDMA_INDEX_OFFSET*idx)&0xfff;
	int ph = DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_1+DISP_RDMA_INDEX_OFFSET*idx)&0xfffff;

    //reset rdma
	ret=rdma_reset(module,NULL);
	if(ret!=0)
	{
		printk("reset fail\n");
		return -1;
	}

	DISP_REG_SET_FIELD(NULL,SIZE_CON_0_FLD_OUTPUT_FRAME_WIDTH, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_SIZE_CON_0, pw);
    DISP_REG_SET_FIELD(NULL,SIZE_CON_1_FLD_OUTPUT_FRAME_HEIGHT, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_SIZE_CON_1, ph);
	DISP_REG_SET_FIELD(NULL,SIZE_CON_0_FLD_OUTPUT_FRAME_WIDTH, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_SIZE_CON_0, fw);
    DISP_REG_SET_FIELD(NULL,SIZE_CON_1_FLD_OUTPUT_FRAME_HEIGHT, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_SIZE_CON_1, fh);

	int w = DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0+DISP_RDMA_INDEX_OFFSET*idx)&0xfff;
    int h = DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_1+DISP_RDMA_INDEX_OFFSET*idx)&0xfffff;
    if((w!=pw) && (h!=ph))
    {
        printk("w=%d,h=%d,pw=%d,ph=%d,fw=%d,fh=%d\n",w,h,pw,ph,fw,fh);
		DISP_REG_SET_FIELD(NULL,SIZE_CON_0_FLD_OUTPUT_FRAME_WIDTH, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_SIZE_CON_0, pw);
    	DISP_REG_SET_FIELD(NULL,SIZE_CON_1_FLD_OUTPUT_FRAME_HEIGHT, idx*DISP_RDMA_INDEX_OFFSET+ DISP_REG_RDMA_SIZE_CON_1, ph);
		ret =-1;
    }
	
	return ret;
}
#endif
