#include "slimport.h"
#include "slimport_tx_drv.h"
#include "slimport_tx_cec.h"

//for debug messgage
#ifdef CEC_DBG_MSG_ENABLED
#include <stdio.h>
#include <stdarg.h>
#endif

#ifdef CEC_ENABLE

#define CEC_DEVICE_TYPE     CEC_DEVICE_TYPE_PLAYBACK_DEVICE

unchar  g_CECRecvBuf[CEC_RECV_BUF_SIZE];
unchar  *g_pCECRecvHead;
unchar  *g_pCECRecvTail;
unchar  g_CECSendBuf[CEC_SEND_BUF_SIZE];
unchar  *g_pCECSendHead;
unchar  *g_pCECSendTail;

unchar  g_CECRecvBuf_HDMI[CEC_RECV_BUF_SIZE];
unchar  *g_pCECRecvHead_HDMI;
unchar  *g_pCECRecvTail_HDMI;
unchar  g_CECSendBuf_HDMI[CEC_SEND_BUF_SIZE];
unchar  *g_pCECSendHead_HDMI;
unchar  *g_pCECSendTail_HDMI;

unchar bCECStatus = CEC_NOT_READY;


struct tagCECFrame g_CECFrame;
struct tagCECFrame g_CECFrame_HDMI;

unchar g_LogicAddr;

unchar g_TimeOut;


#ifdef CEC_PHYCISAL_ADDRESS_INSERT
unchar downstream_physicaladdrh;
unchar downstream_physicaladdrl;
#endif




void cec_init(void)
{
    unsigned char c;
	unsigned char i;

    bCECStatus = CEC_NOT_READY;//set status to un-initialied.

    g_LogicAddr = CEC_DEVICE_TYPE;

    //set uptream CEC logic address to 0x0(worked as TV), reset CEC, set to RX
    sp_write_reg(RX_P0,RX_CEC_CTRL, 0x09);


    //set downtream CEC logic address to 0x04(worked as upstream DVD playback)
    //reset cec, enable RX
    c = 0x49;
	for(i=0;i<5;i++){
		if(AUX_OK == sp_tx_aux_dpcdwrite_bytes(0x00, 0x05, 0x70,1, &c))
			break;
		//else
		//	TRACE("RX CEC logic address set fail!\r\n");
	}

    //initial receive and transmitter buffer
    for(c=0;c<CEC_RECV_BUF_SIZE;c++)
    {
        g_CECRecvBuf[c] = 0;
        g_CECRecvBuf_HDMI[c] = 0;
    }

    for(c=0;c<CEC_SEND_BUF_SIZE;c++)
    {
        g_CECSendBuf[c] = 0;
        g_CECSendBuf_HDMI[c] = 0;
    }

    g_pCECRecvHead = g_CECRecvBuf;
    g_pCECRecvTail = g_CECRecvBuf;
    g_pCECSendHead = g_CECSendBuf;
    g_pCECSendTail = g_CECSendBuf;

    g_pCECRecvHead_HDMI= g_CECRecvBuf_HDMI;
    g_pCECRecvTail_HDMI= g_CECRecvBuf_HDMI;
    g_pCECSendHead_HDMI= g_CECSendBuf_HDMI;
    g_pCECSendTail_HDMI= g_CECSendBuf_HDMI;

#ifdef CEC_PHYCISAL_ADDRESS_INSERT
	downstream_physicaladdrh = 0xff;
	downstream_physicaladdrl = 0xff;
#endif


    bCECStatus = CEC_IS_READY;//set status toinitialied.
    g_TimeOut = 0;
}


void cec_status_set(CEC_STATUS cStatus)
{
    bCECStatus = cStatus;
}


CEC_STATUS cec_status_get(void)
{
    //*cStatus = bCECStatus ;
    return bCECStatus;
}



