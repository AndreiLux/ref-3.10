/******************************************************************************

			  版权所有 (C), 2001-2011, 华为技术有限公司

******************************************************************************
文 件 名   : drv_mailbox_ifc.c
版 本 号   : 初稿
作	  者   : 莫南 00176101
生成日期   : 2013年4月11日
最近修改   :
功能描述   : mailbox&跨核邮箱驱动软件，核间调用主体代码。
函数列表   :

修改历史   :
1.日	期	 : 2013年4月11日
作	  者   : 莫南 00176101
修改内容   : 创建文件
******************************************************************************/

/*****************************************************************************
1 头文件包含
*****************************************************************************/
#include "drv_mailbox_cfg.h"
#include "drv_mailbox_debug.h"
#include "drv_mailbox_gut.h"
#include "drv_mailbox_ifc.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
	可维可测信息中包含的C文件编号宏定义
*****************************************************************************/
#undef	_MAILBOX_FILE_
#define _MAILBOX_FILE_	 "ifc"
/*****************************************************************************
  2 全局变量定义
*****************************************************************************/
/*远程函数调用规则与约束:
  远程调用接口形式不固定，接口定义开放给用户，本模块提供一种机制:
	用户可以按自己的实际需要直接定义并生成任意一个远程函数，且此远程函数有以下约束：
	1.	函数的返回值只能为long型，不能返回指针。
	2.	函数的参数目前最多支持6个。
	3.	函数的参数实际上都被看成指针处理，只能有如下6种类型的参数：
		1>	固定大小的(指针)入参，其大小在函数生成的时候被定义并生效。
		2>	固定大小的(指针)出参，其大小在函数生成的时候被定义并生效。
		3>	固定大小的(指针)双参，其大小在函数生成的时候被定义并生效。
		4>	可变大小的(指针)入参，其长度由下一个固定大小的参数(常量入参)定义，
			在函数被调用时生效。
		5>	可变大小的(指针)出参，其长度由下一个固定大小的参数(常量入参)定义，
			在函数被调用时生效。
		6>	可变大小的(指针)双参)，其长度由下一个固定大小的参数(常量入参)定义，
			在函数被调用时生效。
		7>	常量入参。
	4.	如果某个参数为可变大小参数，那么他的下一个参数必需是固定大小的参数(long型)，
		 并且后者在函数调用过程中将描述前者的大小。
	5.	函数调用可配置为等待核间返回的调用和不等待核间返回的调用（丢弃式调用）。
	6.	注意(入参+双参)的大小或者(出参+双参)的大小不能超过邮箱单个邮件的最大大小。
*/

/*
	根据发送邮编号(send code)，获得接收邮编号(receive code)
*/
long mailbox_ifc_get_rcode(unsigned long scode)
{
	unsigned long src_id	  = mailbox_get_src_id(scode);
	unsigned long dst_id	   = mailbox_get_dst_id(scode);
	/*unsigned long carrier_id	 = mailbox_get_carrier_id(scode);*/

	/*所有ifc接收邮编号固定使用反向对称0号buff通道的0号应用(此通道的0偏移应用号),
	  需要保证每个ifc通道对应存在一个反方向的0号通道。这个通道的邮编都只能做单向用途，
	  (只发送消息或者只调用无返回的函数)*/
	return ((dst_id << MAILBOX_ID_SRC_CPU_OFFSET)
			|(src_id << MAILBOX_ID_DST_CPU_OFFSET)
			/*|(carrier_id << MAILBOX_ID_CHANNEL_OFFSET)*/
		   );
}

long mailbox_ifc_buf_pad(struct ifc_mb *ifc_b, unsigned long datalen)
{
	/* 重要:此处需要检查in arg buff空间是否为线性，如果为非线性的话，
	   需要自动填充空白至fifo的末端，以保证函数执行参数空间的连续性*/
	struct mb_queue   *que = (MAILBOX_USER_QUEUE(ifc_b->mbuf));
	unsigned long talsize;

	talsize = sizeof(struct ifc_head) + datalen;
	/*ifc_b->head.lmsg = talsize;*/

	if ((que->front + sizeof(struct ifc_head)) >= que->base + que->length) {
		/*消息头反转,线性，*/
		ifc_b->head.data = que->front + sizeof(struct ifc_head) - que->length;
	} else	{
		if ((que->front + talsize) >= que->base + que->length) {
			/*data区间反转，非线性*/
			ifc_b->head.data = que->base;
			talsize += (que->base + que->length) -	/*填充尾部空间*/
					   (que->front + sizeof(struct ifc_head));
		} else { /*正常情况,线性*/
			ifc_b->head.data = que->front + sizeof(struct ifc_head);
		}
	}

	/*在线性的前提下，还要保证空间足够*/
	if( talsize > mailbox_queue_left(que->rear, que->front, que->length)) {
		return mailbox_logerro_p1(MAILBOX_FULL, datalen);;
	}

	ifc_b->total_size = talsize;
	ifc_b->data_size = datalen;
	return MAILBOX_OK;
}

