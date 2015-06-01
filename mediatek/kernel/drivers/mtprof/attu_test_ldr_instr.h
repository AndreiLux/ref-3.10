
static void test_instr_ldrd_only_pld(int printlog)
{
    unsigned long copy_size; 
    unsigned long flags; 
    unsigned long i,j,k, pld_dst;
    unsigned long long t1, t2, t_temp, avg, avg_t;
    unsigned long temp;
    unsigned long long result[10];
    unsigned long long result_t[10];
    char output_line[320];
    char buf[32];
    copy_size = 256;

    if(printlog == 1)
        printk(KERN_EMERG"\n\n\r == LDRD PLD only ===\n\r");
    i = 0;
    while(i< 256 + 16*4 -2){
        int line_offset = 0;
        memset(output_line, 0, 320);
/*
        if(i<128){
            copy_size = 512 + i*512; // inc 256 byte from 0~64 KBytes [128 time]
        }else if (i< 128 +16*4){
            copy_size = 1024*64 + (i-128)*1024*64; // inc 64Kbyte form 64KB~1MB [16*4 time]
        }
*/
        if(i<256){
            copy_size = 512 + i*512; // inc 512 byte from 0~128 KBytes [128 time]
        }else if (i< 256 +16*4 - 2){
            copy_size = 1024*128 + (i-256)*1024*64; // inc 64Kbyte form 128KB~4MB [16*4 time]
        }
        i++;
        mdelay(DELAY_MS);
        preempt_disable();
        local_irq_save(flags);
        inner_dcache_flush_all();
        if(printlog == 1){
            memset(buf, 0, 32);
            sprintf(buf," %lu :",copy_size);
            snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
            line_offset += strlen(buf);
        }

        avg =0;
        for(j = 0; j<8; j++){
            mdelay(DELAY_MS);
            if(flush_cache == 1)
                inner_dcache_flush_all();
#if 1
            /* enable ARM CPU PMU */
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                    "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "ORR %0, %0, #1 << 0\n"   /* enable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c12, 1\n"
                    "ORR %0, %0, #1 << 31\n"
                    "MCR p15, 0, %0, c9, c12, 1\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
#endif
            asm volatile(
                    "push {r0,r1,r2,r3}\n"
                    "mov r0, %0\n"
                    "mov r1, %1\n"
                    "mov r2, %2\n"
                    "mov r3, %3\n"
                    "stmfd        sp!, {r4, r5, r6, r7, r8, r9}\n" 
                    "1:\n" 
                    //"pldw        [r0, r9]\n" 
                    //".word 0xf790f009 \n"
                    "ldrd    r4, r5, [r1, #0]\n"
                    "strd    r4, r5, [r0, #0]\n"
                    "ldrd    r4, r5, [r1, #8]\n"
                    "strd    r4, r5, [r0, #8]\n"
                    "ldrd    r4, r5, [r1, #16]\n"
                    "strd    r4, r5, [r0, #16]\n"
                    "ldrd    r4, r5, [r1, #24]\n"
                    "strd    r4, r5, [r0, #24]\n"
                    "ldrd    r4, r5, [r1, #32]\n"
                    "strd    r4, r5, [r0, #32]\n"

                    "ldrd    r4, r5, [r1, #40]\n"
                    "ldrd    r6, r7, [r1, #48]\n"
                    "ldrd    r8, r9, [r1, #56]\n"
                    //"pld         [r1, r9]\n"
                    "strd    r4, r5, [r0, #40]\n"
                    "strd    r6, r7, [r0, #48]\n"
                    "strd    r8, r9, [r0, #56]\n"
                    " add     r0, r0, #64\n"
                    " add     r1, r1, #64\n"
                    " subs    r2, r2, #64\n"
                    " bge     1b\n"

                    "ldmfd       sp!, { r4, r5, r6, r7, r8, r9}\n"
                    "pop {r0,r1,r2,r3}\n"
                    :: "r"(&buffer_dst), "r"(&buffer_src), "r"(copy_size), "r"(pld_dst)
                    :"r0","r1","r2","r3"
                        );


            //#if defined(DEBUG_DRAMC_CALIB)
#if 1
            /* get CPU cycle count from the ARM CPU PMU */
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c13, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
#endif
            result[j] = temp;
            //  printk(KERN_CONT" %d ", temp);
        }
        avg = 0;
        for(j = 0; j<8; j++){
            avg += result[j];
        }
        avg = avg >>3;
        if(printlog == 1){
            //printk(KERN_CONT" %d ", avg );
            memset(buf, 0, 32);
            snprintf(buf,32, " %llu ",avg);
            snprintf(output_line+line_offset, 320 - line_offset, "%s %d",buf, flush_cache);
            line_offset += strlen(buf);
        }
        for(pld_dst = PLD_START_DST; pld_dst<CACHELINE_SIZE*PLD_END_DST; pld_dst+= CACHELINE_SIZE * PLD_DST_INC){
            avg = 0;
            for(j = 0; j<8; j++){
                mdelay(DELAY_MS);
                if(flush_cache == 1)
                    inner_dcache_flush_all();
                for(k=0; k<2;k++){
               //     if(k==0)
                //        inner_dcache_flush_all();
            
                /* enable ARM CPU PMU */
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "BIC %0, %0, #1 << 0\n"   /* disable */
                        "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                        "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "ORR %0, %0, #1 << 0\n"   /* enable */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        "MRC p15, 0, %0, c9, c12, 1\n"
                        "ORR %0, %0, #1 << 31\n"
                        "MCR p15, 0, %0, c9, c12, 1\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
                asm volatile(
                        "push {r0,r1,r2,r3}\n"
                        "mov r0, %0\n"
                        "mov r1, %1\n"
                        "mov r2, %2\n"
                        "mov r3, %3\n"
                        "stmfd        sp!, {r4, r5, r6, r7, r8, r9}\n" 
                        "1:\n" 
                        //"pldw        [r0, r9]\n" 
                        //".word 0xf790f009 \n"
                        "ldrd    r4, r5, [r1, #0]\n"
                        "strd    r4, r5, [r0, #0]\n"
                        "ldrd    r4, r5, [r1, #8]\n"
                        "strd    r4, r5, [r0, #8]\n"
                        "ldrd    r4, r5, [r1, #16]\n"
                        "strd    r4, r5, [r0, #16]\n"
                        "ldrd    r4, r5, [r1, #24]\n"
                        "strd    r4, r5, [r0, #24]\n"
                        "ldrd    r4, r5, [r1, #32]\n"
                        "strd    r4, r5, [r0, #32]\n"
                        "ldrd    r4, r5, [r1, #40]\n"
                        "ldrd    r6, r7, [r1, #48]\n"
                        "ldrd    r8, r9, [r1, #56]\n"
                        "pld         [r1, r9]\n"
                        "strd    r4, r5, [r0, #40]\n"
                        "strd    r6, r7, [r0, #48]\n"
                        "strd    r8, r9, [r0, #56]\n"
                        " add     r0, r0, #64\n"
                        " add     r1, r1, #64\n"
                        " subs    r2, r2, #64\n"
                        " bge     1b\n"

                        "ldmfd       sp!, {r4, r5, r6, r7, r8, r9 }\n"
                        "pop {r0,r1,r2,r3}\n"
                        :: "r"(&buffer_dst), "r"(&buffer_src), "r"(copy_size), "r"(pld_dst)
                        :"r0","r1","r2","r3"
                            );

                /* get CPU cycle count from the ARM CPU PMU */
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "BIC %0, %0, #1 << 0\n"   /* disable */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        "MRC p15, 0, %0, c9, c13, 0\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
                result[j] = temp;
                } // for k < 2
                //  printk(KERN_CONT" %d ", temp);
            }
            avg =0;
            for(j = 0; j<8; j++){
                avg += result[j];
            }
            avg = avg >>3;
            if(printlog == 1){
                //printk(KERN_EMERG" %d ", avg );
                memset(buf, 0, 32);
                snprintf(buf,32, " %llu ",avg);
            //    snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
                snprintf(output_line+line_offset, 320 - line_offset, "%s %d",buf, flush_cache);
                line_offset += strlen(buf);
            }
        }
        if(printlog == 1)
            printk(KERN_EMERG"%s\n", output_line);
        local_irq_restore(flags); 
        preempt_enable();
    }
    if(printlog == 1)
        printk(KERN_EMERG"\n\r ==== test done only pld ==== flush_cache:%d \n", flush_cache);
}
static void test_instr_ldr_pld_pldw(int printlog)
{
    unsigned long copy_size;
    unsigned long flags;
    int i,j, avg, pld_dst;
    int k;
    int temp;
    int result[10];
    copy_size = 1024*64;
    if(printlog == 1)
        printk(KERN_EMERG"\n\n\r == Start test pld+pldw ===\n\r");
    i = 0;
    while(i< 256 + 16 + 4){
        if(i<256){
            copy_size = 256 + i*256; // inc 256 byte from 0~64 KBytes
        }else if (i< 256 +16){
            copy_size = 1024*64 + (i-256)*1024*64; // inc 64Kbyte form 64KB~1MB
        }else if (i< 256 +16 + 4){
            copy_size = 1024*1024 + (i-256-16)*1024*1024; //inc 1MB from 1MB~4MB
        }
        i++;
        mdelay(5);
        preempt_disable();
        local_irq_save(flags); 
        //printk(KERN_EMERG"\n\r %lu ",copy_size);
        inner_dcache_flush_all();
        if(printlog == 1)
            printk(KERN_EMERG" %lu :",copy_size);
        avg =0;
        for(j = 0; j<8; j++){
            mdelay(3);
            if(flush_cache == 1)
                inner_dcache_flush_all();
#if 1
            /* enable ARM CPU PMU */
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                    "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "ORR %0, %0, #1 << 0\n"   /* enable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c12, 1\n"
                    "ORR %0, %0, #1 << 31\n"
                    "MCR p15, 0, %0, c9, c12, 1\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
#endif
            asm volatile(
                    "push {r0,r1,r2,r9}\n"
                    "mov r0, %0\n"
                    "mov r1, %1\n"
                    "mov r2, %2\n"
                    "mov r9, %3\n"
                    "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 

                    "1:\n" 
                    //"pld         [r1, r9]\n"
                    //"pldw        [r0, r9]\n" 
                    //".word 0xf790f009 \n"
                    "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                    "subs        r2, r2, #32  \n"
                    "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                    "blt 2f\n"

                    "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                    "subs        r2, r2, #32  \n"
                    "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                    "bge 1b\n"
                    "2:\n" 

                    "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                    "pop {r0,r1,r2,r9}\n"
                    :: "r"(&buffer_dst), "r"(&buffer_src), "r"(copy_size), "r"(pld_dst)
                    :
                        );


            //#if defined(DEBUG_DRAMC_CALIB)
#if 1
            /* get CPU cycle count from the ARM CPU PMU */
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c13, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
#endif
            result[j] = temp;
            //  printk(KERN_CONT" %d ", temp);
        }
        avg = 0;
        for(j = 0; j<8; j++){
            avg += result[j];
        }
        avg = avg >>3;
        if(printlog == 1)
            printk(KERN_CONT" %d ", avg );
        for(pld_dst = PLD_START_DST; pld_dst<CACHELINE_SIZE*PLD_END_DST; pld_dst+= CACHELINE_SIZE * PLD_DST_INC){
            //for(pld_dst = 0; pld_dst<64*16*3; pld_dst+=64*3){
            for(j = 0; j<8; j++){
                mdelay(3);
                if(flush_cache == 1)
                    inner_dcache_flush_all();
#if 1
#if 0
                /* reset RA mode*/
                asm volatile(
                        "MRC p15, 0, %0, c1, c0, 1\n"
                        "ORR %0, %0, #1 << 11\n"   /* disable */
                        "ORR %0, %0, #1 << 12\n"   /* disable */
                        "MCR p15, 0, %0, c1, c0, 1\n"

                        "MRC p15, 0, %0, c1, c0, 1\n"
                        "BIC %0, %0, #1 << 11\n"   /* enable */
                        "BIC %0, %0, #1 << 12\n"   /* enable */
                        "MCR p15, 0, %0, c1, c0, 1\n"

                        : "+r"(temp)
                        :
                        : "cc"
                        );
#endif            
                /* enable ARM CPU PMU */
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "BIC %0, %0, #1 << 0\n"   /* disable */
                        "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                        "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "ORR %0, %0, #1 << 0\n"   /* enable */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        "MRC p15, 0, %0, c9, c12, 1\n"
                        "ORR %0, %0, #1 << 31\n"
                        "MCR p15, 0, %0, c9, c12, 1\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
#endif
                asm volatile(
                        "push {r0,r1,r2,r9}\n"
                        "mov r0, %0\n"
                        "mov r1, %1\n"
                        "mov r2, %2\n"
                        "mov r9, %3\n"
                        "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 

                        "1:\n" 
                        "pld         [r1, r9]\n"
                        //"pldw        [r0, r9]\n" 
                        ".word 0xf790f009 \n"
                        "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                        "subs        r2, r2, #32  \n" "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                        "blt 2f\n"

                        "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                        "subs        r2, r2, #32  \n"
                        "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                        "bge 1b\n"
                        "2:\n" 

                        "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                        "pop {r0,r1,r2,r9}\n"
                        :: "r"(&buffer_dst), "r"(&buffer_src), "r"(copy_size), "r"(pld_dst)
                        :
                            );


                //#if defined(DEBUG_DRAMC_CALIB)
#if 1
                /* get CPU cycle count from the ARM CPU PMU */
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "BIC %0, %0, #1 << 0\n"   /* disable */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        "MRC p15, 0, %0, c9, c13, 0\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
#endif
                result[j] = temp;
                //  printk(KERN_CONT" %d ", temp);
            }
            avg =0;
            for(j = 0; j<8; j++){
                avg += result[j];
            }
            avg = avg >>3;
            if(printlog == 1)
                printk(KERN_CONT" %d ", avg );
        }
        local_irq_restore(flags); 
        preempt_enable();
    }
    if(printlog == 1)
        printk(KERN_EMERG"\n\r ==== test done pld+pldw==== flush_cache:%d \n", flush_cache);
}