unchar downStream_hdmi_cec_writemessage(struct tagCECFrame *pFrame, unchar Len)
{
    unchar t;

    if(bCECStatus ==CEC_NOT_READY)
        return 0;

//#ifdef CEC_DBG_MSG_ENABLED
//        TRACE(">> ");
//
//        TRACE_ARRAY((unsigned char *)pFrame, Len);
//#endif
    t = downstream_cec_sendframe((unsigned char *)pFrame, Len);
    if(t == 0)
    {
//#ifdef CEC_DBG_MSG_ENABLED
//        TRACE(">> ");
//
//        TRACE_ARRAY((unsigned char *)pFrame, Len);
//#endif

    }
    return t;
}

void downstream_hdmi_cec_readmessage(void)
{
    struct tagCECFrame *pFrame;
    unsigned char len;



    if(bCECStatus ==CEC_NOT_READY)
        return;

    pFrame=&g_CECFrame;
    downstream_cec_readfifo();
    while((len = downstream_cec_recvframe((unsigned char *)pFrame)) != 0)
    {

#ifdef CEC_DBG_MSG_ENABLED

        TRACE("<< ");
        TRACE_ARRAY((unsigned char *)pFrame, len);
#endif
        if(len >= 1)
        {
            upstream_hdmi_cec_writemessage(&g_CECFrame,len);
			//DownStream_CEC_ParseMessage();

        }
    }

}






void downstream_cec_readfifo(void)
{
    unsigned char cstatus,c;
    unsigned char i;
    unsigned char FIFOLen;

	unsigned int time_out_counter;

	//if aux error , abort read
    if(AUX_ERR == sp_tx_aux_dpcdread_bytes(0x00,0x05,0x71,1,&cstatus))
		return;
//#ifdef CEC_DBG_MSG_ENABLED
    //pr_info("%s %s : CEC status =%.2x\n", LOG_TAG, __func__, (uint)cstatus);
//#endif

	time_out_counter = 0;

    do{

        FIFOLen = cstatus&0x1F;//get fifo count

        FIFOLen <<= 1;
        for(i = 0; i < FIFOLen; i++)
        {
            sp_tx_aux_dpcdread_bytes(0x00,0x05,0x80,1,&c);//cec fifo
            *g_pCECRecvHead = c;

            if(g_pCECRecvHead == g_CECRecvBuf + CEC_RECV_BUF_SIZE - 1)
            {
                g_pCECRecvHead = g_CECRecvBuf;
            }
            else
            {
                g_pCECRecvHead++;
            }
            if(g_pCECRecvHead == g_pCECRecvTail)
            {
		#ifdef CEC_DBG_MSG_ENABLED
		TRACE("buffer full!\r\n");
		#endif
            }
        }
        if(AUX_ERR == sp_tx_aux_dpcdread_bytes(0x00,0x05,0x71,1,&cstatus))
			return;

		time_out_counter++;
		if(time_out_counter >= TIME_OUT_THRESHOLD)
			break;

    }while((cstatus&0x80)|(cstatus&0x1F));//CEC busy or fifo data count not zero, go on reading
}

unsigned char downstream_cec_checkfullframe(void)
{
    unsigned char *pTmp;
    unsigned char i;
	unsigned int time_out_counter;

	time_out_counter = 0;
    i = 0;
    pTmp = g_pCECRecvTail;
    while(pTmp != g_pCECRecvHead)    // check if end of buffer
    {   // check frame header
        //TRACE("check fram header!\r\n");
        if(*pTmp != 0)
        {
            i++;
        }
        if(pTmp == g_CECRecvBuf + CEC_RECV_BUF_SIZE - 1)
        {
            pTmp = g_CECRecvBuf;
        }
        else
        {
            pTmp++;
        }
        if(pTmp == g_CECRecvBuf + CEC_RECV_BUF_SIZE - 1)
        {
            pTmp = g_CECRecvBuf;
        }
        else
        {
            pTmp++;
        }

		time_out_counter++;
		if(time_out_counter >= TIME_OUT_THRESHOLD)
			break;
    }


    if(i >= 1)
    {
        return 1; //one full frame found
    }
    else
    {
        return 0;//no frame found
    }
}
// 01 xx 00 xx 00 xx ... 00 xx 01 xx 00 xx
unsigned char downstream_cec_recvframe(unsigned char *pFrameData)
{
    unsigned char DataLen;

	unsigned int time_out_counter;

	time_out_counter = 0;


    DataLen = 0;
    //TRACE("API_CEC_RecvFram! \n");
    if(downstream_cec_checkfullframe())
    {
        do
        {
            if(g_pCECRecvTail == g_pCECRecvHead)
            {
                //pr_info("%s %s : head == tail\n", LOG_TAG, __func__);
                break;
            }

		if(g_pCECRecvTail == g_CECRecvBuf + CEC_RECV_BUF_SIZE - 1)
            {
                g_pCECRecvTail = g_CECRecvBuf;
            }
            else
            {
                g_pCECRecvTail++;
            }
            *pFrameData = *g_pCECRecvTail;
            DataLen++;
            pFrameData++;
            if(g_pCECRecvTail == g_CECRecvBuf + CEC_RECV_BUF_SIZE - 1)
            {
                g_pCECRecvTail = g_CECRecvBuf;
            }
            else
            {
                g_pCECRecvTail++;
            }

			time_out_counter++;
			if(time_out_counter >= TIME_OUT_THRESHOLD)
				break;
        }while(*g_pCECRecvTail == 0);
    }

    return DataLen;
}