long mailbox_ifc_send_buf(struct ifc_mb *ifc_b)
{
	unsigned long ret_val = 0;
	unsigned long ifc_data = 0;
	struct mb_queue   *que = (MAILBOX_USER_QUEUE(ifc_b->mbuf));

	ifc_data = ifc_b->head.data;/*本核ifc线性数据地址*/

	/*ifc发送buff的线性数据地址将被远端执行核使用，需要转换成物理地址*/
	ifc_b->head.data = mailbox_virt_to_phy(ifc_b->head.data);

	/*把ifc消息头写入缓存*/
	mailbox_write_buff(que, (char *)&ifc_b->head, sizeof(struct ifc_head));

	/*并且更新邮箱写指针到ifc数据末尾*/
	que->front = ifc_data + ifc_b->data_size;

	if (MAILBOX_OK == mailbox_sealup_buff( ifc_b->mbuf,  ifc_b->total_size)) {
		ret_val = mailbox_send_buff(ifc_b->mbuf);
#ifdef _DRV_LLT_
		st_mailbox_notify(ifc_b->head.scode);
#endif
	} else {
		ret_val = MAILBOX_ERRO;
	}

	return ret_val;
}

void mailbox_ifc_waiter(void *mbuf, void *usrhandle, void *data)
{
	struct ifc_mb *ifc_b = (struct ifc_mb *)usrhandle;
	struct mb_queue *queue = MAILBOX_USER_QUEUE(mbuf);
	struct ifc_head  head_r ;
	long i;

	mailbox_read_buff(queue, (char*)&head_r, sizeof(struct ifc_head));

	if (head_r.stamp == ifc_b->head.stamp) { /*通过时间戳过滤之前的超时响应*/
		for ( i = 0; i < IFC_MAX_ARG ;	i++ ) {
			if (ifc_b->argo[i].addr) { /*拷贝出参*/
				mailbox_memcpy((void *) ifc_b->argo[i].addr,
							   (const void *)mailbox_phy_to_virt(head_r.argv[i]),
							   ifc_b->argo[i].size);
			}
		}

		ifc_b->head.retval = head_r.retval;
		mailbox_complete(&ifc_b->waitsem);
	}
}

/*发起调用消息，并等待调用返回*/
long mailbox_ifc_send_wait(struct ifc_mb *ifc_b, unsigned long timeout)
{
	unsigned long rcode = ifc_b->head.rcode;

	if (MAILBOX_OK != mailbox_register_cb(rcode , mailbox_ifc_waiter, ifc_b,
										  MAILBOX_NULL)) {
		goto wait_exit;
	}

	ifc_b->waitsem = mailbox_init_completion();

	if (MAILBOX_OK != mailbox_ifc_send_buf(ifc_b)) {
		mailbox_complete(&ifc_b->waitsem);
	}

	if (MAILBOX_OK != mailbox_wait_completion(&ifc_b->waitsem, timeout)) {
		ifc_b->head.retval = MAILBOX_TIME_OUT;

		/*超时需要注销回调，否者后面可能会延迟响应,踩ifc_b栈空间的地址*/
		mailbox_register_cb(rcode , MAILBOX_NULL, MAILBOX_NULL,
							MAILBOX_NULL);
	}

wait_exit:
	mailbox_release_buff(ifc_b->mbuf);
	return ifc_b->head.retval;
}


/*发起调用消息，并等待调用返回*/
long mailbox_ifc_send_no_wait(struct ifc_mb *ifc_b)
{
	long ret = mailbox_ifc_send_buf(ifc_b);

	ret |=	mailbox_release_buff(ifc_b->mbuf);
	return ret;
}