unsigned char downstream_cec_sendframe(unsigned char *pFrameData, unsigned char Len)
{
    unsigned char i,j;
    unsigned char *pDataTmp;
    unsigned char LenTmp;
    unsigned char c;

	unsigned int time_out_counter;
	time_out_counter = 0;
    // According to CEC 7.1, re-transmission can be attempted up to 5 times
    for(i = 5; i; i--)
    {
        for(pDataTmp = pFrameData, LenTmp = Len; LenTmp; LenTmp--)
        {
            //SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,0x80,1,&c);
            //*pDataTmp = c;
            c = *pDataTmp;
            sp_tx_aux_dpcdwrite_bytes(0x00, 0x05, 0x80,1, &c);
            //ANX7805_API_DPCDWriteByte(ANX7730_CEC_FIFO, *pDataTmp);
            pDataTmp++;
        }
        // make sure CEC rx is idle
        time_out_counter = 0;
        do
        {
            sp_tx_aux_dpcdread_bytes(0x00,0x05,0x71,1,&c);
           // TRACE("00571: %02BX\r\n", c);
            //RXSTReg.all = ANX7805_API_DPCDReadByte(ANX7730_CEC_RX_ST);

			time_out_counter++;
			if(time_out_counter >= TIME_OUT_THRESHOLD)
				break;
        }while(c&0x80);

        //TRACE("After For!");

        //clear tx done interrupt first
        sp_tx_aux_dpcdread_bytes(0x00,0x05,0x10,1,&c);
        c|=0x80;
        sp_tx_aux_dpcdwrite_bytes(0x00, 0x05, 0x10,1, &c);


        // switch to CEC tx mode
        for(j=0;j<20;j++){
	        //if(AUX_OK != sp_tx_aux_dpcdread_bytes(0x00,0x05,0x70,1,&c)){
			//	TRACE("switch to cec tx read error!");
			//	continue;
	        //}

			c = CEC_DEVICE_TYPE<<4;
	        //c&=0xf3;
	        c|=0x04;
	        if(AUX_OK != sp_tx_aux_dpcdwrite_bytes(0x00, 0x05, 0x70,1, &c)){
				//TRACE("switch to cec tx write error!");
				continue;
	        }
			else
				break;
        }

		time_out_counter = 0;
        do
        {
            if(AUX_OK == sp_tx_aux_dpcdread_bytes(0x00,0x05,0x10,1,&c)){
			time_out_counter++;
			if(time_out_counter >= 100*TIME_OUT_THRESHOLD)
				break;
            }else
		break;
        }while(!(c&0x80));

        //clear the CEC TX done interrupt
        c = 0x80;
        sp_tx_aux_dpcdwrite_bytes(0x00, 0x05, 0x10,1, &c);


        //Get CEC TX status
        sp_tx_aux_dpcdread_bytes(0x00,0x05,0x11,1,&c);
        //pr_info("%s %s : DPCD 00511 = %.2x\n",LOG_TAG, __func__, (uint)c);
        if((c&0x03)==0)
        //if(!IRQ3Reg.bits.CEC_TX_ERR && !IRQ3Reg.bits.CEC_TX_NO_ACK)
        {
            break;
        }
        else
        {
            //clear the CEC error interrupt
            c=0x03;
            sp_tx_aux_dpcdwrite_bytes(0x00, 0x05, 0x11,1, &c);
            //reset CEC
            //sp_tx_aux_dpcdread_bytes(0x00,0x05,0x70,1,&c);
            //c|=0x01;
			c = CEC_DEVICE_TYPE<<4;
	        //c&=0xf3;
	        c|=0x05;
            sp_tx_aux_dpcdwrite_bytes(0x00, 0x05, 0x70,1, &c);

        }
    }

    // switch to CEC rx mode
    for(j=0;j<20;j++){
        //if(AUX_OK != sp_tx_aux_dpcdread_bytes(0x00,0x05,0x70,1,&c)){
		//	TRACE("switch to cec rx read error!");
		//	continue;
        //}
	    //c&=0xf3;
	    //c|=0x08;


		c = CEC_DEVICE_TYPE<<4;
		c|=0x08;

		if(AUX_OK != sp_tx_aux_dpcdwrite_bytes(0x00, 0x05, 0x70,1, &c)){
			//TRACE("switch to cec rx write error!");
			continue;
		}
		else
			break;
    }

    if(i != 0)//send succeed
    {
        return 0;
    }
    else
    {
        return 1;
    }
}



//for HDMI RX CEC
unsigned char upstream_hdmi_cec_writemessage(struct tagCECFrame *pFrame, unsigned char Len)
{
    unsigned char t;

    if(bCECStatus == CEC_NOT_READY)
        return 0;

//#ifdef CEC_DBG_MSG_ENABLED
//    TRACE(">> ");
//    TRACE_ARRAY((unsigned char *)pFrame, Len);
//#endif

    t = upstream_cec_sendframe((unsigned char *)pFrame, Len);
    if(t == 0)
    {
//#ifdef CEC_DBG_MSG_ENABLED
//        TRACE(">> ");
//        TRACE_ARRAY((unsigned char *)pFrame, Len);
//#endif
    }
    return t;
}

void upstream_hdmi_cec_readmessage(void)
{
    struct tagCECFrame *pFrame;
    unsigned char len;



    if(bCECStatus == CEC_NOT_READY){
		//TRACE("cec not ready!");
		return;
    }

    pFrame=&g_CECFrame_HDMI;

    upstream_cec_readfifo();


    while((len = upstream_cec_recvframe((unsigned char *)pFrame)) != 0){
#ifdef CEC_DBG_MSG_ENABLED
        //TRACE_ARRAY(&len, 1);
        TRACE(">> ");
        TRACE_ARRAY((unsigned char *)pFrame, len);
#endif
        if(len >= 1){
#ifdef CEC_PHYCISAL_ADDRESS_INSERT
		uptream_cec_parsemessage();
#endif
            downStream_hdmi_cec_writemessage(&g_CECFrame_HDMI,len);
        }
    }
}