/*丢弃，不执行本次邮箱发送*/
long mailbox_ifc_discard_buf(struct ifc_mb *ifc_b)
{
	return mailbox_release_buff(ifc_b->mbuf);
}

void mailbox_ifc_executer(void *mbuf, void *handle, void *data)
{
	long (*ife)(struct ifc_head * ifc_h) = handle;
	struct ifc_head  head;
	struct mb_queue *queue = MAILBOX_USER_QUEUE(mbuf);

	mailbox_read_buff(queue, (char*)&head, sizeof(struct ifc_head));

	/*调用核送过来的是物理地址,(函数mailbox_ifc_send_buf中)
	这里由执行核转换成虚拟地址后再使用*/
	head.data = mailbox_phy_to_virt(head.data);
	if (ife) {
		ife(&head);
	}
}

long mailbox_ifc_register_exec(
	unsigned long mailcode,
	long (*ife_cb)(struct ifc_head * ifc_h)
)
{
	return mailbox_register_cb(mailcode, mailbox_ifc_executer,
							   (void*)ife_cb, (void *)mailcode);
}

long mailbox_ifc_init_buf(
	struct ifc_mb *ifc_b,
	unsigned long mailcode,
	unsigned long direct,
	unsigned long inlen,
	unsigned long timeout)
{
	unsigned long ret_val;
	unsigned long try_times = 0;
	unsigned long go_on = MAILBOX_FALSE;

	mailbox_memset(ifc_b, 0x00, sizeof(struct ifc_mb));
	ifc_b->head.scode = mailcode;
	ifc_b->head.retval= MAILBOX_ERRO;

	if (timeout) {
		ifc_b->head.needret = MAILBOX_TRUE;
	}

	if (IFC_LAUNCH == direct) { /*函数调用,发送消息初始化*/
		ifc_b->head.rcode = mailbox_ifc_get_rcode(mailcode);  /*获得固定调用返回邮编号*/
		ifc_b->head.stamp = mailbox_get_timestamp(); /*获取时间戳进行响应校验*/
	} else if (IFC_RESPONSE == direct) {
		ifc_b->head.rcode = 0;
#ifdef _DRV_LLT_  /*pc工程使用返码邮编*/
		mailcode = ((mailcode & 0xff000000) >> 8) |
				   ((mailcode & 0x00ff0000) << 8) |
				   ((mailcode & 0x0000ffff)) ;
		ifc_b->head.scode = mailcode;
#endif
	}

	do { /*满等待延迟重试*/
		ret_val = mailbox_request_buff(mailcode, (struct mb_buff **)&ifc_b->mbuf);
		if (ifc_b->mbuf) {
			ret_val =  mailbox_ifc_buf_pad(ifc_b, inlen);
		}

		if (MAILBOX_FULL == ret_val) {
			go_on = mailbox_scene_delay(MAILBOX_DELAY_SCENE_IFC_FULL, &try_times);
			if (ifc_b->mbuf) {
				mailbox_release_buff(ifc_b->mbuf);
				ifc_b->mbuf = MAILBOX_NULL;
			}
		} else {
			go_on = MAILBOX_FALSE;
		}

	} while (MAILBOX_TRUE == go_on);

	if (MAILBOX_OK != ret_val) {
		if (MAILBOX_NULL != ifc_b->mbuf) {
			mailbox_release_buff(ifc_b->mbuf);
		}
		mailbox_show(mailcode,0);
		mailbox_assert(mailcode);
		return mailbox_logerro_p1(ret_val, mailcode);
	}

	return	MAILBOX_OK;
}

/*
以下函数是被ifc生成宏调用的参数，他们的作用是对用户定义或者传入的远程调用参数进行处理,
这里对他们部分通用类型进行统一说明
	db: 当前参数类型, 比如:IFC_INVAR
	ab: 当前参数数值, 可能是指针，也可能是变量
	sb: 当前参数size, 如果是定长指针，这里表示其指向的数据长度。
	df: 下一个参数类型
	af: 下一个参数数值
	sf: 下一个参数size
	in:　邮箱输入buffer，线性。
	out: 邮箱输出buffer,线性。
*/