void upstream_cec_readfifo(void)
{
    unsigned char cStatus,c;
    unsigned char i;
    unsigned char FIFOLen;

	unsigned int time_out_counter;

	time_out_counter = 0;

    //Get CEC length
    sp_read_reg(RX_P0,HDMI_RX_CEC_RX_STATUS_REG, &cStatus);

//#ifdef CEC_DBG_MSG_ENABLED
    //pr_info("%s %s : HDMI CEC status =%.2x\n",LOG_TAG, __func__, (uint)cStatus);
//#endif
    //SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,0x71,1,&c);
    //pr_info("%s %s : CEC status =%.2x\n",LOG_TAG, __func__,(uint)c);

    do{
        FIFOLen = cStatus&0x0F;
        if((FIFOLen ==0)&&(cStatus&0x20))//Fifi full
        {
            FIFOLen = 0x10;
        }

        FIFOLen <<= 1;
        for(i = 0; i < FIFOLen; i++)
        {
            sp_read_reg(RX_P0,HDMI_RX_CEC_FIFO_REG, &c);
            *g_pCECRecvHead_HDMI= c;

            if(g_pCECRecvHead_HDMI== g_CECRecvBuf_HDMI+ CEC_RECV_BUF_SIZE - 1)
            {
                g_pCECRecvHead_HDMI= g_CECRecvBuf_HDMI;
            }
            else
            {
                g_pCECRecvHead_HDMI++;
            }
            if(g_pCECRecvHead_HDMI== g_pCECRecvTail_HDMI)
            {
				#ifdef CEC_DBG_MSG_ENABLED
                TRACE("buffer full!\r\n");
				#endif
            }
        }
        sp_read_reg(RX_P0,HDMI_RX_CEC_RX_STATUS_REG, &cStatus);

		time_out_counter++;
		if(time_out_counter>LOCAL_REG_TIME_OUT_THRESHOLD)
			break;

    }while((cStatus&0x80)|(cStatus&0x0F));

}

unsigned char upstream_cec_checkfullframe(void)
{
    unsigned char *pTmp;
    unsigned char i;

	unsigned int time_out_counter;

	time_out_counter = 0;

    i = 0;
    pTmp = g_pCECRecvTail_HDMI;
    while(pTmp != g_pCECRecvHead_HDMI)    // check if end of buffer
    {   // check frame header
        //TRACE("check fram header!\r\n");

        if(*pTmp != 0)
        {
            i++;
        }
        if(pTmp == g_CECRecvBuf_HDMI+ CEC_RECV_BUF_SIZE - 1)
        {
            pTmp = g_CECRecvBuf_HDMI;
        }
        else
        {
            pTmp++;
        }
        if(pTmp == g_CECRecvBuf_HDMI+ CEC_RECV_BUF_SIZE - 1)
        {
            pTmp = g_CECRecvBuf_HDMI;
        }
        else
        {
            pTmp++;
        }

		time_out_counter++;
		if(time_out_counter >= TIME_OUT_THRESHOLD)
			break;
    }

    if(i >= 1)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// 01 xx 00 xx 00 xx ... 00 xx 01 xx 00 xx
unsigned char upstream_cec_recvframe(unsigned char *pFrameData)
{
    unsigned char DataLen;

	unsigned int time_out_counter;

	time_out_counter = 0;

    DataLen = 0;


    if(upstream_cec_checkfullframe())
    {


        do
        {
            if(g_pCECRecvTail_HDMI == g_pCECRecvHead_HDMI)
            {
                //pr_info("%s %s : head == tail\n",LOG_TAG, __func__);
                break;
            }

            if(g_pCECRecvTail_HDMI== g_CECRecvBuf_HDMI+ CEC_RECV_BUF_SIZE - 1)
            {
                g_pCECRecvTail_HDMI= g_CECRecvBuf_HDMI;
            }
            else
            {
                g_pCECRecvTail_HDMI++;
            }


           *pFrameData = *g_pCECRecvTail_HDMI;
            DataLen++;
            pFrameData++;

            if(g_pCECRecvTail_HDMI== g_CECRecvBuf_HDMI+ CEC_RECV_BUF_SIZE - 1)
            {
                g_pCECRecvTail_HDMI= g_CECRecvBuf_HDMI;
            }
            else
            {
                g_pCECRecvTail_HDMI++;
            }

			time_out_counter++;
			if(time_out_counter >= TIME_OUT_THRESHOLD)
				break;
        }while(*g_pCECRecvTail_HDMI== 0);
    }

   return DataLen;
}



unsigned char upstream_cec_sendframe(unsigned char *pFrameData, unsigned char Len)
{
    unsigned char i;
    unsigned char *pDataTmp;
    unsigned char LenTmp;
    unsigned char c;

	unsigned int time_out_counter;

	time_out_counter = 0;
    //pr_info("%s %s : send to up len = %.2x\n",LOG_TAG, __func__,(unsigned char)Len);
    // According to CEC 7.1, re-transmission can be attempted up to 5 times
    for(i = 5; i; i--)
    {
        for(pDataTmp = pFrameData, LenTmp = Len; LenTmp; LenTmp--)
        {
            c = *pDataTmp;
            sp_write_reg(RX_P0,HDMI_RX_CEC_FIFO_REG,c);//write CEC FIFO
            //pr_info("%s %s : FD = %.2x\n",LOG_TAG, __func__,(unsigned char)c);

            pDataTmp++;

        }

		time_out_counter = 0;
        // make sure CEC rx is idle
        do
        {
            sp_read_reg(RX_P0,HDMI_RX_CEC_RX_STATUS_REG, &c);

			time_out_counter++;
			if(time_out_counter >= LOCAL_REG_TIME_OUT_THRESHOLD)
				break;

        }while(c&0x80);


        //clear tx done int first
        sp_read_reg(RX_P0,HDMI_RX_INT_STATUS7_REG, &c);
        sp_write_reg(RX_P0,HDMI_RX_INT_STATUS7_REG,(c|0x01));



        // switch to CEC tx mode
        sp_read_reg(RX_P0,RX_CEC_CTRL, &c);
        c&=0xf3;
        c|=0x04;
        sp_write_reg(RX_P0,RX_CEC_CTRL,c);

        time_out_counter = 0;
        do
        {

            sp_read_reg(RX_P0,HDMI_RX_INT_STATUS7_REG, &c);

			time_out_counter++;
			if(time_out_counter >= LOCAL_REG_TIME_OUT_THRESHOLD)
			{
				g_TimeOut = 1;
				break;
			}
        }while(!(c&0x01));
        //clear the CEC TX done interrupt
        c = 0x01;
        sp_write_reg(RX_P0,HDMI_RX_INT_STATUS7_REG,c);
        //SP_TX_AUX_DPCDWrite_Bytes(0x00, 0x05, 0x10,1, &c);

        //ANX7805_API_DPCDWriteByte(ANX7730_IRQ2, IRQ2Reg.all);

        //Get CEC TX status
        sp_read_reg(RX_P0,HDMI_RX_CEC_TX_STATUS_REG, &c);
        //pr_info("%s %s : 0x7E:0xD2 = %.2x\n",LOG_TAG, __func__,(uint)c);
        if((c&0x40)==0 && g_TimeOut == 0)
        {
            break;
        }
        else
        {
			#ifdef CEC_DBG_MSG_ENABLED
            //TRACE("send failed, reset CEC\n");
			#endif
            //reset CEC
            sp_read_reg(RX_P0,RX_CEC_CTRL, &c);
            c|=0x01;
            sp_write_reg(RX_P0,RX_CEC_CTRL,c);
	     g_TimeOut = 0;
        }
    }


    // switch to CEC rx mode
    sp_read_reg(RX_P0,RX_CEC_CTRL, &c);
    c&=0xf3;
    c|=0x08;
    sp_write_reg(RX_P0,RX_CEC_CTRL,c);

    if(i != 0)//send succeed
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

#ifdef CEC_PHYCISAL_ADDRESS_INSERT
void downstream_cec_phy_add_set(unchar addr0,unchar addr1)
{
	pr_info("%s %s : 00 set phy addr[0] = %.2x,phy addr[1] = %.2x,\n", LOG_TAG, __func__, (uint)addr0,(uint)addr1);
	downstream_physicaladdrh = addr0;
	downstream_physicaladdrl = addr1;
}
#endif


/*

bool downstream_cec_is_free(void)
{
    unchar c1,c2;

    sp_tx_aux_dpcdread_bytes(0x00,0x05,0x71,1,&c1);
    sp_tx_aux_dpcdread_bytes(0x00,0x05,0x72,1,&c2);

     if(((c1&0x80)==0)&&((c2&0x80)==0))//rx idle and tx idle
        return 1;
     else
        return 0;
}



void Downstream_CEC_ImageViewOn(void)
{
    g_CECFrame.header.init = g_LogicAddr;
    g_CECFrame.header.dest = 0;
    g_CECFrame.msg.raw[0] = CEC_OPCODE_IMAGE_VIEW_ON;
    downStream_hdmi_cec_writemessage(&g_CECFrame, 2);
}



void Downstream_Give_Physical_Addr(void)
{

	g_CECFrame.header.init = g_LogicAddr;
	g_CECFrame.header.dest = 0;
	g_CECFrame.msg.raw[0] = CEC_OPCODE_GIVE_PHYSICAL_ADDRESS;
	downStream_hdmi_cec_writemessage(&g_CECFrame, 4);

}



void Uptream_CEC_PING(void)
{
    g_CECFrame.header.init = 0;
    g_CECFrame.header.dest = 4;
    //g_CECFrame.msg.raw[0] = CEC_OPCODE_IMAGE_VIEW_ON;
    upstream_hdmi_cec_writemessage(&g_CECFrame, 1);
}


void CEC_PING(void)
{
    g_CECFrame.header.init = 4;
    g_CECFrame.header.dest = 0;
    //g_CECFrame.msg.raw[0] = CEC_OPCODE_IMAGE_VIEW_ON;
    downStream_hdmi_cec_writemessage(&g_CECFrame, 1);
}
*/



#ifdef CEC_DBG_MSG_ENABLED
void trace(const char code *format, ...)
{
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void TraceArray(unsigned char idata array[], unsigned char len)
{
    unsigned char i;

    for(i = 0; i < len; i++)
    {
        trace(" %02BX", array[i]);
    }
    trace("\r\n");
}
#endif


#if 0
void DownStream_CEC_ParseMessage(void)
{
	unchar c;

	//unsigned char msglength;

    switch(g_CECFrame.msg.raw[0])
    {

    case CEC_OPCODE_REPORT_PHYSICAL_ADDRESS:
		c = (unchar)(g_CECFrame.header.init);
		pr_info("%s %s : init address = %.2x\n",LOG_TAG, __func__,(uint)c);

		c = (unchar)(g_CECFrame.header.dest);
		pr_info("%s %s : dest address = %.2x\n",LOG_TAG, __func__,(uint)c);

		c = (unchar)(g_CECFrame.msg.raw[0]);
		pr_info("%s %s : message raw[0] = %.2x\n",LOG_TAG, __func__,(uint)c);

		c = (unchar)(g_CECFrame.msg.raw[1]);
		pr_info("%s %s : message raw[1] = %.2x\n",LOG_TAG, __func__,(uint)c);

		c = (unchar)(g_CECFrame.msg.raw[2]);
		pr_info("%s %s : message raw[2] = %.2x\n",LOG_TAG, __func__,(uint)c);

		c = (unchar)(g_CECFrame.msg.raw[3]);
		pr_info("%s %s : message raw[3] = %.2x\n",LOG_TAG, __func__,(uint)c);

		c = (unchar)(g_CECFrame.msg.raw[4]);
		pr_info("%s %s : message raw[4] = %.2x\n",LOG_TAG, __func__,(uint)c);

		c = (unchar)(g_CECFrame.msg.raw[5]);
		pr_info("%s %s : message raw[5] = %.2x\n",LOG_TAG, __func__,(uint)c);
       break;


    case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
        /*
            g_CECFrame.header.init = g_LogicAddr;
            g_CECFrame.header.dest = 0;
            g_CECFrame.msg.raw[0] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
            g_CECFrame.msg.raw[1] = 0x20;
            g_CECFrame.msg.raw[2] = 0x00;
            downStream_hdmi_cec_writemessage(&g_CECFrame, 4);*/
        //DownStream_BroadcastPhysicalAddress();
        break;
    case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
        g_CECFrame.header.init = g_LogicAddr;
        g_CECFrame.header.dest = CEC_DEVICE_BROADCAST;
        g_CECFrame.msg.raw[0] = CEC_OPCODE_ACTIVE_SOURCE;
        g_CECFrame.msg.raw[1] = 0x20;
        g_CECFrame.msg.raw[2] = 0x00;
        downStream_hdmi_cec_writemessage(&g_CECFrame, 4);
        break;
    case CEC_OPCODE_GIVE_DEVICE_POWER_STATUS:
        g_CECFrame.header.init = g_LogicAddr;
        g_CECFrame.header.dest = 0;
        g_CECFrame.msg.raw[0] = CEC_OPCODE_REPORT_POWER_STATUS;
        g_CECFrame.msg.raw[1] = 0x00;
        downStream_hdmi_cec_writemessage(&g_CECFrame, 3);
        break;
    case CEC_OPCODE_GIVE_DEVICE_VENDOR_ID:
        g_CECFrame.header.init = g_LogicAddr;
        g_CECFrame.header.dest = CEC_DEVICE_BROADCAST;
        g_CECFrame.msg.raw[0] = CEC_OPCODE_DEVICE_VENDOR_ID;
        g_CECFrame.msg.raw[1] = 0x00;
        g_CECFrame.msg.raw[2] = 0xE0;
        g_CECFrame.msg.raw[3] = 0x91;
        downStream_hdmi_cec_writemessage(&g_CECFrame, 5);
        break;
    case CEC_OPCODE_GIVE_DECK_STATUS:
        g_CECFrame.header.init = g_LogicAddr;
        g_CECFrame.header.dest = 0;
        g_CECFrame.msg.raw[0] = CEC_OPCODE_DECK_STATUS;
        g_CECFrame.msg.raw[1] = 0x11;
        downStream_hdmi_cec_writemessage(&g_CECFrame, 3);
        break;
    case CEC_OPCODE_DEVICE_VENDOR_ID:

        g_CECFrame.header.init = g_LogicAddr;
        g_CECFrame.header.dest = CEC_DEVICE_BROADCAST;
        g_CECFrame.msg.raw[0] = CEC_OPCODE_DEVICE_VENDOR_ID;
        g_CECFrame.msg.raw[1] = 0x00;
        g_CECFrame.msg.raw[2] = 0xE0;
        g_CECFrame.msg.raw[3] = 0x91;
        downStream_hdmi_cec_writemessage(&g_CECFrame, 5);

        break;
    case CEC_OPCODE_USER_CONTROL_PRESSED:
        //downStream_hdmi_cec_writemessage();
        break;
    case CEC_OPCODE_USER_CONTROL_RELEASE:
        break;

    default :
        //downStream_hdmi_cec_writemessage(pFrame, Len);
#ifdef CEC_DBG_MSG_ENABLED
        TRACE("default message!\r\n");
#endif
        break;
    }
}
#endif


#ifdef CEC_PHYCISAL_ADDRESS_INSERT
void uptream_cec_parsemessage(void)
{
	//unchar c;

	//unsigned char msglength;

    switch(g_CECFrame_HDMI.msg.raw[0])
    {

    case CEC_OPCODE_REPORT_PHYSICAL_ADDRESS:
		pr_info("%s %s : parse phy addr[0] = %.2x,phy addr[1] = %.2x,\n", LOG_TAG, __func__, (uint)downstream_physicaladdrh,(uint)downstream_physicaladdrl);
		if((downstream_physicaladdrh != 0xff)&&(downstream_physicaladdrl != 0xff)){
			g_CECFrame_HDMI.msg.raw[1] = downstream_physicaladdrh;
			g_CECFrame_HDMI.msg.raw[2] = downstream_physicaladdrl;
		}

       break;

    default :
        break;
    }
}

void Downstream_Report_Physical_Addr(void)
{

	g_CECFrame.header.init = g_LogicAddr;
	g_CECFrame.header.dest = 0x0F;
	g_CECFrame.msg.raw[0] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
	g_CECFrame_HDMI.msg.raw[1] = 0x20;
	g_CECFrame_HDMI.msg.raw[2] = 0x00;
	g_CECFrame_HDMI.msg.raw[3] = CEC_DEVICE_TYPE_PLAYBACK_DEVICE;


	downStream_hdmi_cec_writemessage(&g_CECFrame, 5);


}
#endif

#endif