/*保存用户传入的输出参数地址，在调用返回后，将从接收buffer拷贝出参内容
	ary: 保存用户传入的参数值，如果是出参，这个值会在返回的时候用到。
*/
void __ifc_arg_out(struct ifc_arg ** p_ary, long db, long ab, long sb, long af)
{
	struct ifc_arg * ary = *p_ary;
	if ((IFC_OUT & db) || (IFC_BI & db)) {
		ary->addr = ab; /*接收出参数地址，只在本核使用，不需要转换*/
		if (IFC_FIX & db) {
			ary->size = sb;
		} else if (IFC_VAR & db) {
			ary->size = af;
		}
	}
	*p_ary = *p_ary + 1;
}

/* 把远程函数的输入参数压入邮箱的发送通道buffer:
	av:保存入参值，如果有变长指针，执行核将使用这里保存的变长指针长度
*/
void __ifc_push_arg(unsigned long * in, unsigned long ** p_av, unsigned long db,
					unsigned long ab, unsigned long sb, unsigned long af)
{
	unsigned long *av = *p_av;
	*av = ab;
	if ((IFC_IN & db) || (IFC_BI & db)) {
		if (IFC_CNT & db) {
			*(unsigned long*)(*in) = ab;
			*in += ifc_align_size(sizeof(unsigned long), IFC_ALIGN);
		} else if (IFC_FIX & db) {
			mailbox_memcpy((void*)*in, (const void*)ab, sb);
			*in += ifc_align_size(sb, IFC_ALIGN);
		} else if (IFC_VAR & db) {
			mailbox_memcpy((void*)*in, (const void*)ab, af);
			*in += ifc_align_size(af, IFC_ALIGN);
		}
	}
	*p_av = *p_av + 1;
}

/* 计算输入buffer被ifc参数占用的空间 */
long __ifc_in_size(unsigned long db, unsigned long sb, unsigned long af)
{
	if ((IFC_IN & db) || (IFC_BI & db)) {
		if (IFC_CNT & db) {
			return ifc_align_size(sizeof(unsigned long), IFC_ALIGN);
		} else if (IFC_FIX & db) {
			return ifc_align_size(sb, IFC_ALIGN);
		} else if (IFC_VAR & db) {
			return ifc_align_size(af, IFC_ALIGN);
		}
	}
	return 0;
}

/* 计算输出buffer给ifc函数占用的空间
	pp_a:指向参数值数组中当前参数值的指针
*/
long __ifc_out_size(unsigned long db, unsigned long sb, unsigned long **pp_a)
{
	unsigned long size = 0;
	if ((IFC_OUT & db) || (IFC_BI & db)) {
		if (IFC_FIX & db) {
			size = ifc_align_size(sb, IFC_ALIGN);
		} else if (IFC_VAR & db) {
#ifdef C_CALL_ARG_R2L
			unsigned long v_sz = *(unsigned long*)(*pp_a - 1);
#else
			unsigned long v_sz = *(unsigned long*)(*pp_a + 1);
#endif
			size =	ifc_align_size(v_sz, IFC_ALIGN);
		}
	}
	*pp_a = *pp_a + 1;
	return size;
}

/*为远程真实执行函数生成参数值
	pp_a:指向参数值数组中当前参数值的指针
*/
long __ifc_gen_arg(unsigned long *in, unsigned long *out, unsigned long db,
				   unsigned long sb, unsigned long **pp_a)
{
	unsigned long ret_val = 0;
	unsigned long v_sz = 0;
	if (IFC_VAR & db) {
#ifdef C_CALL_ARG_R2L
		v_sz = *(unsigned long*)(*pp_a - 1);
#else
		v_sz = *(unsigned long*)(*pp_a + 1);
#endif
	}

	if (IFC_IN & db) {
		if (IFC_CNT & db) {
			ret_val = *(unsigned long*)*in;
			*in = *in + sizeof(unsigned long);
		} else if (IFC_FIX & db) {
			ret_val = *in;
			*in = *in + ifc_align_size(sb, IFC_ALIGN);
		} else if (IFC_VAR & db) {
			ret_val = *in;
			*in = *in + ifc_align_size(v_sz , IFC_ALIGN);
		}
	} else if (IFC_BI & db) {
		if (IFC_FIX & db) {
			ret_val = *in;
			*in = *in + ifc_align_size(sb, IFC_ALIGN);
			*out = *out + ifc_align_size(sb, IFC_ALIGN);
		} else if (IFC_VAR & db) {
			ret_val = *in;
			*in = *in + ifc_align_size(v_sz, IFC_ALIGN);
			*out= *out + ifc_align_size(v_sz, IFC_ALIGN);
		}
	} else if (IFC_OUT & db) {
		ret_val = *out;
		if (IFC_FIX & db) {
			*out = *out + ifc_align_size(sb, IFC_ALIGN);
		} else if (IFC_VAR & db) {
			*out = *out + ifc_align_size(v_sz, IFC_ALIGN);
		}
	}
	*pp_a = *pp_a + 1;
	return	ret_val;
}

/*远程函数执行完后，把双参内容拷贝到输出buffer的指针。
	pp_a:指向参数值数组中当前参数值的指针
*/
void __ifc_copy_arg(unsigned long *in, unsigned long *out, unsigned long db,
					unsigned long sb, unsigned long **p_av , unsigned long **p_ao)
{
	unsigned long cpsize = 0;
	unsigned long *av = *p_av;
	unsigned long *ao = *p_ao;

	*ao = 0;
	if (IFC_FIX & db) {
		cpsize = ifc_align_size(sb, IFC_ALIGN);
	} else if (IFC_VAR & db) {
#ifdef C_CALL_ARG_R2L
		unsigned long v_sz = *(av - 1);
#else
		unsigned long v_sz = *(av + 1);
#endif
		cpsize = ifc_align_size(v_sz, IFC_ALIGN);
	} else if (IFC_CNT & db) {
		cpsize = sizeof(unsigned int);
	}

	if (IFC_BI & db) {
		mailbox_memcpy((void*)(*out), (const void*)(*in), cpsize);
		*ao = mailbox_virt_to_phy(*out); /*在调用核mailbox_ifc_waiter()中被拷贝*/
		*out += cpsize;
		*in  += cpsize;
	} else if (IFC_IN & db) {
		*in += cpsize;
	} else if (IFC_OUT & db) {
		*ao = mailbox_virt_to_phy(*out);
		*out += cpsize;
	}

	*p_av = *p_av + 1;
	*p_ao = *p_ao + 1;
}


#ifdef IFC_DEBUG_PARAM
/*ifc可维可测，保存调用方的参数，并检查静态ifc配置的正确性
	head: ifc 输入buffer描述
*/
void __ifc_dbg_arg(struct ifc_head *head, unsigned long db, unsigned long sb,
				   unsigned long df)
{
	struct ifc_param *param = head->param_tbl;
	if ((db & IFC_FIX) &&  (0 == sb)) {
		mailbox_out(("ifc error: fix-size pointer zero length!\n")); /*错误，定长指针被配成了0长度*/
	} else if ((db & IFC_VAR) && (0 == (df & IFC_CNT))) {
		mailbox_out(("ifc error: variable-size pointer no length!\n")); /*错误，变长指针的下一个参数不是描述长度的类型*/
	}
	param[head->param_prob].type = db;
	param[head->param_prob].size = sb;
	head->param_prob++;
}

void __ifc_chk_arg(struct ifc_head *head, unsigned long db, unsigned long sb)
{
	struct ifc_param *param = head->param_tbl;
	int ret = 0;
	int i ;
	head->param_prob--; /*prob 计数已经在调用核置为最后一个参数，所以这里倒序进行检查*/
	if (head->param_prob < 0) {
		ret = 1;
		mailbox_out(("ifc error: param num not match!\n")); /*错误，调用方和执行方参数数目不一致*/
	}
	if (param[head->param_prob].type != db) {
		ret = 1;
		mailbox_out(("ifc error: param type not match!\n")); /*错误，调用方和执行方参数类型不一致*/
	}
	if ((IFC_FIX & db) && (param[head->param_prob].size != sb)) {
		ret = 1;
		mailbox_out(("ifc error: param size not match!\n")); /*错误，调用方和执行方参数指针长度不一致*/
	}
	if (0 != ret) {
		mailbox_out(("param list!\n"));
		for (i = 0; i < IFC_MAX_ARG; i++) {
			mailbox_out(("p%d, type:(0x%x), size =0x%x !\n", i,
						 (unsigned int)param[i].type, (unsigned int)param[i].size ));
		}
	}
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

